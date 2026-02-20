#include <iostream>
#include <fstream>

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include "orderbook.hpp"

class Publisher {
private:
    OrderBook& book;
    RingBuffer<Trade>& matchBuffer;
    std::atomic<bool>& running;
    std::string filename;
public:
    Publisher(OrderBook& _book, RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename);
    void run();
};