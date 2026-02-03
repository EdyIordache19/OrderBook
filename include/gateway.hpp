#pragma once

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include <cstdint>

#define PORT 1234
#define MAXLINE 1024

class Gateway {
public:
    void run(RingBuffer& ring_buffer, std::atomic<bool>& running, uint64_t numOrders);
};