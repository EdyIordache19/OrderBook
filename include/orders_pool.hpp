#pragma once

#include "order_types.hpp"
#include <vector>

/**
 * @brief Used to preallocate orders memory to avoid new/delete and keep pointers stable
 *  - allocateOrder returns pointer to memory owned by the pool; caller must use deallocateOrder on it exactly once
 *  - not thread-safe, intended for single-threaded matching engine
 *  - no double-free checks (by design for speed)
 *  - fixed capacity, throws if exhausted
 *  - O(1) allocate/free
 *  - improves cache locality instead of scattered heap allocations
 */
class OrdersPool {
    private:
        std::vector<Order> ordersPool;
        std::vector<Order*> freeSlots;
    public:
        OrdersPool(size_t poolSize);
        Order* allocateOrder();
        void deallocateOrder(Order* orderPtr);
};
