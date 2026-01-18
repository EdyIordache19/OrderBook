#pragma once

#include "order_types.hpp"

#include <vector>
#include <atomic>

class RingBuffer {
private:
    std::vector<Order> buffer;
    alignas(64) std::atomic<size_t> head;
    alignas(64) std::atomic<size_t> tail;
    size_t buff_size;
public:
    RingBuffer(size_t buff_size);
    int push(const Order& order);
    int pop(Order& outputOrder);
    bool is_empty();
};