#pragma once

enum PacketID : unsigned short
{
    SYS_ACCEPT = 1,
    SYS_CLOSE = 2,

    ECHO_REQ = 241,
    ECHO_RES = 242,

    LOGIN_REQ = 21,
    LOGIN_RES = 22,

    ROOM_ENTER_REQ = 61,
    ROOM_ENTER_RES = 62,
    ROOM_ENTER_NTF = 63,

    ROOM_LEAVE_REQ = 66,
    ROOM_LEAVE_RES = 67,
    ROOM_LEAVE_NTF = 68,

    ROOM_CHAT_REQ = 76,
    ROOM_CHAT_RES = 77,
    ROOM_CHAT_NTF = 78
};

struct PacketInfo
{
    int client_index;
    PacketID packet_id;
    unsigned short size;
    char * buf;
};

const int MAX_USER_ID_LENGTH = 16;
const int MAX_USER_PW_LENGTH = 16;
const int MAX_ROOM_CHAT_MSG_LENGTH = 256;

#pragma pack(push, 1)

struct PacketHeader {
    unsigned short total_size;
    PacketID packet_id;
    unsigned char reserved;
};

struct PacketLoginReq {
    char user_id[MAX_USER_ID_LENGTH];
    char user_pw[MAX_USER_PW_LENGTH];
};

struct PacketLoginRes : PacketHeader {
    unsigned short code;
};

struct PacketEnterRoomReq {
    int room_number;
};

struct PacketEnterRoomRes : PacketHeader {
    unsigned short code;
};

struct PacketEnterRoomNtf : PacketHeader {
    char user_id[MAX_USER_ID_LENGTH];
};

struct PacketLeaveRoomRes : PacketHeader {
    unsigned short code;
};

struct PacketLeaveRoomNtf : PacketHeader {
    char user_id[MAX_USER_ID_LENGTH];
};

struct PacketChatRoomReq {
    unsigned short message_length;
    char message[MAX_ROOM_CHAT_MSG_LENGTH];
};

struct PacketChatRoomRes : PacketHeader {
    unsigned short code;
};

struct PacketChatRoomNtf : PacketHeader {
    char message[MAX_ROOM_CHAT_MSG_LENGTH];
};

#pragma pack(pop)
