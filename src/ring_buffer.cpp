#include "ring_buffer.hpp"

#include <iostream>

void RingBuffer::addToRing(const Order& order) {
    if (tail - head >= BUFF_SIZE) {
        std::cout << "BUFFER IS FULL, CAN'T WRITE";
        return;
    }

    std::atomic<size_t> slot = tail & (BUFF_SIZE - 1);
    buffer[slot] = order;

    tail++;
}

void RingBuffer::removeFromRing(Order& order) {
    if (tail - head >= BUFF_SIZE) {
        std::cout << "BUFFER IS FULL, CAN'T READ";
        return;
    }

    std::atomic<size_t> slot = head & (BUFF_SIZE - 1);
    order = buffer[slot];

    head++;
}