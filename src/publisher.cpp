#include "publisher.hpp"
#include "main.hpp"
#include "order_types.hpp"
#include "ring_buffer.hpp"

#include <cstdint>
#include <iostream>
#include <fstream>
#include <chrono>
#include <netinet/in.h>
#include <thread>

Publisher::Publisher(OrderBook& _book,
    RingBuffer<Trade>& _matchBuffer, RingBuffer<OrderHistory>& _historyBuffer,
    std::atomic<bool>& _running, std::string _filename,
    std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance)
    : book(_book),
      matchBuffer(_matchBuffer),
      historyBuffer(_historyBuffer),
      running(_running),
      filename(_filename),
      usd_balance(_usd_balance),
      equity_balance(_equity_balance)
    { }


/**
 * @brief Used for debugging, currently legacy
 */
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

/**
 * @brief Updates OHLCV candle from trades observed since last publish tick
 *  - candles reset once sent (every 50ms)
 */
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

/**
 * Send_X methods, for sending specific information
 *  - header size is sizeof(payload), not total packet size
 *  - do not reorder payloads without updating bridge
 */

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

void Publisher::send_trade(int sockfd, sockaddr_in servaddr, Trade current_trade) {
    TradePacket packet;
    packet.header.type = MsgType::MSG_TRADE;
    packet.header.size = sizeof(Trade);

    packet.payload = current_trade;

    sendto(sockfd, &packet, sizeof(TradePacket), 0, (const sockaddr *)&servaddr, sizeof(servaddr));
}

void Publisher::send_account_info(int sockfd, sockaddr_in servaddr) {
    AccountPacket packet;
    packet.header.type = MsgType::MSG_ACCOUNT_INFO;
    packet.header.size = sizeof(AccountInfo);

    packet.payload.equity = equity_balance;
    packet.payload.usd = usd_balance;

    sendto(sockfd, &packet, sizeof(AccountPacket), 0, (const sockaddr *)&servaddr, sizeof(servaddr));
}

void Publisher::send_open_orders(int sockfd, sockaddr_in servaddr) {
    OpenOrdersPacket packet;
    packet.header.type = MsgType::MSG_OPEN_ORDERS;
    packet.header.size = sizeof(OpenOrders);

    packet.payload.count = 0;
    for (Order* order : book.getOrderLookup()) {
        if (packet.payload.count >= 10) break;
        if (order != nullptr && order->user_id == 1) {
            // Populate one row in open orders table with order info
            OpenOrderRow order_row;
            order_row.id = order->id;
            order_row.price = order->price;
            order_row.quantity = order->quantity;
            order_row.side = order->side;
            order_row.type = order->type;
            order_row.timestamp = order->timestamp;
            if (order->initial_quantity == 0) {
                order_row.filled_pct = 0;
            } else {
                uint64_t filled_pct = 100 - (order->quantity * 100 / order->initial_quantity);
                order_row.filled_pct = filled_pct > 100 ? 100 : filled_pct;
            }
            // Add this row to the array
            packet.payload.open_orders[packet.payload.count++] = order_row;
        }
    }

    sendto(sockfd, &packet, sizeof(OpenOrdersPacket), 0, (const sockaddr *)&servaddr, sizeof(servaddr));
}

void Publisher::run() {
    pin_thread_to_core(3);

    // Open the UDP multicast socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(sockaddr_in));

    servaddr.sin_port = htons(5000);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("239.0.0.1");

    std::ofstream outFile(filename);

    Trade trade;

    // Fixed rate publishing for UI; decouples render cadence from engine match throughput
    auto interval = std::chrono::milliseconds(50);
    auto next_send_time = std::chrono::high_resolution_clock::now();

    // Handle timestamps of orders
    time_t timestamp;
    time(&timestamp);
    uint64_t logical_time = (uint64_t)timestamp;
    logical_time += 60*60*2; // From UTC to UTC+2

    // Read last time from file
    std::ifstream time_in(".last_time.txt");
    if (time_in.good()) {
        time_in >> logical_time;
    }
    time_in.close();

    Candle current_candle;
    while (running) {
        auto now = std::chrono::high_resolution_clock::now();
        // Best effort check, can race
        if (!matchBuffer.is_empty()) {
            matchBuffer.pop(trade);

            update_candle(trade, current_candle);
        } else {
            std::this_thread::yield();
        }

        if (now >= next_send_time) {
            // Send to UI once every 50 ms
            // UI gets latest trade info per tick, not every trade
            send_snapshot(sockfd, servaddr);
            send_account_info(sockfd, servaddr);
            send_trade(sockfd, servaddr, trade);
            send_open_orders(sockfd, servaddr);

            if (current_candle.is_active) {
                current_candle.simulated_time = logical_time;
                logical_time++;

                send_candle(sockfd, servaddr, current_candle);
                // Reset current candle
                current_candle = Candle();
            }

            next_send_time += interval;
        }
    }

    // Store last time in file
    std::ofstream time_out(".last_time.txt");
    if (time_out.good()) {
        time_out << logical_time;
    }
    time_out.close();

    close(sockfd);
}
