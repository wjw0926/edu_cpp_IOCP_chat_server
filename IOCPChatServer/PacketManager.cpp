#include <iostream>
#include "PacketManager.h"
#include "ErrorCode.h"

void PacketManager::Init()
{
    user_manager_.Init();
    room_manager_.Init();

    auto async_send_func = [&](int client_index, char * buf, int size) { AsyncSend(client_index, buf, size); };
    room_manager_.AsyncSend = async_send_func;

    FuncList_[static_cast<int>(PacketID::SYS_ACCEPT)] = &PacketManager::FuncAccept;
    FuncList_[static_cast<int>(PacketID::SYS_CLOSE)] = &PacketManager::FuncClose;
    FuncList_[static_cast<int>(PacketID::ECHO_REQ)] = &PacketManager::FuncEcho;
    FuncList_[static_cast<int>(PacketID::LOGIN_REQ)] = &PacketManager::FuncLogin;
    FuncList_[static_cast<int>(PacketID::ROOM_ENTER_REQ)] = &PacketManager::FuncEnterRoom;
    FuncList_[static_cast<int>(PacketID::ROOM_LEAVE_REQ)] = &PacketManager::FuncLeaveRoom;
    FuncList_[static_cast<int>(PacketID::ROOM_CHAT_REQ)] = &PacketManager::FuncChatRoom;
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

            delete packet_info.buf;
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
        case User::State::LOGIN:
            user_manager_.RemoveUser(packet_info.client_index);
            break;
        case User::State::ROOM:
            room_manager_.LeaveRoom(*user);
            user_manager_.RemoveUser(packet_info.client_index);
            break;
    }
}

void PacketManager::FuncEcho(PacketInfo packet_info) {
    std::cout << "Echoing..." << std::endl;

    AsyncSend(packet_info.client_index, packet_info.buf, packet_info.size);
}

void PacketManager::FuncLogin(PacketInfo packet_info) {
    auto req = reinterpret_cast<PacketLoginReq *>(packet_info.buf);
    std::cout << "SERVER: Received login request: " << req->user_id << ", " << req->user_pw << std::endl;

    PacketLoginRes res;
    res.packet_id = PacketID::LOGIN_RES;
    res.total_size = sizeof(PacketLoginRes);

    auto user = user_manager_.GetUser(packet_info.client_index);

    if (user == nullptr) {
        res.code = ErrorCode::INVALID_CLIENT_INDEX;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLoginRes));
        return;
    }

    if (user->GetState() == User::State::NONE) {
        res.code = ErrorCode::USER_NOT_CONNECTED;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLoginRes));
        return;
    }

    if (user->GetState() == User::State::LOGIN) {
        res.code = ErrorCode::USER_ALREADY_LOGIN;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLoginRes));
        return;
    }

    user->SetState(User::State::LOGIN);
    user->SetUserID(req->user_id);

    res.code = ErrorCode::NONE;

    std::cout << "SERVER: Sending login response..." << std::endl;

    AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLoginRes));
}

void PacketManager::FuncEnterRoom(PacketInfo packet_info) {
    // Prepare response packet
    PacketEnterRoomRes res;
    res.packet_id = PacketID::ROOM_ENTER_RES;
    res.total_size = sizeof(PacketEnterRoomRes);

    // Check user is log-inned
    auto user = user_manager_.GetUser(packet_info.client_index);

    if (user->GetState() != User::State::LOGIN) {
        res.code = ErrorCode::ROOM_ENTER_USER_NOT_LOGIN;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketEnterRoomRes));
        return;
    }

    // Check room number is valid
    auto req = reinterpret_cast<PacketEnterRoomReq *>(packet_info.buf);

    if (req->room_number < 0 || req->room_number > MAX_CREATED_ROOMS) {
        res.code = ErrorCode::ROOM_ENTER_INVALID_ROOM_INDEX;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketEnterRoomRes));
        return;
    }

    // Check room is not full to enter
    auto room = room_manager_.GetRoom(req->room_number);

    if (room->IsFull()) {
        res.code = ErrorCode::ROOM_ENTER_ROOM_IS_FULL;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketEnterRoomRes));
        return;
    }

    // Normal execution: enter room
    room_manager_.EnterRoom(*user, room->GetIndex());
    res.code = ErrorCode::NONE;
    AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketEnterRoomRes));

    // Notify users in the room about new user
    PacketEnterRoomNtf ntf;
    ntf.packet_id = PacketID::ROOM_ENTER_NTF;
    ntf.total_size = sizeof(PacketEnterRoomNtf);
    memcpy(ntf.user_id, user->GetUserID(), MAX_USER_ID_LENGTH);

    room->NotifyUsers(reinterpret_cast<char *>(&ntf), sizeof(PacketEnterRoomNtf));
}

