#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"


void engine(OrderBook& orderBook, RingBuffer& buffer, std::atomic<bool>& running, std::vector<uint64_t>& latencies) {
    latencies.reserve(NUM_ORDERS);

    while (running.load(std::memory_order_acquire) || !buffer.is_empty()) {
        Order order;
        if (!buffer.pop(order)) {
            if (order.type == OrderType::MARKET) {
                if (order.side == Order::BUY) order.price = NUM_ORDERS - 1;
                else order.price = 0;

                order.tif = TimeInForce::IOC;
            }

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
    RingBuffer buffer(128);
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
        auto start_wait = std::chrono::high_resolution_clock::now();
        while (std::chrono::high_resolution_clock::now() - start_wait < std::chrono::nanoseconds(250));
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

    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99% Latency:     " << p99 << " ns\n";
    std::cout << "99.9% Latency:   " << p99_9 << " ns\n";

    return 0;
}