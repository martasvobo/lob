#include "OrderMessage.hpp"
#include <iostream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 9000;

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    for (uint64_t i = 1; i <= 10; ++i)
    {
        OrderMessage order{i, "AAPL", 150.0 + i, 100 + i, (i % 2 == 0) ? 'B' : 'S'};
        send(sock, (char *)&order, sizeof(order), 0);
        std::cout << "Sent order " << order.id << std::endl;
    }
    closesocket(sock);
    WSACleanup();
    return 0;
}
