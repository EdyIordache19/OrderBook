#pragma once

#include <vector>
#include <atomic>

/**
 * @brief Lock-free SPSC ring buffer class for handoff between two threads
 * @pre buff_size is a power of 2
 *  - alignas and padding for CPU cache line friendliness
 *  - atomic head and tail variables used with acquire/release for ordering in other threads
 */
template <typename T>
class RingBuffer {
private:
    std::vector<T> buffer;
    alignas(64) std::atomic<size_t> head;
    char padding[64];
    alignas(64) std::atomic<size_t> tail;
    size_t buff_size;
public:
    RingBuffer<T>(size_t buff_size) {
        this->buff_size = buff_size;
        buffer.resize(buff_size);
        tail = 0;
        head = 0;
    }

    // Producer writes buffer[slot] and stores in tail with release
    int push(const T& item) {
        if (tail - head.load(std::memory_order_acquire) >= buff_size) {
            return 1;
        }

        // Bitwise operation
        size_t slot = tail.load(std::memory_order_acquire) & (buff_size - 1);
        buffer[slot] = item;

        tail.store(tail + 1, std::memory_order_acquire);
        return 0;
    }

    // Consumer reads buffer[slot] and stores in head with release
    int pop(T& item) {
        if (tail.load(std::memory_order_acquire) == head) {
            return 1;
        }

        // Bitwise operation
        size_t slot = head.load(std::memory_order_acquire) & (buff_size - 1);
        item = buffer[slot];

        head.store(head + 1, std::memory_order_release);
        return 0;
    }

    bool is_empty() {
        return tail == head;
    }
};