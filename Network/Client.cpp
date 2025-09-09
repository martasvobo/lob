#include "OrderMessage.hpp"
#include <iostream>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define closesocket close
#define SOCKET_ERROR -1
#define WSADATA int
#define WSAStartup(a,b) 0
#define WSACleanup()
#define WSAGetLastError() errno
#define MAKEWORD(a,b) 0
#endif

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
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    std::cout << "Connected to server!" << std::endl;

    for (uint64_t i = 1; i <= 10; ++i)
    {
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch())
                          .count();
        OrderMessage order(i, ts, "AAPL", 150.0 + i, 100 + i, (i % 2 == 0) ? 'B' : 'S');
        int sent = send(sock, (char *)&order, sizeof(OrderMessage), 0);
        std::cout << "Sent order " << order.id << " (" << sent << " bytes)" << std::endl;
    }
    std::cout << "Client finished sending orders, disconnecting." << std::endl;
    closesocket(sock);
    WSACleanup();
    return 0;
}
