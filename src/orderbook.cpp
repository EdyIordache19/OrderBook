#include "orderbook.hpp"

OrderBook::OrderBook() {
    bidOrders.resize(NUM_ORDERS);
    askOrders.resize(NUM_ORDERS);

    orderLookup.resize(NUM_ORDERS + 1);
}

void OrderBook::addOrder(Order& order) {
    Order* orderPtr = ordersPool.allocateOrder();
    *orderPtr = order;
    orderPtr->next = nullptr;
    orderPtr->prev = nullptr;

    orderLookup[order.id] = orderPtr;

    std::vector<Level>& book = order.side == Order::BUY ? bidOrders : askOrders;

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

    if (order.side == Order::BUY) {
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
    std::vector<Level>& book = orderToRemove->side == Order::BUY ? bidOrders : askOrders;

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
    std::vector<Level>& book = orderToRemove->side == Order::BUY ? bidOrders : askOrders;

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

uint32_t OrderBook::processOrder(Order& incoming) {
    while (incoming.quantity > 0) {
        if (incoming.side == Order::BUY) {
            if (minAsk > incoming.price) break;
            if (askOrders[minAsk].head == nullptr) {
                minAsk++;
                if (minAsk > askOrders.size()) break;
                continue;
            }
        } else {
            if (maxBid < incoming.price) break;
            if (bidOrders[maxBid].head == nullptr) {
                if (maxBid == 0) break;
                maxBid--;
                continue;
            }
        }

        Level& level = (incoming.side == Order::BUY) ? askOrders[minAsk] : bidOrders[maxBid];
        Order *order = level.head;

        uint32_t tradeQuantity = std::min(incoming.quantity, order->quantity);

        incoming.quantity -= tradeQuantity;
        order->quantity -= tradeQuantity;

        if (order->quantity == 0) {
            level.head = order->next;
            if (level.head) level.head->prev = nullptr;
            else level.tail = nullptr;

            orderLookup[order->id] = nullptr;
            ordersPool.deallocateOrder(order);
        }
    }

    return incoming.quantity;
}

bool OrderBook::canFill(Order& incoming) {
    Order incoming_copy = incoming;

    uint64_t currentPrice = incoming.side == Order::BUY ? minAsk : maxBid;

    Order *currentOrder = nullptr;
    uint32_t currentQty = 0;

    while (incoming_copy.quantity > 0) {
        while (!currentOrder) {
            if (incoming.side == Order::BUY) {
                // If price is too big return false (whole BUY order not fulfilled)
                if (currentPrice > incoming.price || currentPrice > askOrders.size()) return false;
                currentOrder = askOrders[currentPrice].head;

                // If current price level empty, move price up
                if (!currentOrder) currentPrice++;
            } else {
                // If price is too small return false (whole SELL order not fulfilled)
                if (currentPrice < incoming.price || currentPrice == 0) return false;
                currentOrder = bidOrders[currentPrice].head;

                // If current price level empty, move price down
                if (!currentOrder) currentPrice--;
            }

            if (currentOrder) {
                currentQty = currentOrder->quantity;
            }
        }

        if (!currentOrder) return false;

        uint32_t tradeQty = std::min(incoming_copy.quantity, currentQty);
        incoming_copy.quantity -= tradeQty;
        currentQty -= tradeQty;

        if (currentQty == 0) {
            // Current trade is empty, move
            currentOrder = currentOrder->next;
            if (currentOrder) {
                currentQty = currentOrder->quantity;
            } else {
                if (incoming.side == Order::BUY) {
                    currentPrice++;
                } else {
                    currentPrice--;
                }
            }
        }
    }

    return true;
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
