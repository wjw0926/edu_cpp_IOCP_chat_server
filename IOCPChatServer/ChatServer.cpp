#include <iostream>

#include "ChatServer.h"

void ChatServer::InitServer(unsigned short port)
{
    if((error_code_ = IOCPServer::InitSocket(port)) != ErrorCode::NONE)
    {
        std::cout << "Error: " << (unsigned short) error_code_ << std::endl;
    }

    packet_manager_.Init();

    auto async_send_func = [&](int client_index, char * buf, int size) { AsyncSend(client_index, buf, size); };
    packet_manager_.AsyncSend = async_send_func;
}

void ChatServer::StartServer()
{
    packet_manager_.Start();

    if((error_code_ = IOCPServer::StartServer()) != ErrorCode::NONE)
    {
        std::cout << "Error: " << (unsigned short) error_code_ << std::endl;
    }
}

void ChatServer::StopServer() {
    packet_manager_.Stop();
}

void ChatServer::OnAccept(int client_index)
{
    packet_manager_.AddPacketQueue(client_index, PacketID::SYS_ACCEPT, 0, nullptr);
}

void ChatServer::OnClose(int client_index)
{
    packet_manager_.AddPacketQueue(client_index, PacketID::SYS_CLOSE, 0, nullptr);
}

void ChatServer::OnReceive(int client_index, char * buf, int size)
{
    auto header = reinterpret_cast<PacketHeader *>(buf);

    if (header->total_size != size) {
        delete buf;
        return;
    }

    PacketID packet_id = header->packet_id;

    char * body = new char[size - PACKET_HEADER_SIZE];
    memcpy(body, &buf[PACKET_HEADER_SIZE], size - PACKET_HEADER_SIZE);

    delete buf;

    packet_manager_.AddPacketQueue(client_index, packet_id, size - PACKET_HEADER_SIZE, body);

    // For echo
    //packet_manager_.AddPacketQueue(client_index, PacketID::ECHO_REQ, size, buf);
}
