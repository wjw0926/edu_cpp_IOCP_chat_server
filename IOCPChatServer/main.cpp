#include <iostream>
#include "ChatServer.h"

const unsigned short SERVER_PORT = 11021;

int main() {
    ChatServer chat_server;

    chat_server.InitServer(SERVER_PORT);
    chat_server.StartServer();

    return 0;
}
