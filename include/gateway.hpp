#pragma once

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include <cstdint>

#define PORT 1234
#define MAXLINE 1024

/**
 * @brief Class that handles orders coming in from the network
 */
class Gateway {
private:
    // Lock-free SPSC ring buffer
    RingBuffer<Order>& ordersBuffer;

    // Shared running flag across gateway, engine and publisher threads
    // Gateway sets false on KILL order or after numOrders orders
    std::atomic<bool>& running;
    uint64_t numOrders;

public:
    Gateway(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, uint64_t _numOrders);
    void run();
};