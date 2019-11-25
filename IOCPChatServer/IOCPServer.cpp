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
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        ClientInfo client_info;
        client_info.client_socket = INVALID_SOCKET;
        client_info.client_index = i;
        client_infos_.push_back(client_info);
    }

    if((iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKER_THREADS)) == NULL)
    {
        return ErrorCode::CREATE_IOCP_FAIL;
    }

    for(int i = 0; i < MAX_WORKER_THREADS; ++i)
    {
        worker_threads_.emplace_back([this]() { WorkerThread(); });
    }

    std::cout << "Server start..." << std::endl;

    while(1)
    {
        auto client_info = GetVacantClientInfo();

        if(client_info == nullptr)
        {
            return ErrorCode::FULL_CLIENTS;
        }

        int client_addr_len = sizeof(client_info->client_addr);

        if((client_info->client_socket = accept(server_socket_, (SOCKADDR *) &client_info->client_addr, &client_addr_len)) == INVALID_SOCKET)
        {
            return ErrorCode::ACCPET_FAIL;
        }

        auto iocp_handle = CreateIoCompletionPort((HANDLE) client_info->client_socket, iocp_handle_, (ULONG_PTR) client_info, 0);

        if(iocp_handle == NULL || iocp_handle != iocp_handle_)
        {
            return ErrorCode::CONNECT_IOCP_FAIL;
        }

        std::cout << "Accept client..." << std::endl;

        OnAccept(client_info->client_index);

        if(AsyncRecv(client_info) == ErrorCode::WSA_RECV_FAIL)
        {
            return ErrorCode::WSA_RECV_FAIL;
        }
    }

    return ErrorCode::NONE;
}

ClientInfo * IOCPServer::GetVacantClientInfo()
{
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(client_infos_.at(i).client_socket == INVALID_SOCKET)
        {
            return &client_infos_.at(i);
        }
    }

    return nullptr;
}

ErrorCode IOCPServer::AsyncRecv(ClientInfo * client_info)
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

ErrorCode IOCPServer::AsyncSend(ClientInfo * client_info, char * buf, int size)
{
    OverlappedData * send_overlapped_data = new OverlappedData;

    memset(send_overlapped_data, 0, sizeof(OverlappedData));
    memcpy(send_overlapped_data->buf, buf, size);
    send_overlapped_data->wsabuf.len = size;
    send_overlapped_data->wsabuf.buf = send_overlapped_data->buf;
    send_overlapped_data->io_operation = IOOperation::SEND;
        
    if(WSASend(client_info->client_socket, &send_overlapped_data->wsabuf, 1, NULL, 0, (LPWSAOVERLAPPED) send_overlapped_data, NULL) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        return ErrorCode::WSA_SEND_FAIL;
    }

    return ErrorCode::NONE;
}

ErrorCode IOCPServer::AsyncSend(int client_index, char * buf, int size)
{
    auto client_info = client_infos_.at(client_index);

    return AsyncSend(&client_info, buf, size);
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
            CloseSocket(client_info);
            continue;
        }

        if(overlapped_data->io_operation == IOOperation::RECV)
        {
            if(bytes == 0)
            {
                CloseSocket(client_info);
                continue;
            }

            OnReceive(client_info->client_index, client_info->recv_overlapped_data.buf, bytes);

            if(AsyncRecv(client_info) == ErrorCode::WSA_RECV_FAIL)
            {
                continue;
            }

        }
        else if (overlapped_data->io_operation == IOOperation::SEND)
        {
            std::cout << "Asynchronous send complete" << std::endl;
            delete overlapped_data;
        }
        else
        {
            std::cout << "Error: invalid IO operation" << std::endl;
        }
    }
}

void IOCPServer::CloseSocket(ClientInfo * client_info)
{
    std::cout << "Closing client socket..." << std::endl;

    OnClose(client_info->client_index);

    shutdown(client_info->client_socket, SD_BOTH);
    closesocket(client_info->client_socket);
    client_info->client_socket = INVALID_SOCKET;
}
