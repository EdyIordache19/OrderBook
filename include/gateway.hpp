#pragma once

#include "order_types.hpp"
#include <cstdint>

#define PORT 1234
#define MAXLINE 1024

struct WireMessage {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;

    OrderType type;
    TimeInForce tif;
};

class Gateway {
public:
    void run();
};