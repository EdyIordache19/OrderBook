#pragma once

#include "orderbook.hpp"
#include "ring_buffer.hpp"

#include <thread>
#include <string.h>
#include <atomic>
#include <vector>

class Engine {
private:
    OrderBook& orderBook;
    RingBuffer<Order>& ordersBuffer;
    std::atomic<bool>& running;
    std::thread engineThread;
    std::vector<uint64_t> latencies;
public:
    Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& orderBook, uint64_t numOrders);
    void run();
    void stop();
    void printStats();
};