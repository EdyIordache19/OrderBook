#include <iostream>
#include <fstream>

#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include "orderbook.hpp"

class Publisher {
private:
    OrderBook& book;
    RingBuffer<Trade>& matchBuffer;
    std::atomic<bool>& running;
    std::string filename;

    void send_snapshot(int sockfd, sockaddr_in servaddr);
    void send_candle(int sockfd, sockaddr_in servaddr, Candle current_candle);
public:
    Publisher(OrderBook& _book, RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename);
    void run();
};