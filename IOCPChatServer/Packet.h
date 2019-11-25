#pragma once

enum PacketID : unsigned short
{
    SYS_ACCEPT = 1,
    SYS_CLOSE = 2,

    ECHO_REQ = 241,
    ECHO_RES = 242
};

struct PacketInfo
{
    int client_index;
    PacketID packet_id;
    unsigned short size;
    char * buf;
};

#pragma pack(push, 1)

struct PacketHeader {
    unsigned short total_size;
    PacketID packet_id;
    unsigned char reserved;
};

#pragma pack(pop)
