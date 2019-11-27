#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include <WinSock2.h>
#include <mswsock.h>
#include <vector>
#include <deque>
#include <boost\thread.hpp>

#include "Config.h"
#include "ErrorCode.h"

const int BACKLOG = 5;
const unsigned int MAX_WORKER_THREADS = 4;

enum IOOperation { ACCEPT, RECV, SEND };

struct ClientInfo;

struct OverlappedData
{
    WSAOVERLAPPED overlapped;
    WSABUF wsabuf;
    char buf[MAX_BUFFER];
    IOOperation io_operation;
    ClientInfo * client_info;
};

struct ClientInfo
{
    SOCKET client_socket;
    SOCKADDR_IN client_addr;
    OverlappedData accept_overlapped_data;
    OverlappedData recv_overlapped_data;
    std::deque<OverlappedData *> send_queue;
    int client_index;
    bool sending;
    bool connecting;
};

class IOCPServer
{
public:
    IOCPServer();
    ~IOCPServer();

protected:
    ErrorCode InitSocket(unsigned short port);
    ErrorCode StartServer();

    virtual void OnAccept(int client_index) {};
    virtual void OnClose(int client_index) {};
    virtual void OnReceive(int client_index, char * buf, int size) {};

    void AsyncSend(int client_index, char * buf, int size);

private:
    ClientInfo * GetIdleClientInfo();

    ErrorCode AsyncAccept(ClientInfo * client_info);
    ErrorCode AsyncReceive(ClientInfo * client_info);
    void AsyncSend(ClientInfo * client_info, char * buf, int size);

    void WorkerThread();
    void SendThread();
    void CloseSocket(ClientInfo * client_info);

    void ProcessAccept(ClientInfo * client_info);
    void ProcessReceive(ClientInfo * client_info, int bytes);
    void ProcessSend(ClientInfo * client_info);

    SOCKET server_socket_;
    HANDLE iocp_handle_;

    std::vector<ClientInfo> client_infos_;
    std::vector<boost::thread> worker_threads_;
    boost::thread send_thread_;
    boost::mutex mutexes_[MAX_CLIENTS];
};
