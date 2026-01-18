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
    OrdersPool ordersPool{100000};

    std::map<uint64_t, std::list<Order*>, std::greater<uint64_t>> bidOrders;
    std::map<uint64_t, std::list<Order*>> askOrders;

    //  Lookup map, stores orderId for faster order removal
    std::map<uint64_t, std::list<Order*>::iterator> orderLookup;
public:
    void addOrder(const Order& order);
    void removeOrder(uint64_t orderId);
    void printOrders(char *filename);
    std::vector<Trade> matchOrders();
};
