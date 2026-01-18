#pragma once

#include <cstdint>

struct Trade {
    uint64_t price;
    uint32_t quantity;
};

struct Order {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
    enum type { BUY, SELL } orderType;

    Order *next = nullptr;
    Order *prev = nullptr;
};

struct Level {
    // First order (highest priority)
    Order *head = nullptr;

    // Last order (lowest priority)
    Order *tail = nullptr;
};