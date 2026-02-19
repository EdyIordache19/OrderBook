#include "publisher.hpp"
#include "main.hpp"
#include "gateway.hpp"

#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


Publisher::Publisher(RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename)
    : matchBuffer(_matchBuffer),
      running(_running),
      filename(_filename)
    { }

void Publisher::run() {
    pin_thread_to_core(3);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(sockaddr_in));

    servaddr.sin_port = htons(5000);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("239.0.0.1");

    std::ofstream outFile(filename);

    Trade trade;
    while (running) {
        if (!matchBuffer.is_empty()) {
            matchBuffer.pop(trade);

            outFile << "Maker ID: " << trade.maker_id << ", ";
            outFile << "Taker ID: " << trade.taker_id << ", ";
            outFile << "Price: " << trade.price << ", ";
            outFile << "Quantity: " << trade.quantity << "\n";

            sendto(sockfd, &trade, sizeof(Trade), 0, (const sockaddr *)&servaddr, sizeof(servaddr));
        } else {
            std::this_thread::yield();
        }
    }

    close(sockfd);
}
