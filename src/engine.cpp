#include "engine.hpp"
#include "main.hpp"

Engine::Engine(RingBuffer& buffer, std::atomic<bool>& isRunning, OrderBook& book)
    : ringBuffer(buffer),
      running(isRunning),
      orderBook(book)
{
    latencies.reserve(100000);
}

void Engine::runLoop() {
    pin_thread_to_core(1);
    while (running.load(std::memory_order_acquire) || !ringBuffer.is_empty()) {
        Order order;
        if (!ringBuffer.pop(order)) {
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

void Engine::start() {
    running = true;
    engineThread = std::thread(&Engine::runLoop, this);
}

void Engine::stop() {
    running = false;
    if (engineThread.joinable()) {
        engineThread.join();
    }
}

void Engine::printStats() {
    std::sort(latencies.begin(), latencies.end());

    double median = latencies[latencies.size() * 0.5];
    double p99 = latencies[latencies.size() * 0.99];
    double p99_9 = latencies[latencies.size() * 0.999];

    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99% Latency:     " << p99 << " ns\n";
    std::cout << "99.9% Latency:   " << p99_9 << " ns\n";
}