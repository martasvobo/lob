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

constexpr int PORT = 9001; // Use different port to avoid conflicts
constexpr size_t QUEUE_CAPACITY = 1024;
constexpr int NUM_CLIENTS = 3;
constexpr int ORDERS_PER_CLIENT = 100;

std::atomic<bool> serverRunning{true};
std::atomic<int> connectedClients{0};
std::atomic<int> totalOrdersProcessed{0};

// Function to get the singleton test queue instance
blanknamespace::MPSCQueue<OrderMessage>& getTestQueue() {
    static blanknamespace::MPSCQueue<OrderMessage> testQueue(QUEUE_CAPACITY);
    return testQueue;
}

void orderProcessor() {
    OrderMessage order;
    auto startTime = std::chrono::high_resolution_clock::now();
    auto& testQueue = getTestQueue();

    while (serverRunning || !testQueue.empty()) {
        if (testQueue.try_pop(order)) {
            totalOrdersProcessed++;

            // Calculate latency
            auto now = std::chrono::high_resolution_clock::now();
            auto orderTime = std::chrono::time_point<std::chrono::high_resolution_clock>(
                std::chrono::nanoseconds(order.timestamp));
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - orderTime).count();

            std::cout << "Processed order " << order.id
                      << " from " << order.symbol
                      << " (latency: " << latency << "μs)" << std::endl;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "\n=== Order Processing Summary ===" << std::endl;
    std::cout << "Total orders processed: " << totalOrdersProcessed << std::endl;
    std::cout << "Total time: " << totalTime << "ms" << std::endl;
    std::cout << "Throughput: " << (totalOrdersProcessed * 1000.0 / totalTime) << " orders/sec" << std::endl;
}

void handleClient(int clientSocket, int clientId) {
    std::cout << "Client " << clientId << " connected" << std::endl;
    connectedClients++;
    auto& testQueue = getTestQueue();

    while (serverRunning) {
        OrderMessage order;
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&order), sizeof(order), 0);

        if (bytesReceived <= 0) {
            break; // Client disconnected
        }

        if (bytesReceived == sizeof(order)) {
            // Successfully received complete order, add to queue
            testQueue.push(order);
        } else {
            std::cerr << "Received incomplete order from client " << clientId << std::endl;
        }
    }

    connectedClients--;
    closesocket(clientSocket);
    std::cout << "Client " << clientId << " disconnected" << std::endl;
}

void runServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == SOCKET_ERROR) {
        std::cerr << "Failed to create server socket" << std::endl;
        return;
    }

    // Set socket option to reuse address
    int opt = 1;
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind server socket" << std::endl;
        closesocket(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on server socket" << std::endl;
        closesocket(serverSocket);
        return;
    }

    std::cout << "Test server listening on port " << PORT << std::endl;

    std::vector<std::thread> clientThreads;
    int clientCounter = 0;

    // Accept connections until we have enough clients
    while (serverRunning && clientCounter < NUM_CLIENTS) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != SOCKET_ERROR) {
            clientThreads.emplace_back(handleClient, clientSocket, ++clientCounter);
        }
    }

    // Wait for all clients to finish
    for (auto& thread : clientThreads) {
        thread.join();
    }

    closesocket(serverSocket);
    WSACleanup();
}

void runClient(int clientId) {
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100 * clientId));

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == SOCKET_ERROR) {
        std::cerr << "Client " << clientId << ": Failed to create socket" << std::endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Client " << clientId << ": Failed to connect to server" << std::endl;
        closesocket(clientSocket);
        return;
    }

    std::cout << "Client " << clientId << " connected to server" << std::endl;

    // Send orders
    for (int i = 0; i < ORDERS_PER_CLIENT; ++i) {
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        std::string symbol = "SYM" + std::to_string(clientId);
        OrderMessage order(
            clientId * 1000 + i,  // unique order ID
            timestamp,
            symbol.c_str(),
            100.0 + (i % 50),     // price
            100 + (i % 100),      // quantity
            (i % 2 == 0) ? 'B' : 'S'  // side
        );

        int bytesSent = send(clientSocket, reinterpret_cast<char*>(&order), sizeof(order), 0);
        if (bytesSent != sizeof(order)) {
            std::cerr << "Client " << clientId << ": Failed to send order " << i << std::endl;
            break;
        }

        // Small delay to simulate realistic order flow
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    std::cout << "Client " << clientId << " finished sending " << ORDERS_PER_CLIENT << " orders" << std::endl;
    closesocket(clientSocket);
    WSACleanup();
}

int main() {
    std::cout << "=== MPSC Queue Network Test ===" << std::endl;
    std::cout << "Testing with " << NUM_CLIENTS << " clients, "
              << ORDERS_PER_CLIENT << " orders each" << std::endl;

    // Start order processor thread
    std::thread processor(orderProcessor);

    // Start server thread
    std::thread server(runServer);

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Start client threads
    std::vector<std::thread> clients;
    for (int i = 1; i <= NUM_CLIENTS; ++i) {
        clients.emplace_back(runClient, i);
    }

    // Wait for all clients to finish
    for (auto& client : clients) {
        client.join();
    }

    std::cout << "\nAll clients finished. Waiting for remaining orders to be processed..." << std::endl;

    // Wait a bit more for remaining orders to be processed
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop server and processor
    serverRunning = false;
    server.join();
    processor.join();

    // Verify results
    int expectedOrders = NUM_CLIENTS * ORDERS_PER_CLIENT;
    if (totalOrdersProcessed == expectedOrders) {
        std::cout << "\n✅ TEST PASSED: All " << expectedOrders << " orders were processed correctly!" << std::endl;
    } else {
        std::cout << "\n❌ TEST FAILED: Expected " << expectedOrders
                  << " orders, but processed " << totalOrdersProcessed << std::endl;
    }

    return 0;
}
