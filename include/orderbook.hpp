#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <list>

struct Trade {
    uint64_t price;
    uint32_t quantity;
};

struct Order {
    uint64_t id;
    uint64_t price;
    uint32_t quantity;
    enum type { BUY, SELL } orderType;
};

class OrderBook {
private:
    std::map<uint64_t, std::list<Order>, std::greater<uint64_t>> bidOrders;
    std::map<uint64_t, std::list<Order>> askOrders;

    //  Lookup map, stores orderId for faster order removal
    std::map<uint64_t, std::list<Order>::iterator> orderLookup;
public:
    void addOrder(const Order& order);
    void removeOrder(uint64_t orderId);
    void printOrders();
    std::list<Trade> matchOrders();
};
