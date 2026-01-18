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


class OrderBook {
private:
    OrdersPool ordersPool{10000000};

    std::vector<Level> askOrders;
    std::vector<Level> bidOrders;

    std::vector<Order*> orderLookup;

    uint64_t maxBid = 0;
    uint64_t minAsk = 9999999;
public:
    OrderBook();
    void addOrder(const Order& order);
    void removeOrder(uint64_t orderId);
    void removeOrder(Order *orderToRemove);
    void matchOrders();
    void printOrders(char *filename);
};
