#pragma once

#include <vector>
#include <functional>
#include "Room.h"
#include "User.h"

class RoomManager {
public:
    RoomManager(void) {};
    ~RoomManager(void) {};

    std::function<void(int client_index, char * buf, int size)> AsyncSend;

    void Init();

    void EnterRoom(User &user, int room_index);
    void LeaveRoom(User &user);
    Room *GetRoom(int index) { return &rooms_.at(index); };

private:
    std::vector<Room> rooms_;
};
