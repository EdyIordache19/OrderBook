#pragma once

#include "order_types.hpp"

#include <vector>
#include <atomic>

#define BUFF_SIZE 1024

class RingBuffer {
private:
    std::vector<Order> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;

public:
    void addToRing(const Order& order);
    void removeFromRing(Order& outputOrder);
};