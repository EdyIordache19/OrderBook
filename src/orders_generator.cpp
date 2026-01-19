#include "orders_generator.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

Order OrdersGenerator::generateRandomOrder(uint64_t id) {
    Order order;
    order.id = id;
    order.price = 90 + rand() % 21; // Random price between 90 and 110
    order.quantity = 1 + rand() % 100; // Random quantity between 1 and 100
    order.side = (rand() % 2 == 0) ? Order::BUY : Order::SELL;
    return order;
}

std::list<Order> OrdersGenerator::generateOrdersToFile(const std::string& filename, size_t numOrders) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return {};
    }

    std::list<Order> generatedOrders;
    for (size_t i = 0; i < numOrders; ++i) {
        Order order = generateRandomOrder(i + 1);
        outFile << order.id << " " << order.price << " " << order.quantity << " "
                << ((order.side == Order::BUY) ? "BUY" : "SELL") << "\n";
        generatedOrders.push_back(order);
    }

    outFile.close();

    return generatedOrders;
}