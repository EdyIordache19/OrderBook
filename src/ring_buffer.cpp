#include "ring_buffer.hpp"

#include <iostream>

RingBuffer::RingBuffer(size_t buff_size) {
    this->buff_size = buff_size;
    buffer.resize(buff_size);
    buffer.clear();
    tail = 0;
    head = 0;
}

int RingBuffer::push(const Order& order) {
    if (tail - head >= buff_size) {
        std::cout << "BUFFER IS FULL, CAN'T WRITE\n";
        return 1;
    }

    std::atomic<size_t> slot = tail & (buff_size - 1);
    buffer[slot] = order;

    tail++;
    return 0;
}

int RingBuffer::pop(Order& order) {
    if (tail == head) {
        std::cout << "BUFFER IS EMPTY, CAN'T READ\n";
        return 1;
    }

    std::atomic<size_t> slot = head & (buff_size - 1);
    order = buffer[slot];

    head++;
    return 0;
}

bool RingBuffer::is_empty() {
    return tail == head;
}