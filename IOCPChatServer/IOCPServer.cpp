#include <iostream>
#include "IOCPServer.h"

IOCPServer::IOCPServer()
{
    server_socket_ = INVALID_SOCKET;
    iocp_handle_ = INVALID_HANDLE_VALUE;
}

IOCPServer::~IOCPServer()
{
    WSACleanup();
}

ErrorCode IOCPServer::InitSocket(unsigned short port)
{
    WSADATA wsa_data;
    SOCKADDR_IN server_addr;

    if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return ErrorCode::WSA_STARTUP_FAIL;
    }

    if((server_socket_ = WSASocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
    {
        return ErrorCode::WSA_SOCKET_FAIL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if(bind(server_socket_, (SOCKADDR *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        return ErrorCode::BIND_FAIL;
    }

    if(listen(server_socket_, BACKLOG) == SOCKET_ERROR)
    {
        return ErrorCode::LISTEN_FAIL;
    }

    return ErrorCode::NONE;
}

ErrorCode IOCPServer::StartServer()
{
    // Initialize client infos
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        ClientInfo client_info;
        client_info.client_socket = INVALID_SOCKET;
        client_info.client_index = i;
        client_info.sending = false;
        client_info.connecting = false;
        client_infos_.push_back(client_info);
    }

    // Create IOCP
    if((iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKER_THREADS)) == NULL)
    {
        return ErrorCode::CREATE_IOCP_FAIL;
    }

    // Connect listen socket to IOCP
    auto iocp_handle = CreateIoCompletionPort((HANDLE) server_socket_, iocp_handle_, 0, 0);

    if(iocp_handle == NULL || iocp_handle != iocp_handle_)
    {
        closesocket(server_socket_);
        return ErrorCode::CONNECT_IOCP_FAIL;
    }

    // Create worker threads
    for(int i = 0; i < MAX_WORKER_THREADS; ++i)
    {
        worker_threads_.emplace_back([this]() { WorkerThread(); });
    }

    // Create send thread
    send_thread_ = boost::thread([this]() { SendThread(); });

    std::cout << "Server start..." << std::endl;

    while(1)
    {
        // Get idle client info
        auto client_info = GetIdleClientInfo();

        if(client_info == nullptr)
        {
            continue;
        }

        // Create client socket
        if((client_info->client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
        {
            return ErrorCode::WSA_SOCKET_FAIL;
        }

        // Call asynchronous accept
        if(AsyncAccept(client_info) == ErrorCode::ACCEPT_FAIL)
        {
            closesocket(client_info->client_socket);
            return ErrorCode::ACCEPT_FAIL;
        }

        std::cout << "Ready to accept clients..." << std::endl;
    }

    return ErrorCode::NONE;
}

ClientInfo * IOCPServer::GetIdleClientInfo()
{
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(!client_infos_.at(i).connecting)
        {
            return &client_infos_.at(i);
        }
    }

    return nullptr;
}

ErrorCode IOCPServer::AsyncAccept(ClientInfo * client_info)
{
    memset(&client_info->accept_overlapped_data.overlapped, 0, sizeof(WSAOVERLAPPED));
    client_info->accept_overlapped_data.client_info = client_info;
    client_info->accept_overlapped_data.io_operation = IOOperation::ACCEPT;

    if (!AcceptEx(server_socket_,
                  client_info->client_socket,
                  (PVOID) &client_info->accept_overlapped_data.buf,
                  0,
                  sizeof(SOCKADDR_IN) + 16,
                  sizeof(SOCKADDR_IN) + 16,
                  NULL,
                  (LPOVERLAPPED) &client_info->accept_overlapped_data)
        && WSAGetLastError() != WSA_IO_PENDING)
    {
        return ErrorCode::ACCEPT_FAIL;
    }

    client_info->connecting = true;

    return ErrorCode::NONE;
}

ErrorCode IOCPServer::AsyncReceive(ClientInfo * client_info)
{
    DWORD recv_bytes = 0;
    DWORD flags = 0;

    memset(&client_info->recv_overlapped_data.overlapped, 0, sizeof(WSAOVERLAPPED));
    client_info->recv_overlapped_data.wsabuf.len = MAX_BUFFER;
    client_info->recv_overlapped_data.wsabuf.buf = client_info->recv_overlapped_data.buf;
    client_info->recv_overlapped_data.io_operation = IOOperation::RECV;

    if(WSARecv(client_info->client_socket, &(client_info->recv_overlapped_data.wsabuf), 1, &recv_bytes, &flags, (LPWSAOVERLAPPED) &(client_info->recv_overlapped_data), NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        return ErrorCode::WSA_RECV_FAIL;
    }

    return ErrorCode::NONE;
}

void IOCPServer::AsyncSend(ClientInfo * client_info, char * buf, int size)
{
    OverlappedData * send_overlapped_data = new OverlappedData;

    memset(send_overlapped_data, 0, sizeof(OverlappedData));
    memcpy(send_overlapped_data->buf, buf, size);
    send_overlapped_data->wsabuf.len = size;
    send_overlapped_data->wsabuf.buf = send_overlapped_data->buf;
    send_overlapped_data->io_operation = IOOperation::SEND;

    boost::unique_lock<boost::mutex> lock(mutexes_[client_info->client_index]);
    if(client_info->send_queue.size() < MAX_SEND_QUEUE_LENGTH)
    {
        client_info->send_queue.push_back(send_overlapped_data);
        lock.unlock();
    }
    else
    {
        CloseSocket(client_info);
    }
}

void IOCPServer::AsyncSend(int client_index, char * buf, int size)
{
    AsyncSend(&client_infos_.at(client_index), buf, size);
}

void IOCPServer::WorkerThread()
{
    ClientInfo * client_info = nullptr;
    OverlappedData * overlapped_data = nullptr;
    DWORD bytes;

    while(1)
    {
        if(!GetQueuedCompletionStatus(iocp_handle_, &bytes, (PULONG_PTR) &client_info, (LPOVERLAPPED *) &overlapped_data, INFINITE))
        {
            if(overlapped_data->io_operation == IOOperation::SEND)
            {
                delete overlapped_data;
            }

            CloseSocket(client_info);
            continue;
        }

        switch(overlapped_data->io_operation)
        {
            case IOOperation::ACCEPT:
                ProcessAccept(overlapped_data->client_info);
                break;
            case IOOperation::RECV:
                ProcessReceive(client_info, bytes);
                break;
            case IOOperation::SEND:
                ProcessSend(client_info);
                delete overlapped_data;
                break;
            default:
                std::cout << "Error: invalid IO operation" << std::endl;
        }
    }
}

void IOCPServer::SendThread()
{
    while(1)
    {
        for(int i = 0; i < MAX_CLIENTS; ++i)
        {
            if(client_infos_[i].client_socket != INVALID_SOCKET)
            {
                boost::unique_lock<boost::mutex> lock(mutexes_[client_infos_[i].client_index]);
                if(!client_infos_[i].send_queue.empty() && !client_infos_[i].sending)
                {
                    auto send_overlapped_data = client_infos_[i].send_queue.front();

                    WSASend(client_infos_[i].client_socket, &send_overlapped_data->wsabuf, 1, NULL, 0, (LPWSAOVERLAPPED) send_overlapped_data, NULL);
                    client_infos_[i].sending = true;
                }
                lock.unlock();
            }
        }

        Sleep(10);
    }
}

void IOCPServer::CloseSocket(ClientInfo * client_info)
{
    std::cout << "Closing client socket..." << std::endl;

    OnClose(client_info->client_index);

    boost::unique_lock<boost::mutex> lock(mutexes_[client_info->client_index]);
    for(int i = 0; i < client_info->send_queue.size(); ++i)
    {
        delete client_info->send_queue[i];
    }
    lock.unlock();

    shutdown(client_info->client_socket, SD_BOTH);
    closesocket(client_info->client_socket);

    client_info->sending = false;
    client_info->connecting = false;
}

void IOCPServer::ProcessAccept(ClientInfo * client_info)
{
    std::cout << "Accept client..." << std::endl;

    OnAccept(client_info->client_index);

    // Connect client socket to IOCP
    auto iocp_handle = CreateIoCompletionPort((HANDLE) client_info->client_socket, iocp_handle_, (ULONG_PTR) client_info, 0);

    if(iocp_handle == NULL || iocp_handle != iocp_handle_)
    {
        CloseSocket(client_info);
        return;
    }

    AsyncReceive(client_info);
}

void IOCPServer::ProcessReceive(ClientInfo * client_info, int bytes)
{
    if(bytes == 0)
    {
        CloseSocket(client_info);
        return;
    }

    char * buf = new char[bytes];
    memcpy(buf, client_info->recv_overlapped_data.buf, bytes);

    OnReceive(client_info->client_index, buf, bytes);

    AsyncReceive(client_info);
}

void IOCPServer::ProcessSend(ClientInfo * client_info)
{
    std::cout << "Asynchronous send complete" << std::endl;

    boost::unique_lock<boost::mutex> lock(mutexes_[client_info->client_index]);
    client_info->send_queue.pop_front();
    client_info->sending = false;
    lock.unlock();
}
