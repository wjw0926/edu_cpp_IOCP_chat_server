#pragma once

#include "IOCPServer.h"
#include "PacketManager.h"

class ChatServer : IOCPServer
{
public:
    ChatServer() {};
    ~ChatServer() {};

    void InitServer(unsigned short port);
    void StartServer();
    void StopServer();

private:
    void OnAccept(int client_index) override;
    void OnClose(int client_index) override;
    void OnReceive(int client_index, char * buf, int size) override;

    ErrorCode error_code_;
    PacketManager packet_manager_;
};
