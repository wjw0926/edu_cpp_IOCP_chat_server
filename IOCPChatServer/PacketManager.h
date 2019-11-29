#pragma once

#include <deque>
#include <boost\thread.hpp>

#include "Config.h"
#include "UserManager.h"
#include "RoomManager.h"

class PacketManager
{
public:
    PacketManager() {};
    ~PacketManager() {};

    void Init();
    void Start();
    void Stop();

    void AddPacketQueue(PacketInfo packet_info);
    void AddPacketQueue(int client_index, PacketID packet_id, unsigned short size, char * buf);

    std::function<void(int client_index, char * buf, int size)> AsyncSend;

private:
    void ProcessPacket();

    void FuncAccept(PacketInfo packet_info);
    void FuncClose(PacketInfo packet_info);
    void FuncEcho(PacketInfo packet_info);
    void FuncLogin(PacketInfo packet_info);
    void FuncEnterRoom(PacketInfo packet_info);
    void FuncLeaveRoom(PacketInfo packet_info);
    void FuncChatRoom(PacketInfo packet_info);

    void (PacketManager::*FuncList_[MAX_PACKET_ID])(PacketInfo packet_info);

    boost::thread thread_;
    boost::mutex mutex_;
    std::deque<PacketInfo> packet_queue_;

    UserManager user_manager_;
    RoomManager room_manager_;
};
