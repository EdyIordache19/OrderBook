#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>

#include "order_types.hpp"
#include "ring_buffer.hpp"
#include "orderbook.hpp"

/**
 * @brief Network first class that sends different packets via UDP multicast
 *  - UDP multicast on 239.0.0.1:5000
 *  - consumer of SPSC ring buffer matchBuffer (engine -> publisher)
 *  - publishes snapshot / ledger / trade / candle every 50ms
 *  - does not guarantee to publish every trade
 */
class Publisher {
private:
    OrderBook& book;
    RingBuffer<Trade>& matchBuffer;
    std::atomic<bool>& running;
    std::string filename;

    std::atomic<int64_t>& usd_balance;
    std::atomic<int64_t>& equity_balance;

    void send_snapshot(int sockfd, sockaddr_in servaddr);
    void send_candle(int sockfd, sockaddr_in servaddr, Candle current_candle);
    void send_trade(int sockfd, sockaddr_in servaddr, Trade current_trade);
    void send_account_info(int sockfd, sockaddr_in servaddr);
    void send_open_orders(int sockfd, sockaddr_in servaddr);
public:
    Publisher(OrderBook& _book, RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename,
        std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance);
    void run();
};