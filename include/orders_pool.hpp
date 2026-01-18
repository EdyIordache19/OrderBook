#pragma once

#include "order_types.hpp"
#include <vector>

class OrdersPool {
    private:
        std::vector<Order> ordersPool;
        std::vector<Order*> freeSlots;
    public:
        OrdersPool(size_t poolSize);
        Order* allocateOrder();
        void deallocateOrder(Order* orderPtr);
};
