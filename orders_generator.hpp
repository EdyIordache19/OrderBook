#pragma once

#include "orderbook.hpp"
#include <list>
#include <string>

class OrdersGenerator {
public:
    static Order generateRandomOrder(uint64_t id);
    static std::list<Order> generateOrdersToFile(const std::string& filename, size_t numOrders);
};