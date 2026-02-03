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
    uint64_t numOrders;
    OrdersPool ordersPool{numOrders*2};

    std::vector<Level> askOrders;
    std::vector<Level> bidOrders;

    std::vector<Order*> orderLookup;

    uint32_t activeBidsCount = 0;
    uint32_t activeAsksCount = 0;

    uint64_t maxBid = 0;
    uint64_t minAsk = 9999999;
public:
    OrderBook(uint64_t numOfOrders);
    void addOrder(Order& order);
    void removeOrder(uint64_t orderId);
    void removeOrder(Order *orderToRemove);
    void matchOrders();
    uint32_t processOrder(Order& incoming);
    bool canFill(Order& order);
    void printOrders(std::string filename);
    void printOrder(const Order *order, std::ostream& outFile);

    uint32_t getLevelQuantity(uint64_t price, Side side);

    uint64_t getMaxBid() {
        return maxBid;
    }
    uint64_t getMinAsk() {
        return minAsk;
    }
    bool isEmpty() {
        return activeAsksCount == 0 && activeBidsCount == 0;
    }
};
