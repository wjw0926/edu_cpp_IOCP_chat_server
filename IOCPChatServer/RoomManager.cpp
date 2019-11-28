#include "RoomManager.h"
#include "Config.h"

void RoomManager::Init() {
    auto async_send_func = [&](int client_index, char * buf, int size) { AsyncSend(client_index, buf, size); };

    for (int i = 0; i < MAX_CREATED_ROOMS; ++i) {
        rooms_.emplace_back(Room(i));
        rooms_[i].AsyncSend = async_send_func;
    }
}

void RoomManager::EnterRoom(User &user, int room_index) {
    auto room = GetRoom(room_index);

    room->AddUser(user);
    user.SetState(User::State::ROOM);
    user.SetRoomIndex(room_index);
}

void RoomManager::LeaveRoom(User &user) {
    auto room = GetRoom(user.GetRoomIndex());

    room->RemoveUser(user);
    user.SetState(User::State::LOGIN);
    user.ClearRoomIndex();
}
