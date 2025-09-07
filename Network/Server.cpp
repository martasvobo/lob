#include "OrderMessage.hpp"
#include "../Limit_Order_Book/MPMCQueue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <atomic>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 9000;
constexpr size_t QUEUE_CAPACITY = 1024;

// Example: queue for incoming orders
rigtorp::MPMCQueue<OrderMessage> orderQueue(QUEUE_CAPACITY);
std::atomic<bool> running{true};

void processOrders()
{
    while (running)
    {
        OrderMessage order;
        if (orderQueue.try_pop(order))
        {
            std::cout << "Processed order: " << order.id << " " << order.symbol << " " << order.price << " " << order.quantity << " " << order.side << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void handleClient(int clientSock)
{
    while (running)
    {
        OrderMessage order;
        int bytes = recv(clientSock, (char*)&order, sizeof(order), 0);
        if (bytes <= 0)
            break;
    orderQueue.emplace(std::move(order));
    }
    closesocket(clientSock);
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    bind(serverSock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    listen(serverSock, 5);
    std::cout << "Server listening on port " << PORT << std::endl;

    std::thread processor(processOrders);
    std::vector<std::thread> clientThreads;

    while (running)
    {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0)
            continue;
        clientThreads.emplace_back(handleClient, clientSock);
    }

    for (auto &t : clientThreads)
        t.join();
    processor.join();
    closesocket(serverSock);
    return 0;
}
