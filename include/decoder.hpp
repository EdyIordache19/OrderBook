#pragma once

#include "order_types.hpp"
#include <cstdint>
#include <cstring>
#include <chrono>

struct WireMessage {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;

    char side;
    uint8_t type;
    uint8_t tif;
};


class Decoder {
public:
    static bool decode(const char* buffer, size_t len, Order& order, uint64_t maxPrice);
};