void PacketManager::FuncLeaveRoom(PacketInfo packet_info) {
    // Prepare response packet
    PacketLeaveRoomRes res;
    res.packet_id = PacketID::ROOM_LEAVE_RES;
    res.total_size = sizeof(PacketLeaveRoomRes);

    // Check user is in room
    auto user = user_manager_.GetUser(packet_info.client_index);

    if (user->GetState() != User::State::ROOM) {
        res.code = ErrorCode::ROOM_LEAVE_USER_NOT_IN_ROOM;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLeaveRoomRes));
        return;
    }

    // Check room number is valid
    if (user->GetRoomIndex() < 0 || user->GetRoomIndex() > MAX_CREATED_ROOMS) {
        res.code = ErrorCode::ROOM_LEAVE_INVALID_ROOM_INDEX;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLeaveRoomRes));
        return;
    }

    // Check room is not empty to leave
    auto room = room_manager_.GetRoom(user->GetRoomIndex());

    if (room->IsEmpty()) {
        res.code = ErrorCode::ROOM_LEAVE_EMPTY_ROOM;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLeaveRoomRes));
        return;
    }

    // Normal execution: leave room
    room_manager_.LeaveRoom(*user);
    res.code = ErrorCode::NONE;
    AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketLeaveRoomRes));

    // Notify users in the room about leaving user
    PacketLeaveRoomNtf ntf;
    ntf.packet_id = PacketID::ROOM_LEAVE_NTF;
    ntf.total_size = sizeof(PacketLeaveRoomNtf);
    memcpy(ntf.user_id, user->GetUserID(), MAX_USER_ID_LENGTH);

    room->NotifyUsers(reinterpret_cast<char *>(&ntf), sizeof(PacketLeaveRoomNtf));
}

void PacketManager::FuncChatRoom(PacketInfo packet_info) {
    // Prepare response packet
    PacketChatRoomRes res;
    res.packet_id = PacketID::ROOM_CHAT_RES;
    res.total_size = sizeof(PacketChatRoomRes);

    // Check user is in room
    auto user = user_manager_.GetUser(packet_info.client_index);

    if (user->GetState() != User::State::ROOM) {
        res.code = ErrorCode::ROOM_CHAT_USER_NOT_IN_ROOM;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketChatRoomRes));
        return;
    }

    // Check room number is valid
    if (user->GetRoomIndex() < 0 || user->GetRoomIndex() > MAX_CREATED_ROOMS) {
        res.code = ErrorCode::ROOM_CHAT_INVALID_ROOM_INDEX;
        AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketChatRoomRes));
        return;
    }

    // Normal execution: chat
    auto req = reinterpret_cast<PacketChatRoomReq *>(packet_info.buf);
    auto room = room_manager_.GetRoom(user->GetRoomIndex());

    PacketChatRoomNtf ntf;
    ntf.packet_id = PacketID::ROOM_CHAT_NTF;
    ntf.total_size = sizeof(PacketChatRoomNtf);
    memcpy(ntf.message, req->message, req->message_length);

    room->NotifyUsers(reinterpret_cast<char *>(&ntf), sizeof(PacketChatRoomNtf));

    // Send response to chatted user
    res.code = ErrorCode::NONE;
    AsyncSend(packet_info.client_index, reinterpret_cast<char *>(&res), sizeof(PacketChatRoomRes));
}
