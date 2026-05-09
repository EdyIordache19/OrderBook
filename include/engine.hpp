#pragma once

#include "orderbook.hpp"
#include "ring_buffer.hpp"

#include <cstdint>
#include <sys/types.h>
#include <thread>
#include <atomic>
#include <vector>

/**
 * @brief Main Engine class that orchestrates orders from SPSC ring buffer (gateway -> engine)
 *  - applies TIF semantics
 *  - handles cancel and kill orders
 *  - updates the user ledger
 *  - pushes trades to matchBuffer for publisher thread
 */
class Engine {
private:
    OrderBook& orderBook;
    RingBuffer<Order>& ordersBuffer;
    std::atomic<bool>& running;
    std::thread engineThread;
    std::vector<uint64_t> core_to_core_latencies;
    std::vector<uint64_t> engine_latencies;
    double tsc_ticks_per_ns;

    // Atomic variables for ledger, to avoid data races (shared with publisher thread)
    // Used with memory_order_relaxed, since they are independent numeric states
    std::atomic<int64_t>& usd_balance;
    std::atomic<int64_t>& equity_balance;

public:
    Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& orderBook, uint64_t numOrders,
        std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance, double _tsc_ticks_per_ns);
    void run();
    void stop();
    void printStats();
};