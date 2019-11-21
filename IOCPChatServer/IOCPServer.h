#pragma once
#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <vector>
#include <boost\thread.hpp>
#include "ErrorCode.h"

const int BACKLOG = 5;
const unsigned int MAX_CLIENTS = 100;
const unsigned int MAX_BUFFER = 1024;
const unsigned int MAX_WORKER_THREADS = 4;

enum IOOperation { RECV, SEND };

struct OverlappedData
{
	WSAOVERLAPPED overlapped;
	WSABUF wsabuf;
	char buf[MAX_BUFFER];
	IOOperation io_operation;
};

struct ClientInfo
{
	SOCKET client_socket;
	SOCKADDR_IN client_addr;
	OverlappedData recv_overlapped_data;
	OverlappedData send_overlapped_data;
};

class IOCPServer
{
public:
	IOCPServer();
	~IOCPServer();

	ErrorCode InitSocket(unsigned short port);
	ErrorCode StartServer();

private:
	ClientInfo * GetVacantClientInfo();

	ErrorCode AsyncRecv(ClientInfo * client_info);
	ErrorCode AsyncSend(ClientInfo * client_info, char * buf, int size);

	void WorkerThread();
	void CloseSocket(ClientInfo * client_info);

	SOCKET server_socket_;
	HANDLE iocp_handle_;

	std::vector<ClientInfo> client_infos_;
	std::vector<boost::thread> worker_threads_;
};