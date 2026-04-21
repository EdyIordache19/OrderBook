#pragma once

#include "order_types.hpp"
#include "orders_pool.hpp"
#include "ring_buffer.hpp"

#include <vector>
#include <cstdint>
#include <iostream>

#define MAX_PRICE 10000

/**
 * @brief Core matching data structure, holds all orders
 *  - price-time priority: FIFO within a price level via intrusive list
 *  - bidOrders/askOrders are vectors indexed by integer price level
 *  - maxBid and minAsk track best bid / ask indices to avoid searching whole book
 *  - ordersPool for fast access to orders and preallocation; avoids allocation overhead
 *  - orderLookup vector to have O(1) access to orders (cancel by id), while O(MAX_PRICE) for memory
 */
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
    uint64_t minAsk = MAX_PRICE;
public:
    RingBuffer<Trade>& matchBuffer;

    OrderBook(uint64_t numOfOrders, RingBuffer<Trade>& _matchBuffer);
    void addOrder(Order& order);
    void removeOrder(uint64_t orderId);
    void removeOrder(Order *orderToRemove);
    void matchOrders();
    uint32_t processOrder(Order& incoming, std::vector<Trade>& trades);
    bool canFill(Order& order);
    void printOrders(std::string filename);
    void printOrder(const Order *order, std::ostream& outFile);

    BookSnapshot getBookSnapshot();

    uint32_t getLevelQuantity(uint64_t price, Side side);

    uint64_t getMaxBid() {
        return maxBid;
    }
    uint64_t getMinAsk() {
        return minAsk;
    }
    uint32_t getActiveAsksCount() {
        return activeAsksCount;
    }
    uint32_t getActiveBidsCount() {
        return activeBidsCount;
    }

    bool isEmpty() {
        return activeAsksCount == 0 && activeBidsCount == 0;
    }
};
