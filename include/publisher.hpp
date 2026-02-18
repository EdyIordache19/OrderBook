#include <iostream>
#include <fstream>

#include "order_types.hpp"
#include "ring_buffer.hpp"

class Publisher {
private:
    RingBuffer<Trade>& matchBuffer;
    std::atomic<bool>& running;
    std::string filename;
public:
    Publisher(RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename);
    void run();
};