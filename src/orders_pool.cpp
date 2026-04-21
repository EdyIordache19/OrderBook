#include "orders_pool.hpp"

#include <stdexcept>

OrdersPool::OrdersPool(size_t poolSize) {
    ordersPool.resize(poolSize);
    freeSlots.reserve(poolSize);

    for (size_t i = 0; i < poolSize; i++) {
        freeSlots.push_back(&ordersPool[i]);
    }
}

/**
 *  - LIFO free list improves cache warmth (recently freed nodes are likely still hot)
 */
Order* OrdersPool::allocateOrder() {
    if (!freeSlots.empty()) {
        Order* orderPtr = freeSlots.back();
        freeSlots.pop_back();
        return orderPtr;
    } else {
        throw std::runtime_error("No free slots available in OrdersPool");
    }
}

/**
 *  - does not reset order; caller should overwrite memory
 */
void OrdersPool::deallocateOrder(Order *orderPtr) {
    freeSlots.push_back(orderPtr);
}