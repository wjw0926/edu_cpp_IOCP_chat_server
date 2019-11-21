#include <iostream>
#include "IOCPServer.h"

const unsigned short SERVER_PORT = 11021;

int main() {
	IOCPServer iocp_server;
	ErrorCode error_code;

	if((error_code = iocp_server.InitSocket(SERVER_PORT)) != ErrorCode::NONE)
	{
		std::cout << "Error: " << (unsigned short) error_code << ", " << WSAGetLastError() << std::endl;
	}

	if((error_code = iocp_server.StartServer()) != ErrorCode::NONE)
	{
		std::cout << "Error: " << (unsigned short) error_code << ", " << WSAGetLastError() << std::endl;
	}

	return 0;
}
