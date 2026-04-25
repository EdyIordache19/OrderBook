#include "orderbook.hpp"
#include "order_types.hpp"
#include "ring_buffer.hpp"

#include <cstdint>
#include <fstream>
#include <sys/types.h>

OrderBook::OrderBook(uint64_t numOfOrders, RingBuffer<Trade>& _matchBuffer, RingBuffer<OrderHistory>& _historyBuffer)
    : numOrders(numOfOrders),
      matchBuffer(_matchBuffer),
      historyBuffer(_historyBuffer) {
    bidOrders.resize(MAX_PRICE);
    askOrders.resize(MAX_PRICE);

    orderLookup.resize(numOfOrders + 1);
}

void OrderBook::addOrder(Order& order) {
    // Allocate node from pool and copy the incoming order
    Order* orderPtr = ordersPool.allocateOrder();
    *orderPtr = order;
    orderPtr->next = nullptr;
    orderPtr->prev = nullptr;

    // Update orderLookup for O(1) cancel by id
    orderLookup[order.id] = orderPtr;

    std::vector<Level>& book = order.side == Side::BUY ? bidOrders : askOrders;

    // Append to price level tail to preserve FIFO
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

    // Update best prices and counts
    if (order.side == Side::BUY) {
        maxBid = std::max(maxBid, order.price);
        activeBidsCount++;
    } else {
        minAsk = std::min(minAsk, order.price);
        activeAsksCount++;
    }

}

/**
 * @brief Remove order by id
 *  - used by Engine class when cancelling orders based on id
 */
void OrderBook::removeOrder(uint64_t orderId) {
    if (orderId >= orderLookup.size() || !orderLookup[orderId]) {
        return;
    }

    // O(1) order retrieval from lookup
    Order* orderToRemove = orderLookup[orderId];
    Level& level = orderToRemove->side == Side::BUY ?
        bidOrders[orderToRemove->price] :
        askOrders[orderToRemove->price];

    if (orderToRemove->prev) {
        orderToRemove->prev->next = orderToRemove->next;
    } else {
        // It's the head (highest priority)
        level.head = orderToRemove->next;
    }

    if (orderToRemove->next) {
        orderToRemove->next->prev = orderToRemove->prev;
    } else {
        // It's the tail (lowest priority)
        level.tail = orderToRemove->prev;
    }

    if (orderToRemove->side == Side::BUY) {
        activeBidsCount--;
        if (level.head == nullptr && orderToRemove->price == maxBid) {
            while (maxBid > 0 && bidOrders[maxBid].head == nullptr) maxBid--;
        }
    } else {
        activeAsksCount--;
        if (level.head == nullptr && orderToRemove->price == minAsk) {
            while (minAsk < askOrders.size() && askOrders[minAsk].head == nullptr) minAsk++;
        }
    }

    // Empty orderLookup and deallocate from ordersPool
    orderLookup[orderId] = nullptr;
    ordersPool.deallocateOrder(orderToRemove);
}

/**
 * @brief Remove order based on Order*
 *  - used by processOrder method when having pointer to order
 */
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

    // Empty orderLookup and deallocate from ordersPool
    orderLookup[orderToRemove->id] = nullptr;
    ordersPool.deallocateOrder(orderToRemove);
}

