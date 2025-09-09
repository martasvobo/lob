#include "OrderMessage.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

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
constexpr int NUM_MESSAGES = 10000000;  // per client

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    for (int i = 0; i < NUM_MESSAGES; i++) {
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        OrderMessage order(i, ts, "AAPL", 150.0, 100, (i % 2 == 0) ? 'B' : 'S');
        send(sock, (char*)&order, sizeof(OrderMessage), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
