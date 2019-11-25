#pragma once

#include <vector>
#include <unordered_map>
#include "User.h"

class UserManager {
public:
    UserManager() {};
    ~UserManager() {};

    void Init();

    void AddUser(int session_index);
    void RemoveUser(int session_index);
    User* GetUser(int index);

private:
    std::vector<User> users_;
    std::unordered_map<int, User*> index_user_map_;
};
