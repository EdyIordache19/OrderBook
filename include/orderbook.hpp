#pragma once

#include "order_types.hpp"
#include "orders_pool.hpp"

#include <vector>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <list>

#define NUM_ORDERS 1000000

class OrderBook {
private:
    OrdersPool ordersPool{NUM_ORDERS};

    std::vector<Level> askOrders;
    std::vector<Level> bidOrders;

    std::vector<Order*> orderLookup;

    uint32_t activeBidsCount = 0;
    uint32_t activeAsksCount = 0;

    uint64_t maxBid = 0;
    uint64_t minAsk = 9999999;
public:
    OrderBook();
    void addOrder(Order& order);
    void removeOrder(uint64_t orderId);
    void removeOrder(Order *orderToRemove);
    void matchOrders();
    uint32_t processOrder(Order& incoming);
    bool canFill(Order& order);
    void printOrders(char *filename);
};
