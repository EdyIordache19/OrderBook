#include "orderbook.hpp"

OrderBook::OrderBook() {
    bidOrders.resize(NUM_ORDERS);
    askOrders.resize(NUM_ORDERS);

    orderLookup.resize(NUM_ORDERS + 1);
}

void OrderBook::addOrder(const Order& order) {
    Order* orderPtr = ordersPool.allocateOrder();
    *orderPtr = order;
    orderPtr->next = nullptr;
    orderPtr->prev = nullptr;

    orderLookup[order.id] = orderPtr;

    std::vector<Level>& book = order.orderType == Order::BUY ? bidOrders : askOrders;

    Level& level = book[order.price];
    if (level.head == nullptr) {
        level.head = orderPtr;
        level.tail = orderPtr;
    } else {
        // Add to the end of the list
        level.tail->next = orderPtr;
        orderPtr->prev = level.tail;
        level.tail = orderPtr;
    }

    if (order.orderType == Order::BUY) {
        maxBid = std::max(maxBid, order.price);
    } else {
        minAsk = std::min(minAsk, order.price);
    }

}

void OrderBook::removeOrder(uint64_t orderId) {
    if (orderId >= orderLookup.size() || !orderLookup[orderId]) {
        return;
    }

    Order* orderToRemove = orderLookup[orderId];
    std::vector<Level>& book = orderToRemove->orderType == Order::BUY ? bidOrders : askOrders;

    if (orderToRemove->prev) {
        orderToRemove->prev->next = orderToRemove->next;
    } else {
        // It's the head (highest priority)
        book[orderToRemove->price].head = orderToRemove->next;
    }

    if (orderToRemove->next) {
        orderToRemove->next->prev = orderToRemove->prev;
    } else {
        // It's the tail (lowest priority)
        book[orderToRemove->price].tail = orderToRemove->prev;
    }

    orderLookup[orderId] = nullptr;
    ordersPool.deallocateOrder(orderToRemove);
}

void OrderBook::removeOrder(Order *orderToRemove) {
    std::vector<Level>& book = orderToRemove->orderType == Order::BUY ? bidOrders : askOrders;

    if (orderToRemove->prev) {
        orderToRemove->prev->next = orderToRemove->next;
    } else {
        // It's the head (highest priority)
        book[orderToRemove->price].head = orderToRemove->next;
    }

    if (orderToRemove->next) {
        orderToRemove->next->prev = orderToRemove->prev;
    } else {
        // It's the tail (lowest priority)
        book[orderToRemove->price].tail = orderToRemove->prev;
    }

    orderLookup[orderToRemove->id] = nullptr;
    ordersPool.deallocateOrder(orderToRemove);
}

void OrderBook::matchOrders() {
    while (maxBid >= minAsk) {
        Level& bidLevel = bidOrders[maxBid];
        Level& askLevel = askOrders[minAsk];

        if (!bidLevel.head) {
            if (maxBid == 0) {
                break;
            }

            maxBid--;
            continue;
        }

        if (!askLevel.head) {
            if (minAsk >= askOrders.size()) {
                break;
            }

            minAsk++;
            continue;
        }

        Order *bidOrder = bidLevel.head;
        Order *askOrder = askLevel.head;

        // Fetch next orders into L1 cache
        __builtin_prefetch(bidOrder->next);
        __builtin_prefetch(askOrder->next);

        if (bidOrder->price >= askOrder->price) {
            uint32_t tradeQuantity = std::min(bidOrder->quantity, askOrder->quantity);

            askOrder->quantity -= tradeQuantity;
            bidOrder->quantity -= tradeQuantity;

            if (bidOrder->quantity == 0) {
                bidLevel.head = bidOrder->next;
                if (bidLevel.head) bidLevel.head->prev = nullptr;
                else bidLevel.tail = nullptr;

                ordersPool.deallocateOrder(bidOrder);
            }

            if (askOrder->quantity == 0) {
                askLevel.head = askOrder->next;
                if (askLevel.head) askLevel.head->prev = nullptr;
                else askLevel.tail = nullptr;

                ordersPool.deallocateOrder(askOrder);
            }
        } else {
            break;
        }
    }
}


void OrderBook::printOrders(char *filename) {
    std::ofstream outFile(filename);

    outFile << "Sell Orders:\n";

    for (Level level : askOrders) {
        Order* order = level.head;
        while (order) {
            outFile << "ID: " << order->id << ", Price: " << order->price << ", Quantity: " << order->quantity << "\n";
            order = order->next;
        }
    }

    outFile << "----------------------------------\n";

    outFile << "Buy Orders:\n";
    for (Level level : bidOrders) {
        Order* order = level.head;
        while (order) {
            outFile << "ID: " << order->id << ", Price: " << order->price << ", Quantity: " << order->quantity << "\n";
            order = order->next;
        }
    }
}
