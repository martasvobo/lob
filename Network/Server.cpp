#include "OrderMessage.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <limits>

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

std::atomic<bool> running{true};
std::atomic<uint64_t> processed{0};
std::atomic<uint64_t> total_latency{0};
std::atomic<uint64_t> min_latency{std::numeric_limits<uint64_t>::max()};
std::atomic<uint64_t> max_latency{0};

void handleClient(int clientSock) {
    while (running) {
        OrderMessage order;
        int bytes = recv(clientSock, (char*)&order, sizeof(order), 0);
        if (bytes <= 0) break;

        // server timestamp for one-way latency measurement
        uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        uint64_t latency = now - order.timestamp;

        processed++;
        total_latency += latency;
        if (latency < min_latency) min_latency = latency;
        if (latency > max_latency) max_latency = latency;

        // echo back to client for RTT measurement
        send(clientSock, (char*)&order, sizeof(order), 0);
    }
    closesocket(clientSock);
}

void benchmarkPrinter() {
    using namespace std::chrono;
    auto last = high_resolution_clock::now();

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto now = high_resolution_clock::now();
        double elapsed = duration_cast<seconds>(now - last).count();
        last = now;

        uint64_t count = processed.exchange(0);
        uint64_t lat_sum = total_latency.exchange(0);
        uint64_t min_lat = min_latency.exchange(std::numeric_limits<uint64_t>::max());
        uint64_t max_lat = max_latency.exchange(0);

        double throughput = count / elapsed;
        double avg_latency_ms = count ? (lat_sum / count) / 1e6 : 0.0;

        std::cout << "Processed " << count << " orders in " << elapsed << "s. "
                  << "Throughput=" << throughput << " orders/s, "
                  << "One-way Latency avg=" << avg_latency_ms << " ms, "
                  << "min=" << (min_lat==std::numeric_limits<uint64_t>::max()?0:min_lat/1e6) << " ms, "
                  << "max=" << max_lat/1e6 << " ms\n";
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        return 1;
    }
    if (listen(serverSock, 128) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    std::thread printer(benchmarkPrinter);
    std::vector<std::thread> workers;

    while (running) {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) continue;
        std::cout << "Client connected\n";
        workers.emplace_back(handleClient, clientSock);
    }

    for (auto& t : workers) t.join();
    printer.join();
    closesocket(serverSock);
    WSACleanup();
}
