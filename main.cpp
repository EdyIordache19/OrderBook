#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"

int main(int argc, char* argv[]) {
    OrderBook orderBook;

    // Generate random orders and write to file
    std::string ordersFile = argv[1];

    std::list<Order> orders = OrdersGenerator::generateOrdersToFile(ordersFile, 10);

    // Add orders to the order book
    for (const auto& order : orders) {
        orderBook.addOrder(order);
    }

    // Print current orders
    orderBook.printOrders();

    // Match orders
    std::cout << "Matching orders...\n";
    std::list<Trade> trades = orderBook.matchOrders();
    for (const auto& trade : trades) {
        std::cout << "Trade executed: " << trade.quantity << " units at price " << trade.price << "\n";
    }

    // Print remaining orders
    orderBook.printOrders();
    return 0;
}