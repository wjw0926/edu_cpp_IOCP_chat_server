#pragma once

enum ErrorCode : unsigned short {
    NONE = 0,

    WSA_STARTUP_FAIL = 10,
    WSA_SOCKET_FAIL = 11,
    BIND_FAIL = 12,
    LISTEN_FAIL = 13,
    ACCEPT_FAIL = 14,

    CREATE_IOCP_FAIL = 30,
    CONNECT_IOCP_FAIL = 31,

    WSA_RECV_FAIL = 40,
    WSA_SEND_FAIL = 41,

    FULL_CLIENTS = 50
};
