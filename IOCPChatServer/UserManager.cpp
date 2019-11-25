#include "UserManager.h"
#include "Config.h"

void UserManager::Init() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
    users_.emplace_back(User());
    }
}

void UserManager::AddUser(const int client_index) {
    users_[client_index].SetIndex(client_index);
    users_[client_index].SetState(User::State::CONNECT);

    index_user_map_[client_index] = &users_[client_index];
}

void UserManager::RemoveUser(const int client_index) {
    users_[client_index].SetIndex(-1);
    users_[client_index].SetState(User::State::NONE);

    index_user_map_.erase(client_index);
}

User* UserManager::GetUser(int index) {
    if (index_user_map_.find(index) == index_user_map_.end()) {
    return nullptr;
    }

    return index_user_map_[index];
}
