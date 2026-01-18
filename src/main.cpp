#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"

#define NUM_ORDERS 10000000

void engine(OrderBook& orderBook, RingBuffer& buffer, std::atomic<bool>& running, std::vector<uint64_t>& latencies) {
    latencies.reserve(NUM_ORDERS);

    while (running.load(std::memory_order_acquire) || !buffer.is_empty()) {
        Order order;
        if (!buffer.pop(order)) {
            orderBook.addOrder(order);
            orderBook.matchOrders();

            uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count();
            latencies.push_back(now - order.timestamp);
        } else {
            continue;
        }
    }
}

int main(int argc, char* argv[]) {
    OrderBook orderBook;
    RingBuffer buffer(2048);
    std::vector<uint64_t> latencies;

    std::atomic<bool> running = true;

    std::thread engine_thread(engine, std::ref(orderBook), std::ref(buffer), std::ref(running), std::ref(latencies));

    if (argc < 3) {
        std::cout << "You need to parse 2 files\n";
        return 1;
    }

    // Generate random orders and write to file
    std::string ordersFile = argv[1];

    std::list<Order> orders = OrdersGenerator::generateOrdersToFile(ordersFile, NUM_ORDERS);

    auto start = std::chrono::high_resolution_clock::now();

    // Add orders to the order book
    for (auto& order : orders) {
        order.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        while (buffer.push(order) != 0) {
            continue;
        }

        /**
         * Wait as in a real-life scenario
         * If no wait, the throughput is bigger, but the buffer gets flooded and the latency is very high
         */
        // auto start_wait = std::chrono::high_resolution_clock::now();
        // while (std::chrono::high_resolution_clock::now() - start_wait < std::chrono::nanoseconds(500));
    }

    running.store(false, std::memory_order_release);
    engine_thread.join();

    auto end = std::chrono::high_resolution_clock::now();

    // Print current orders
    orderBook.printOrders(argv[2]);

    std::chrono::duration<double> diff = end - start;
    double throughput = NUM_ORDERS / diff.count();

    std::cout << "Processed " << NUM_ORDERS << " orders in " << diff.count() << " seconds.\n";
    std::cout << "Throughput: " << throughput << " orders/second. \n";

    std::sort(latencies.begin(), latencies.end());

    double median = latencies[NUM_ORDERS * 0.5];
    double p99 = latencies[NUM_ORDERS * 0.99];
    double p99_9 = latencies[NUM_ORDERS * 0.999];

    std::cout << "Median Latency: " << median << " ns\n";
    std::cout << "99th% Latency:  " << p99 << " ns\n";
    std::cout << "99.9th% Latency:" << p99_9 << " ns\n";

    return 0;
}