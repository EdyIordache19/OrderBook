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

    std::atomic<int64_t>& usd_balance;
    std::atomic<int64_t>& equity_balance;

public:
    Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& orderBook, uint64_t numOrders,
        std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance);
    void run();
    void stop();
    void printStats();
};