#pragma once

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include <cstdint>

#define PORT 1234
#define MAXLINE 1024

class Gateway {
private:
    RingBuffer<Order>& ordersBuffer;
    std::atomic<bool>& running;
    uint64_t numOrders;

public:
    Gateway(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, uint64_t _numOrders);
    void run();
};