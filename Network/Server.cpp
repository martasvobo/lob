#include "OrderMessage.hpp"
#include "../Limit_Order_Book/MPSCQueue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>

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
constexpr size_t QUEUE_CAPACITY = 1024;

std::atomic<bool> running{true};

// Function to get the singleton queue instance
blanknamespace::MPSCQueue<OrderMessage>& getOrderQueue() {
    static blanknamespace::MPSCQueue<OrderMessage> orderQueue(QUEUE_CAPACITY);
    return orderQueue;
}

void processOrders()
{
    size_t processed = 0;
    uint64_t total_latency = 0;
    uint64_t min_latency = UINT64_MAX;
    uint64_t max_latency = 0;
    auto start = std::chrono::high_resolution_clock::now();
    auto& orderQueue = getOrderQueue();
    while (running)
    {
        OrderMessage order;
        if (orderQueue.try_pop(order))
        {
            processed++;
            uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::high_resolution_clock::now().time_since_epoch())
                               .count();
            uint64_t latency = now - order.timestamp;
            total_latency += latency;
            if (latency < min_latency)
                min_latency = latency;
            if (latency > max_latency)
                max_latency = latency;
        }
        // Benchmark for 5 seconds
        auto now_tp = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now_tp - start).count() >= 5)
        {
            double throughput = processed / 5.0;
            double avg_latency = processed ? (total_latency / processed) / 1e6 : 0.0;
            std::cout << "Benchmark: Processed " << processed << " orders in 5 seconds. Throughput: " << throughput << " orders/sec" << std::endl;
            std::cout << "Latency (ms): avg=" << avg_latency << " min=" << (min_latency / 1e6) << " max=" << (max_latency / 1e6) << std::endl;
            processed = 0;
            total_latency = 0;
            min_latency = UINT64_MAX;
            max_latency = 0;
            start = now_tp;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void handleClient(int clientSock)
{
    auto& orderQueue = getOrderQueue();
    while (running)
    {
        OrderMessage order;
        int bytes = recv(clientSock, (char *)&order, sizeof(order), 0);
        std::cout << "Received " << bytes << " bytes from client " << clientSock << std::endl;
        if (bytes <= 0)
            break;
        orderQueue.emplace(std::move(order));
    }
    std::cout << "Client disconnected: " << clientSock << std::endl;
    closesocket(clientSock);
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSock, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }
    if (listen(serverSock, 5) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    std::thread processor(processOrders);
    std::vector<std::thread> clientThreads;

    while (running)
    {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0)
            continue;
        std::cout << "Accepted client connection: " << clientSock << std::endl;
        clientThreads.emplace_back(handleClient, clientSock);
    }

    for (auto &t : clientThreads)
        t.join();
    processor.join();
    closesocket(serverSock);
    return 0;
}
