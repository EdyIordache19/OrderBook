#include "orderbook.hpp"

void OrderBook::addOrder(const Order& order) {
    if (order.orderType == Order::BUY) {
        bidOrders[order.price].push_back(order);
        orderLookup[order.id] = --bidOrders[order.price].end();
    } else {
        askOrders[order.price].push_back(order);
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
    Order orderToRemove = *(lookupIt->second);
    if (orderToRemove.orderType == Order::BUY) {
        // Remove from bidOrders
        auto& orders = bidOrders[orderToRemove.price];
        orders.erase(lookupIt->second);
        if (orders.empty()) {
            // If no more orders at this price, remove the price level
            bidOrders.erase(orderToRemove.price);
        }

        // Remove from lookup map
        orderLookup.erase(lookupIt);
    } else {
        // Remove from askOrders
        auto& orders = askOrders[orderToRemove.price];
        orders.erase(lookupIt->second);
        if (orders.empty()) {
            // If no more orders at this price, remove the price level
            askOrders.erase(orderToRemove.price);
        }

        // Remove from lookup map
        orderLookup.erase(lookupIt);
    }
}

void OrderBook::printOrders() {
    std::cout << "Sell Orders:\n";
    for (const auto& [price, orders] : askOrders) {
        for (const auto& order : orders) {
            std::cout << "ID: " << order.id << ", Price: " << order.price << ", Quantity: " << order.quantity << "\n";
        }
    }

    std::cout << "-------------------------------------------------\n";

    std::cout << "Buy Orders:\n";
    for (const auto& [price, orders] : bidOrders) {
        for (const auto& order : orders) {
            std::cout << "ID: " << order.id << ", Price: " << order.price << ", Quantity: " << order.quantity << "\n";
        }
    }
}

std::list<Trade> OrderBook::matchOrders() {
    std::list<Trade> trades;
    while (!bidOrders.empty() && !askOrders.empty()) {
        auto highestBidIt = bidOrders.begin();
        auto lowestAskIt = askOrders.begin();

        if (highestBidIt->first >= lowestAskIt->first) {
            auto& bidList = highestBidIt->second;
            auto& askList = lowestAskIt->second;

            auto& bidOrder = bidList.front();
            auto& askOrder = askList.front();

            uint32_t tradeQuantity = std::min(bidOrder.quantity, askOrder.quantity);

            bidOrder.quantity -= tradeQuantity;
            askOrder.quantity -= tradeQuantity;

            Trade trade = {lowestAskIt->first, tradeQuantity};
            trades.push_back(trade);

            if (bidOrder.quantity == 0) {
                bidList.pop_front();
                if (bidList.empty()) {
                    bidOrders.erase(highestBidIt);
                }
            }

            if (askOrder.quantity == 0) {
                askList.pop_front();
                if (askList.empty()) {
                    askOrders.erase(lowestAskIt);
                }
            }
        } else {
            std::cout << "No more matches possible. Printing current order book:\n";
            printOrders();
            break;
        }
    }
    return trades;
}