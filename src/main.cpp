#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"
#include "gateway.hpp"
#include "engine.hpp"

#include <cxxopts.hpp>

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main(int argc, char* argv[]) {
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

    OrderBook orderBook(numOrders);
    RingBuffer buffer(bufferSize);

    std::atomic<bool> running = true;

    Engine engine(buffer, running, orderBook, numOrders);
    engine.start();

    Gateway gateway;
    gateway.run(buffer, running, numOrders);

    engine.stop();
    engine.printStats();

    // Print current orders
    std::string filename = result["output"].as<std::string>();
    orderBook.printOrders(filename);

    return 0;
}