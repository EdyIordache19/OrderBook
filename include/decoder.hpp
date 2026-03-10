#pragma once

#include "order_types.hpp"
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>

struct __attribute__((packed)) WireMessage {
    uint64_t id;
    uint32_t user_id;
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