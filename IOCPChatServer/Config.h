#pragma once

#include "Packet.h"

const unsigned short MAX_PACKET_ID = 256;
const unsigned short PACKET_HEADER_SIZE = sizeof(PacketHeader);

const unsigned int MAX_CLIENTS = 100;
const unsigned int MAX_BUFFER = 1024;
const unsigned int MAX_SEND_QUEUE_LENGTH = 100;

const unsigned short MAX_CONNECTED_USERS = 90;
const unsigned short MAX_CREATED_ROOMS = 2000;
const unsigned short MAX_USERS_IN_ROOM = 3;
