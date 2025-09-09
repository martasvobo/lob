#include "MPSCQueue.hpp"
#include "Order.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>

using namespace blanknamespace;

struct TimedOrder : public Order
{
    std::chrono::high_resolution_clock::time_point enqueueTime;

    TimedOrder(int id, bool buy, int sh, int lim)
        : Order(id, buy, sh, lim), enqueueTime(std::chrono::high_resolution_clock::now()) {}
};

int main()
{
    constexpr size_t totalOps = 20'000'000;
    constexpr size_t numProducers = 4;
    constexpr size_t queueCapacity = 1 << 16;

    MPSCQueue<TimedOrder> q(queueCapacity);
    std::atomic<bool> startFlag{false};
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};

    std::vector<long long> latencies;
    latencies.reserve(totalOps);

    // Producer threads
    std::vector<std::thread> producers;
    for (size_t p = 0; p < numProducers; ++p)
    {
        producers.emplace_back([&, p]()
                               {
            while (!startFlag.load(std::memory_order_acquire)) {
                // spin until start
            }
            for (;;) {
                size_t id = produced.fetch_add(1);
                if (id >= totalOps) break;
                TimedOrder order((int)id, (id % 2 == 0), (id % 100) + 1, (id % 50) + 1);
                q.push(order);
            } });
    }

    // Consumer thread
    auto consumer = std::thread([&]()
                                {
        while (!startFlag.load(std::memory_order_acquire)) {
            // spin until start
        }
        TimedOrder value(0, true, 0, 0);
        while (consumed.load() < totalOps) {
            if (q.try_pop(value)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - value.enqueueTime).count();
                latencies.push_back(ns);
                consumed.fetch_add(1);
            }
        } });

    // Start benchmark
    auto t1 = std::chrono::high_resolution_clock::now();
    startFlag.store(true, std::memory_order_release);

    for (auto &p : producers)
        p.join();
    consumer.join();
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t2 - t1;

    // Sort latencies for percentiles
    std::sort(latencies.begin(), latencies.end());
    auto p50 = latencies[latencies.size() * 50 / 100];
    auto p90 = latencies[latencies.size() * 90 / 100];
    auto p99 = latencies[latencies.size() * 99 / 100];

    std::cout << "Total items: " << totalOps << "\n";
    std::cout << "Time: " << diff.count() << "s\n";
    std::cout << "Throughput: " << (double)totalOps / diff.count() << " ops/sec\n";
    std::cout << "Latency (nanoseconds): p50=" << p50
              << " ns, p90=" << p90 << " ns, p99=" << p99 << " ns\n";
}
