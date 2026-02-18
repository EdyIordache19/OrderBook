#include "publisher.hpp"
#include "main.hpp"

Publisher::Publisher(RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename)
    : matchBuffer(_matchBuffer),
      running(_running),
      filename(_filename)
    { }

void Publisher::run() {
    pin_thread_to_core(3);

    std::ofstream outFile(filename);

    Trade trade;
    while (running) {
        if (!matchBuffer.is_empty()) {
            matchBuffer.pop(trade);

            outFile << "Maker ID: " << trade.maker_id << ", ";
            outFile << "Taker ID: " << trade.taker_id << ", ";
            outFile << "Price: " << trade.price << ", ";
            outFile << "Quantity: " << trade.quantity << "\n";
        }
    }
}
