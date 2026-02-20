#include "orderbook.hpp"

OrderBook::OrderBook(uint64_t numOfOrders, RingBuffer<Trade>& _matchBuffer)
    : numOrders(numOfOrders),
      matchBuffer(_matchBuffer) {
    bidOrders.resize(MAX_PRICE);
    askOrders.resize(MAX_PRICE);

    orderLookup.resize(numOfOrders + 1);
}

void OrderBook::addOrder(Order& order) {
    Order* orderPtr = ordersPool.allocateOrder();
    *orderPtr = order;
    orderPtr->next = nullptr;
    orderPtr->prev = nullptr;

    orderLookup[order.id] = orderPtr;

    std::vector<Level>& book = order.side == Side::BUY ? bidOrders : askOrders;

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

    if (order.side == Side::BUY) {
        maxBid = std::max(maxBid, order.price);
        activeBidsCount++;
    } else {
        minAsk = std::min(minAsk, order.price);
        activeAsksCount++;
    }

}

void OrderBook::removeOrder(uint64_t orderId) {
    if (orderId >= orderLookup.size() || !orderLookup[orderId]) {
        return;
    }

    Order* orderToRemove = orderLookup[orderId];
    std::vector<Level>& book = orderToRemove->side == Side::BUY ? bidOrders : askOrders;

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

    if (orderToRemove->side == Side::BUY) {
        activeBidsCount--;
    } else {
        activeAsksCount--;
    }

    orderLookup[orderId] = nullptr;
    ordersPool.deallocateOrder(orderToRemove);
}

void OrderBook::removeOrder(Order *orderToRemove) {
    std::vector<Level>& book = orderToRemove->side == Side::BUY ? bidOrders : askOrders;

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
    if (incoming.type == OrderType::MARKET) {
        incoming.price = incoming.side == Side::BUY ? MAX_PRICE - 1 : 0;
    }

    if (incoming.price >= MAX_PRICE) {
        return incoming.quantity;
    }

    while (incoming.quantity > 0) {
        if (incoming.side == Side::BUY) {
            if (activeAsksCount == 0) break;

            if (minAsk > incoming.price) break;
            if (askOrders[minAsk].head == nullptr) {
                minAsk++;
                if (minAsk >= askOrders.size()) break;
                continue;
            }
        } else {
            if (activeBidsCount == 0) break;

            if (maxBid < incoming.price) break;
            if (bidOrders[maxBid].head == nullptr) {
                if (maxBid == 0) break;
                maxBid--;
                continue;
            }
        }

        Level& level = (incoming.side == Side::BUY) ? askOrders[minAsk] : bidOrders[maxBid];
        Order *order = level.head;

        if (order->quantity == 0) {
            removeOrder(order);
            continue;
        }

        uint32_t tradeQuantity = std::min(incoming.quantity, order->quantity);

        incoming.quantity -= tradeQuantity;
        order->quantity -= tradeQuantity;

        Trade trade(incoming.id, order->id, tradeQuantity, incoming.price, incoming.timestamp);
        matchBuffer.push(trade);

        if (order->quantity == 0) {
            level.head = order->next;
            if (level.head) level.head->prev = nullptr;
            else level.tail = nullptr;

            orderLookup[order->id] = nullptr;
            ordersPool.deallocateOrder(order);

            if (order->side == Side::BUY) {
                activeBidsCount--;
            } else {
                activeAsksCount--;
            }
        }
    }

    return incoming.quantity;
}

bool OrderBook::canFill(Order& incoming) {
    Order incoming_copy = incoming;

    uint64_t currentPrice = incoming.side == Side::BUY ? minAsk : maxBid;

    Order *currentOrder = nullptr;
    uint32_t currentQty = 0;

    while (incoming_copy.quantity > 0) {
        while (!currentOrder) {
            if (incoming.side == Side::BUY) {
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
                if (incoming.side == Side::BUY) {
                    currentPrice++;
                } else {
                    currentPrice--;
                }
            }
        }
    }

    return true;
}

void OrderBook::printOrders(std::string filename) {
    std::ofstream outFile(filename);

    outFile << "Sell Orders:\n";

    for (const Level& level : askOrders) {
        Order* order = level.head;
        while (order) {
            printOrder(order, outFile);
            order = order->next;
        }
    }

    outFile << "----------------------------------\n";

    outFile << "Buy Orders:\n";
    for (const Level& level : bidOrders) {
        Order* order = level.head;
        while (order) {
            printOrder(order, outFile);
            order = order->next;
        }
    }
}

void OrderBook::printOrder(const Order *order, std::ostream& outFile) {
    std::string type;
    if (order->type == OrderType::LIMIT) {
        type = "Limit";
    } else if (order->type == OrderType::MARKET) {
        type = "Market";
    } else {
        type = "KILL";
    }

    outFile << "ID: " << order->id << ", Price: " << order->price << ", Quantity: " << order->quantity
            << ", Type: " << type << "\n";
}

uint32_t OrderBook::getLevelQuantity(uint64_t price, Side side) {
    uint32_t num_of_orders = 0;
    Order *curr;

    if (side == Side::BUY) {
        if (price >= bidOrders.size() || bidOrders[price].head == nullptr) {
            return 0;
        }

        curr = bidOrders[price].head;
    } else {
        if (price >= askOrders.size() || askOrders[price].head == nullptr) {
            return 0;
        }

        curr = askOrders[price].head;
    }

    while (curr) {
        num_of_orders += curr->quantity;
        curr = curr->next;
    }

    return num_of_orders;
}

BookSnapshot OrderBook::getBookSnapshot() {
    BookSnapshot newSnapshot;

    uint64_t currentAsk = minAsk;

    uint8_t askLevels = 0;
    for (int i = 0; i < 10; i++) {
        if (currentAsk > askOrders.size()) break;

        Level askLevel = askOrders[currentAsk];
        Order *order = askLevel.head;
        if (order == nullptr) break;

        PriceLevel newPriceLevel(currentAsk, 0);
        while (order) {
            newPriceLevel.quantity += order->quantity;
            order = order->next;
        }

        newSnapshot.asks[i] = newPriceLevel;
        askLevels++;

        currentAsk++;
        while (currentAsk < askOrders.size() && askOrders[currentAsk].head == nullptr) {
            currentAsk++;
        }
    }
    newSnapshot.num_asks = askLevels;

    uint64_t currentBid = maxBid;
    uint8_t bidLevels = 0;
    for (int i = 0; i < 10; i++) {
        if (currentBid == 0) break;

        Level bidLevel = bidOrders[currentBid];
        Order *order = bidLevel.head;
        if (order == nullptr) break;

        PriceLevel newPriceLevel(currentBid, 0);
        while (order) {
            newPriceLevel.quantity += order->quantity;
            order = order->next;
        }

        newSnapshot.bids[i] = newPriceLevel;
        bidLevels++;

        currentBid--;
        while (currentBid > 0 && bidOrders[currentBid].head == nullptr) {
            currentBid--;
        }
    }

    newSnapshot.num_bids = bidLevels;

    return newSnapshot;
}
