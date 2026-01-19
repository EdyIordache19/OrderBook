#pragma once

#include <cstdint>

enum OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1
};

enum TimeInForce : uint8_t {
    GTC = 0, // Good 'till Cancel
    IOC = 1, // Immediate or Cancel
    FOK = 2 // Fill or Kill
};

struct Trade {
    uint64_t price;
    uint32_t quantity;
};

struct alignas(64) Order {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;

    enum side : uint8_t { BUY, SELL } side;
    OrderType type;
    TimeInForce tif;

    Order *next = nullptr;
    Order *prev = nullptr;
};

struct Level {
    // First order (highest priority)
    Order *head = nullptr;

    // Last order (lowest priority)
    Order *tail = nullptr;
};