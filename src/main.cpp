#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"

#include <atomic>
#include <thread>

#define NUM_ORDERS 100000

void engine(OrderBook& orderBook, RingBuffer& buffer, std::atomic<bool>& running) {
    while (running.load(std::memory_order_acquire) || !buffer.is_empty()) {
        Order order;
        if (!buffer.pop(order)) {
            orderBook.addOrder(order);
            orderBook.matchOrders();
        } else {
            std::this_thread::yield();
        }
    }
}

int main(int argc, char* argv[]) {
    OrderBook orderBook;
    RingBuffer buffer(1024);

    std::atomic<bool> running = true;

    std::thread engine_thread(engine, std::ref(orderBook), std::ref(buffer), std::ref(running));

    auto start = std::chrono::high_resolution_clock::now();

    if (argc < 3) {
        std::cout << "You need to parse 2 files\n";
        return 1;
    }

    // Generate random orders and write to file
    std::string ordersFile = argv[1];

    std::list<Order> orders = OrdersGenerator::generateOrdersToFile(ordersFile, NUM_ORDERS);

    // Add orders to the order book
    for (const auto& order : orders) {
        while (buffer.push(order) != 0) {
            std::this_thread::yield();
        }
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

    return 0;
}