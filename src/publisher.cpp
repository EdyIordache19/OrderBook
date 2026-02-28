#include "publisher.hpp"
#include "main.hpp"
#include "gateway.hpp"

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

void update_candle(Trade trade, Candle& current_candle) {
    if (!current_candle.is_active) {
        current_candle.open = trade.price;
        current_candle.is_active = true;
    }

    current_candle.high = std::max(current_candle.high, trade.price);
    current_candle.low = std::min(current_candle.low, trade.price);
    current_candle.close = trade.price;

    current_candle.volume += trade.quantity;
}

void Publisher::send_snapshot(int sockfd, sockaddr_in servaddr) {
    BookSnapshot snapshot = book.getBookSnapshot();

    SnapshotPacket packet;
    packet.header.type = MsgType::MSG_BOOK_SNAPSHOT;
    packet.header.size = sizeof(BookSnapshot);

    packet.payload = snapshot;
    sendto(sockfd, &packet, sizeof(SnapshotPacket), 0, (const sockaddr *)&servaddr, sizeof(servaddr));
}

void Publisher::send_candle(int sockfd, sockaddr_in servaddr, Candle current_candle) {
    CandlePacket packet;
    packet.header.type = MsgType::MSG_CANDLE;
    packet.header.size = sizeof(Candle);

    packet.payload = current_candle;

    sendto(sockfd, &packet, sizeof(CandlePacket), 0, (const sockaddr *)&servaddr, sizeof(servaddr));

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

    time_t timestamp;
    time(&timestamp);
    uint64_t logical_time = (uint64_t)timestamp;
    std::ifstream time_in(".last_time.txt");
    if (time_in.good()) {
        time_in >> logical_time;
    }
    time_in.close();

    Candle current_candle;
    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        if (!matchBuffer.is_empty()) {
            matchBuffer.pop(trade);

            update_candle(trade, current_candle);
        } else {
            std::this_thread::yield();
        }

        if (now >= next_send_time) {
            send_snapshot(sockfd, servaddr);

            if (current_candle.is_active) {
                current_candle.simulated_time = logical_time;
                logical_time++;

                send_candle(sockfd, servaddr, current_candle);
                current_candle = Candle();
            }

            next_send_time += interval;
        }
    }

    std::ofstream time_out(".last_time.txt");
    if (time_out.good()) {
        time_out << logical_time;
    }
    time_out.close();

    close(sockfd);
}
