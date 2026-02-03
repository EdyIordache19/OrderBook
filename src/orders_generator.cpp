#include "orders_generator.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

Order OrdersGenerator::generateRandomOrder(uint64_t id) {
    Order order;
    order.id = id;
    order.price = 10 + rand() % 101; // Random price between 10 and 110
    order.quantity = 1 + rand() % 1000; // Random quantity between 1 and 1000
    order.side = (rand() % 2 == 0) ? Side::BUY : Side::SELL;
    order.tif = static_cast<TimeInForce>(rand() % 3);
    order.type = static_cast<OrderType>(rand() % 2);
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
                << ((order.side == Side::BUY) ? "BUY" : "SELL") << " "
                << static_cast<int>(order.tif) << " "
                << static_cast<int>(order.type) << "\n";
        generatedOrders.push_back(order);
    }

    outFile.close();

    return generatedOrders;
}