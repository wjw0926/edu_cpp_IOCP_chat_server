#include <iostream>
#include "PacketManager.h"

void PacketManager::Init()
{
    user_manager_.Init();

    FuncList_[static_cast<int>(PacketID::SYS_ACCEPT)] = &PacketManager::FuncAccept;
    FuncList_[static_cast<int>(PacketID::SYS_CLOSE)] = &PacketManager::FuncClose;
    FuncList_[static_cast<int>(PacketID::ECHO_REQ)] = &PacketManager::FuncEcho;
}

void PacketManager::Start()
{
    thread_ = boost::thread([this]() -> void { ProcessPacket(); });
}

void PacketManager::Stop()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void PacketManager::ProcessPacket()
{
    while(1) {
        boost::unique_lock<boost::mutex> lock(mutex_);
        if (!packet_queue_.empty()) {
            PacketInfo &packet_info = packet_queue_.front();
            packet_queue_.pop_front();
            lock.unlock();

            if (static_cast<int>(packet_info.packet_id) > MAX_PACKET_ID) {
                continue;
            }

            if (FuncList_[static_cast<int>(packet_info.packet_id)] == nullptr) {
                continue;
            }

            (this->*FuncList_[static_cast<int>(packet_info.packet_id)])(packet_info);
        }
    }
}

void PacketManager::AddPacketQueue(PacketInfo packet_info)
{
    boost::unique_lock<boost::mutex> lock(mutex_);
    packet_queue_.emplace_back(packet_info);
    lock.unlock();
}

void PacketManager::AddPacketQueue(int client_index, PacketID packet_id, unsigned short size, char * buf)
{
    PacketInfo packet_info = { client_index, packet_id, size, buf };
    AddPacketQueue(packet_info);
}

void PacketManager::FuncAccept(PacketInfo packet_info) {
    user_manager_.AddUser(packet_info.client_index);
}

void PacketManager::FuncClose(PacketInfo packet_info) {
    auto user = user_manager_.GetUser(packet_info.client_index);

    switch (user->GetState()) {
        case User::State::NONE:
            std::cout << "SERVER: fatal error: call close from unconnected client" << std::endl;
            break;
        case User::State::CONNECT:
            user_manager_.RemoveUser(packet_info.client_index);
            break;
    }
}

void PacketManager::FuncEcho(PacketInfo packet_info) {
    std::cout << "Echoing..." << std::endl;

    AsyncSend(packet_info.client_index, packet_info.buf, packet_info.size);
}
