#pragma once

class User {
public:
    User() { state_ = State::NONE; index_ = -1; };
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

private:
    State state_;
    int index_;
};
