#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"
#include "gateway.hpp"

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void engine(OrderBook& orderBook, RingBuffer& buffer, std::atomic<bool>& running, std::vector<uint64_t>& latencies) {
    pin_thread_to_core(1);

    latencies.reserve(NUM_ORDERS);

    while (running.load(std::memory_order_acquire) || !buffer.is_empty()) {
        Order order;
        if (!buffer.pop(order)) {
            if (order.tif == TimeInForce::IOC) {
                // Just process (match order with book) and dismiss the remainder
                orderBook.processOrder(order);
            } else if (order.tif == TimeInForce::FOK) {
                // Check if it can be filled entirely and process order
                if (orderBook.canFill(order)) {
                    orderBook.processOrder(order);
                }
            } else {
                // Process order and add remainder to book
                uint32_t remaining_qty = orderBook.processOrder(order);

                order.quantity = remaining_qty;
                orderBook.addOrder(order);
            }

            uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count();
            latencies.push_back(now - order.timestamp);
        } else {
            continue;
        }
    }
}

int main(int argc, char* argv[]) {
    OrderBook orderBook;
    RingBuffer buffer(256);
    std::vector<uint64_t> latencies;

    std::atomic<bool> running = true;

    std::thread engine_thread(engine, std::ref(orderBook), std::ref(buffer), std::ref(running), std::ref(latencies));

    if (argc < 3) {
        std::cout << "You need to parse 2 files\n";
        return 1;
    }

    Gateway gateway;
    gateway.run(buffer, running);

    running.store(false, std::memory_order_release);
    engine_thread.join();

    // Print current orders
    orderBook.printOrders(argv[2]);

    std::sort(latencies.begin(), latencies.end());

    double median = latencies[latencies.size() * 0.5];
    double p99 = latencies[latencies.size() * 0.99];
    double p99_9 = latencies[latencies.size() * 0.999];

    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99% Latency:     " << p99 << " ns\n";
    std::cout << "99.9% Latency:   " << p99_9 << " ns\n";

    return 0;
}