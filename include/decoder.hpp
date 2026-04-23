#pragma once

#include "order_types.hpp"
#include <cstdint>
#include <cstring>

// Packed struct for network layout stability
// Fields order/type MUST match sender
struct __attribute__((packed)) WireMessage {
    uint64_t id;
    uint32_t user_id;
    uint64_t price;
    uint32_t quantity;
    uint8_t side;
    uint8_t type;
    uint8_t tif;
};

// Normalizes wire types into engine semantics
class Decoder {
public:
    static bool decode(const char* buffer, size_t len, Order& order, uint64_t maxPrice);
};