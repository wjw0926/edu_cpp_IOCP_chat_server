#pragma once

#include <list>
#include <functional>
#include "Config.h"
#include "User.h"

class Room {
public:
    Room() { index_ = -1; };
    explicit Room(int index) { index_ = index; };
    ~Room() {};

    std::function<void(int, char *, int)> AsyncSend;

    int GetIndex() { return index_; };

    bool IsFull() { return users_.size() >= MAX_USERS_IN_ROOM; };
    bool IsEmpty() { return users_.empty(); };

    void AddUser(User &user) { users_.push_back(&user); };
    void RemoveUser(User &user) { users_.remove(&user); };

    void NotifyUsers(char *packet, int size) {
        for (auto it = users_.begin(); it != users_.end(); ++it) {
            auto user = *it;
            AsyncSend(user->GetIndex(), packet, size);
        }
    };

private:
    int index_;
    std::list<User*> users_;
};
