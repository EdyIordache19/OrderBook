#pragma once

#include "orderbook.hpp"
#include "ring_buffer.hpp"

#include <thread>
#include <string.h>

#define BUFFER_SIZE 256

class Engine {
private:
    OrderBook& orderBook;
    RingBuffer& ringBuffer;
    std::atomic<bool>& running;
    std::thread engineThread;
    std::vector<uint64_t> latencies;

    void runLoop();
public:
    Engine(RingBuffer& buffer, std::atomic<bool>& isRunning, OrderBook& book);
    void start();
    void stop();
    void printStats();
};