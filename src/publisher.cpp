#include "publisher.hpp"
#include "main.hpp"
#include "gateway.hpp"

#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


Publisher::Publisher(OrderBook& _book, RingBuffer<Trade>& _matchBuffer, std::atomic<bool>& _running, std::string _filename)
    : book(_book),
      matchBuffer(_matchBuffer),
      running(_running),
      filename(_filename)
    { }


void printSnapshot(std::ofstream& outFile, BookSnapshot snapshot) {
    outFile << "BIDS: \n";
    for (int i = 0; i < snapshot.num_bids; i++) {
        outFile << "LEVEL " << i << ":   ";
        outFile << "PRICE: " << snapshot.bids[i].price << "     ";
        outFile << "QTY: " << snapshot.bids[i].quantity << "\n";
    }

    outFile << "\n";

    outFile << "ASKS: \n";
    for (int i = 0; i < snapshot.num_asks; i++) {
        outFile << "LEVEL " << i << ":   ";
        outFile << "PRICE: " << snapshot.asks[i].price << "     ";
        outFile << "QTY: " << snapshot.asks[i].quantity << "\n";
    }

    outFile << "\n";

    for (int i = 0; i < 30; i++) {
        outFile << "--";
    }
    outFile << "\n\n";

}

void Publisher::run() {
    pin_thread_to_core(3);

    /**
     * Open the UDP multicast socket
     */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(sockaddr_in));

    servaddr.sin_port = htons(5000);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("239.0.0.1");

    std::ofstream outFile(filename);

    Trade trade;
    auto interval = std::chrono::milliseconds(50);
    auto next_send_time = std::chrono::high_resolution_clock::now();
    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        if (now >= next_send_time) {
            BookSnapshot snapshot = book.getBookSnapshot();

            SnapshotPacket packet;
            packet.header.type = MsgType::MSG_BOOK_SNAPSHOT;
            packet.header.size = sizeof(BookSnapshot);

            packet.payload = snapshot;
            sendto(sockfd, &packet, sizeof(SnapshotPacket) , 0, (const sockaddr *)&servaddr, sizeof(servaddr));

            next_send_time += interval;
        }

        if (!matchBuffer.is_empty()) {
            matchBuffer.pop(trade);

            TradePacket packet;
            packet.header.type = MsgType::MSG_TRADE;
            packet.header.size = sizeof(Trade);

            packet.payload = trade;

            // BookSnapshot snapshot = book.getBookSnapshot();
            // printSnapshot(outFile, snapshot);

            // outFile << "Maker ID: " << trade.maker_id << ", ";
            // outFile << "Taker ID: " << trade.taker_id << ", ";
            // outFile << "Price: " << trade.price << ", ";
            // outFile << "Quantity: " << trade.quantity << "\n";

            sendto(sockfd, &packet, sizeof(TradePacket) , 0, (const sockaddr *)&servaddr, sizeof(servaddr));
        } else {
            std::this_thread::yield();
        }
    }

    close(sockfd);
}
