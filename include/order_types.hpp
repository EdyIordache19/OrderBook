#pragma once

#include <cstdint>

enum OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    CANCEL = 2,
    KILL = 99
};

enum TimeInForce : uint8_t {
    GTC = 0, // Good 'till Cancel
    IOC = 1, // Immediate or Cancel
    FOK = 2 // Fill or Kill
};

enum Side : uint8_t {
    BUY = 0,
    SELL = 1
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

    Side side;
    OrderType type;
    TimeInForce tif;

    Order *next = nullptr;
    Order *prev = nullptr;

    Order() { }

    Order(uint64_t _id, uint64_t _price, uint32_t _quantity, Side _side, OrderType _type) :
        id(_id),
        price(_price),
        quantity(_quantity),
        side(_side),
        type(_type)
            { }
};


struct Level {
    // First order (highest priority)
    Order *head = nullptr;

    // Last order (lowest priority)
    Order *tail = nullptr;
};