/**
 * @brief Legacy method, used to automatically match all remaining orders from book
 * Not needed anymore, since processOrder handles matching in-place
 */
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

            uint64_t bidPrice = bidOrder->price;
            uint64_t askPrice = askOrder->price;

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

            if (bidOrders[bidPrice].head == nullptr && bidPrice == maxBid) {
                while (maxBid > 0 && bidOrders[maxBid].head == nullptr) maxBid--;
            }
            if (askOrders[askPrice].head == nullptr && askPrice == minAsk) {
                while (minAsk < MAX_PRICE && askOrders[minAsk].head == nullptr) minAsk++;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief Handles an incoming order, matching it against the book
 *  - if there remains any quantity of order, Engine decides to rest it or not
 * @param trades Vector to update when a trade is made, for use in Engine
 * @return uint32_t Remaining quantity of order that got left on the book
 */
uint32_t OrderBook::processOrder(Order& incoming, std::vector<Trade>& trades) {
    // For market orders adjust the price into an aggressive limit order
    if (incoming.type == OrderType::MARKET) {
        incoming.price = incoming.side == Side::BUY ? MAX_PRICE - 1 : 0;
    }

    // Guard to keep the price less than MAX_PRICE
    if (incoming.price >= MAX_PRICE) {
        return incoming.quantity;
    }

    // Matching loop that consumes liquidity from the opposite side, starting at best price
    while (incoming.quantity > 0) {
        if (incoming.side == Side::BUY) {
            // BUY consumes asks from minAsk upward while minAsk <= limit
            if (activeAsksCount == 0) break;
            if (minAsk > incoming.price) break;

            // If minAsk price level got depleted, continue with the next price (minAsk++)
            if (askOrders[minAsk].head == nullptr) {
                minAsk++;
                if (minAsk >= askOrders.size()) break;
                continue;
            }
        } else {
            // SELL consumes bids from maxBid downward while maxBid >= limit
            if (activeBidsCount == 0) break;
            if (maxBid < incoming.price) break;

            // If maxBid price level got depleted, continue with next price (maxBid--);
            if (bidOrders[maxBid].head == nullptr) {
                if (maxBid == 0) break;
                maxBid--;
                continue;
            }
        }

        Level& level = (incoming.side == Side::BUY) ? askOrders[minAsk] : bidOrders[maxBid];
        Order *order = level.head;

        // Guard if level head happens to have empty order
        if (order->quantity == 0) {
            removeOrder(order);
            continue;
        }

        uint32_t tradeQuantity = std::min(incoming.quantity, order->quantity);

        incoming.quantity -= tradeQuantity;
        order->quantity -= tradeQuantity;

        // Build trade semantics
        // TODO: Swap maker with taker: incoming order is taker by convention
        Trade trade(incoming.id, order->id, incoming.user_id, order->user_id, order->price, tradeQuantity, incoming.timestamp);
        trades.push_back(trade);

        // If we matched a whole order sitting on the book, update the price level
        if (order->quantity == 0) {
            // If filled order is from user, push to history buffer
            if (order->user_id == 1) {
                uint64_t executed_price = incoming.price;
                if (incoming.type == OrderType::MARKET && !trades.empty()) {
                    uint64_t total_value = 0;
                    uint64_t total_qty = 0;

                    for (auto& trade : trades) {
                        if (trade.maker_id == incoming.id || trade.taker_id == incoming.id) {
                            total_value += trade.price * trade.quantity;
                            total_qty += trade.quantity;
                        }
                    }

                    if (total_qty > 0) {
                        executed_price = total_value / total_qty;
                    }
                }

                OrderHistory orderHistory(order->id, order->user_id,
                    order->timestamp, order->type, order->side, executed_price,
                    order->initial_quantity, HistoryStatus::FILLED);

                historyBuffer.push(orderHistory);
            }
            level.head = order->next;
            if (level.head) level.head->prev = nullptr;
            else level.tail = nullptr;

            if (order->side == Side::BUY) {
                activeBidsCount--;
            } else {
                activeAsksCount--;
            }

            orderLookup[order->id] = nullptr;
            ordersPool.deallocateOrder(order);

            if (level.head == nullptr) {
                if (incoming.side == Side::BUY) {
                    // We just depleted an Ask level
                    while (minAsk < askOrders.size() && askOrders[minAsk].head == nullptr) {
                        minAsk++;
                    }
                } else {
                    // We just depleted a Bid level
                    while (maxBid > 0 && bidOrders[maxBid].head == nullptr) {
                        maxBid--;
                    }
                }
            }
        }
    }

    if (incoming.quantity == 0) {
        if (incoming.user_id == 1) {
            uint64_t executed_price = incoming.price;
            if (incoming.type == OrderType::MARKET && !trades.empty()) {
                uint64_t total_value = 0;
                uint64_t total_qty = 0;

                for (auto& trade : trades) {
                    if (trade.maker_id == incoming.id || trade.taker_id == incoming.id) {
                        total_value += trade.price * trade.quantity;
                        total_qty += trade.quantity;
                    }
                }

                if (total_qty > 0) {
                    executed_price = total_value / total_qty;
                }
            }

            OrderHistory orderHistory(incoming.id,
                incoming.user_id,
                incoming.timestamp,
                incoming.type, incoming.side,
                executed_price,
                incoming.initial_quantity,
                HistoryStatus::FILLED);

            historyBuffer.push(orderHistory);
        }
    }
    return incoming.quantity;
}

/**
 * @brief Method to check if order can be filled completely for FOK orders
 *  - worst case runs through many price levels and order nodes (O(depth))
 */
bool OrderBook::canFill(Order& incoming) {
    Order incoming_copy = incoming;

    uint64_t currentPrice = incoming.side == Side::BUY ? minAsk : maxBid;

    Order *currentOrder = nullptr;
    uint32_t currentQty = 0;

    while (incoming_copy.quantity > 0) {
        // Loop to find order that can match incoming
        while (!currentOrder) {
            if (incoming.side == Side::BUY) {
                // If price is too big return false (whole BUY order not fulfilled)
                if (currentPrice > incoming.price || currentPrice >= askOrders.size()) return false;
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

/**
 * @brief Aggregates quantities per price level
 *  - O(number of orders in top levels), not O(1)
 * @return BookSnapshot contains top 10 levels for UI
 */
BookSnapshot OrderBook::getBookSnapshot() {
    BookSnapshot newSnapshot;

    uint64_t currentAsk = minAsk;

    uint8_t askLevels = 0;
    while (askLevels < 10 && currentAsk < askOrders.size()) {
        Order *order = askOrders[currentAsk].head;

        if (order != nullptr) {
            PriceLevel newPriceLevel(currentAsk, 0);
            while (order) {
                newPriceLevel.quantity += order->quantity;
                order = order->next;
            }

            if (newPriceLevel.quantity > 0) {
                newSnapshot.asks[askLevels] = newPriceLevel;
                askLevels++;
            }
        }

        currentAsk++;
    }
    newSnapshot.num_asks = askLevels;

    uint64_t currentBid = maxBid;
    uint8_t bidLevels = 0;
    while (bidLevels < 10 && currentBid > 0) {
        Order *order = bidOrders[currentBid].head;

        if (order != nullptr) {
            PriceLevel newPriceLevel(currentBid, 0);
            while (order) {
                newPriceLevel.quantity += order->quantity;
                order = order->next;
            }

            if (newPriceLevel.quantity > 0) {
                newSnapshot.bids[bidLevels] = newPriceLevel;
                bidLevels++;
            }
        }

        currentBid--;
    }
    newSnapshot.num_bids = bidLevels;

    return newSnapshot;
}
