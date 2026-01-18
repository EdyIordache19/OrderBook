#include "orderbook.hpp"

void OrderBook::addOrder(const Order& order) {
    Order* orderPtr = ordersPool.allocateOrder();
    *orderPtr = order;

    if (order.orderType == Order::BUY) {
        bidOrders[order.price].push_back(orderPtr);
        orderLookup[order.id] = --bidOrders[order.price].end();
    } else {
        askOrders[order.price].push_back(orderPtr);
        orderLookup[order.id] = --askOrders[order.price].end();
    }
}

void OrderBook::removeOrder(uint64_t orderId) {
    // Iterator to find the order in the lookup map
    auto lookupIt = orderLookup.find(orderId);
    if (lookupIt == orderLookup.end()) {
        // Order ID not found
        return;
    }

    // Retrieve the order details
    Order* orderToRemovePtr = *(lookupIt->second);

    ordersPool.deallocateOrder(orderToRemovePtr);
    if (orderToRemovePtr->orderType == Order::BUY) {
        // Remove from bidOrders
        auto& orders = bidOrders[orderToRemovePtr->price];
        orders.erase(lookupIt->second);
        if (orders.empty()) {
            // If no more orders at this price, remove the price level
            bidOrders.erase(orderToRemovePtr->price);
        }

        // Remove from lookup map
        orderLookup.erase(lookupIt);
    } else {
        // Remove from askOrders
        auto& orders = askOrders[orderToRemovePtr->price];
        orders.erase(lookupIt->second);
        if (orders.empty()) {
            // If no more orders at this price, remove the price level
            askOrders.erase(orderToRemovePtr->price);
        }

        // Remove from lookup map
        orderLookup.erase(lookupIt);
    }
}

void OrderBook::printOrders(char *filename) {
    std::ofstream outFile(filename);

    outFile << "Sell Orders:\n";
    for (const auto& [price, orders] : askOrders) {
        for (const auto& order : orders) {
            outFile << "ID: " << order->id << ", Price: " << order->price << ", Quantity: " << order->quantity << "\n";
        }
    }

    outFile << "----------------------------------\n";

    outFile << "Buy Orders:\n";
    for (const auto& [price, orders] : bidOrders) {
        for (const auto& order : orders) {
            outFile << "ID: " << order->id << ", Price: " << order->price << ", Quantity: " << order->quantity << "\n";
        }
    }
}

std::vector<Trade> OrderBook::matchOrders() {
    std::vector<Trade> trades;
    trades.reserve(10);
    while (!bidOrders.empty() && !askOrders.empty()) {
        auto highestBidIt = bidOrders.begin();
        auto lowestAskIt = askOrders.begin();

        if (highestBidIt->first >= lowestAskIt->first) {
            auto& bidList = highestBidIt->second;
            auto& askList = lowestAskIt->second;

            auto& bidOrder = bidList.front();
            auto& askOrder = askList.front();

            uint32_t tradeQuantity = std::min(bidOrder->quantity, askOrder->quantity);

            bidOrder->quantity -= tradeQuantity;
            askOrder->quantity -= tradeQuantity;

            Trade trade = {lowestAskIt->first, tradeQuantity};
            trades.push_back(trade);

            if (bidOrder->quantity == 0) {
                ordersPool.deallocateOrder(bidOrder);
                bidList.pop_front();
                if (bidList.empty()) {
                    bidOrders.erase(highestBidIt);
                }
            }

            if (askOrder->quantity == 0) {
                ordersPool.deallocateOrder(askOrder);
                askList.pop_front();
                if (askList.empty()) {
                    askOrders.erase(lowestAskIt);
                }
            }
        } else {
            break;
        }
    }
    return trades;
}