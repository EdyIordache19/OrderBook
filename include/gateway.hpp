#pragma once

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include <cstdint>

#define PORT 1234
#define MAXLINE 1024

struct WireMessage {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;

    char side;
    uint8_t type;
    uint8_t tif;
};

class Gateway {
public:
    void run(RingBuffer& ring_buffer, std::atomic<bool>& running);
};