#pragma once

#include <cstring>
#include "Config.h"

class User {
public:
    User()
    {
        state_ = NONE;
        index_ = -1;
        room_index_ = -1;
    };
    ~User() {};

    enum State {
        NONE = 0,
        CONNECT = 1,
        LOGIN = 2,
        ROOM = 3
    };

    int GetIndex() { return index_; };
    void SetIndex(int index) { index_ = index; };

    State GetState() { return state_; };
    void SetState(State state) { state_ = state; };

    int GetRoomIndex() { return room_index_; };
    void SetRoomIndex(int room_index) { room_index_ = room_index; };
    void ClearRoomIndex() { room_index_ = -1; };

    char* GetUserID() { return user_id_; };
    void SetUserID(char user_id[MAX_USER_ID_LENGTH]) { memcpy(user_id_, user_id, MAX_USER_ID_LENGTH); };

    void Clear() {
        state_ = NONE;
        index_ = -1;
        room_index_ = -1;
        memset(user_id_, 0, MAX_USER_ID_LENGTH);
    }

private:
    State state_;
    int index_;
    int room_index_;
    char user_id_[MAX_USER_ID_LENGTH];
};
