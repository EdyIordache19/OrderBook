#include "main.hpp"
#include "order_types.hpp"
#include "orderbook.hpp"
#include "ring_buffer.hpp"
#include "gateway.hpp"
#include "engine.hpp"
#include "publisher.hpp"

#include <iostream>
#include <fstream>
#include <atomic>
#include <thread>
#include <x86intrin.h>

#include <cxxopts.hpp>

double tsc_ticks_per_ns = 0.0;

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void calibrate_tsc() {
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t start_tsc = __rdtsc();

    // Sleep for a short duration to get a measurable gap (10-100ms is usually plenty)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    uint64_t end_tsc = __rdtsc();
    auto end_time = std::chrono::high_resolution_clock::now();

    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

    // Calculate ticks per nanosecond (which is effectively the frequency in GHz)
    tsc_ticks_per_ns = static_cast<double>(end_tsc - start_tsc) / elapsed_ns;
}

int main(int argc, char* argv[]) {
    calibrate_tsc();

    cxxopts::Options options("OrderBook", "High-Performance OrderBook Engine");

    options.add_options()
        ("h,help", "Print usage")
        ("o, output", "Output file for orders", cxxopts::value<std::string>())
        ("b, buffer-size", "Size of ring buffer", cxxopts::value<size_t>()->default_value("65536"))
        ("n, num-orders", "Number of orders to be parsed", cxxopts::value<uint64_t>()->default_value("1000000"));

    auto result = options.parse(argc, argv);
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (!result.count("output")) {
        std::cerr << "Output file not specified\n";
        return 1;
    }

    size_t bufferSize = result["buffer-size"].as<size_t>();
    uint64_t numOrders = result["num-orders"].as<uint64_t>();

    std::string filename = result["output"].as<std::string>();
    std::ofstream outFile(filename);

    RingBuffer<Order> ordersBuffer(bufferSize);
    RingBuffer<Trade> matchBuffer(bufferSize);
    RingBuffer<OrderHistory> historyBuffer(bufferSize);
    OrderBook orderBook(numOrders, matchBuffer, historyBuffer);

    std::atomic<bool> running = true;
    std::atomic<int64_t> usd_balance = 1000000;
    std::atomic<int64_t> equity_balance = 0;

    Engine engine(ordersBuffer, running, orderBook, numOrders, usd_balance, equity_balance, tsc_ticks_per_ns);
    Gateway gateway(ordersBuffer, running, numOrders);
    Publisher publisher(orderBook, matchBuffer, historyBuffer,
         running, filename, usd_balance, equity_balance);

    std::thread t_engine(&Engine::run, &engine);
    std::thread t_gateway(&Gateway::run, &gateway);
    std::thread t_publisher(&Publisher::run, &publisher);

    t_gateway.join();
    t_engine.join();
    t_publisher.join();

    // engine.stop();
    engine.saveLatencies();

    engine.printStats();
    // Print current orders
    // orderBook.printOrders(filename);

    return 0;
}