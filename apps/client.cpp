#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <random>
#include <atomic>
#include <thread>
#include <chrono>

#include "../include/orderbook.hpp"
#include "../include/order_types.hpp"
#include "../include/decoder.hpp"
#include "../include/gateway.hpp"

#include "../include/cxxopts.hpp"

#define MAX_BATCH_SIZE 1000

std::atomic<uint64_t> shared_price{1000};
std::atomic<bool> running{true};

/**
 * @brief Listens to multicast trades to maintain a moving mid-price for
 * randomly generated orders without coupling to the engine
 */
void listen_for_trade() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in servaddr;

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(5000);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Failed to bind socket");
        return;
    }

    ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr("239.0.0.1");
    group.imr_interface.s_addr = INADDR_ANY;
    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group));

    char buffer[65536];

    while (running.load()) {
        ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);

        if (n == sizeof(TradePacket)) {
            TradePacket *packet = reinterpret_cast<TradePacket *>(buffer);
            if (packet->header.type == MsgType::MSG_TRADE) {
                int current_price = shared_price.load(std::memory_order_relaxed);

                // Recalculate price based on traded price
                // Ignore price if close to MAX_PRICE or 0 to avoid generating prices close to extremes
                int new_price;
                if (packet->payload.price >= (MAX_PRICE - 1000) || packet->payload.price == 0) {
                    new_price = current_price;
                } else {
                    new_price = packet->payload.price;
                }

                // Atomic int used to hint price movement, no cross-variable synchronization needed,
                // so std::memory_order_relaxed is sufficient
                shared_price.store(new_price, std::memory_order_relaxed);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Cxxopts for CLI options
    cxxopts::Options options("UDP-Client", "UDP Server for sending orders");

    options.add_options()
        ("h, help", "Print usage")
        ("n, num-orders", "Number of orders to send", cxxopts::value<uint64_t>()->default_value("1000000"))
        ("b, batch-size", "Size of batches to send orders (Max 1000)", cxxopts::value<uint16_t>()->default_value("100"));

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    uint64_t numOrders = result["num-orders"].as<uint64_t>();
    uint16_t batchSize = result["batch-size"].as<uint16_t>();

    if (batchSize > MAX_BATCH_SIZE) {
        std::cerr << "Batch size should be less than 1000";
        exit(0);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Disable Nagle's Algorithm to decrease latency massively
    int optval = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        if (errno == ECONNREFUSED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        std::cout << "ERROR WITH CONNECT: " << strerror(errno) << "\n";
        running.store(false, std::memory_order_relaxed);
        return -1;
    }

    // Random uniform distribution for generating orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> mid_price_movement(-1, 1);
    std::uniform_int_distribution<> qty_dist(1, 10);
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> tif_dist(0, 2);
    std::uniform_int_distribution<> type_dist(0, 2);

    std::cout << "Blasting " << numOrders << " orders in batches of " << batchSize << "..." << std::endl;

    int initial_price = 5000;
    std::ifstream price_in(".last_price.txt");
    if (price_in.good()) {
        price_in >> initial_price;
    }
    price_in.close();
    shared_price.store(initial_price, std::memory_order_relaxed);

    // Separate thread for listening for trades
    std::thread listener(listen_for_trade);

    std::vector<WireMessage> batch(batchSize);
    for (uint64_t i = 0; i < numOrders && running.load(std::memory_order_relaxed) == true; i += batchSize) {
        // Current mid price and variance of price generated
        uint64_t current_mid = shared_price.load(std::memory_order_relaxed);
        uint64_t net_drift = 0;

        for (int j = 0; j < batchSize; j++) {
            // Populate random orders
            batch[j].id = i + j;
            batch[j].user_id = 0;

            // Price distribution around mid price
            std::uniform_int_distribution<int64_t> price_dist(
                static_cast<int64_t>(current_mid) - 10,
                static_cast<int64_t>(current_mid) + 10
            );

            batch[j].price = static_cast<uint64_t>(
                std::min(std::max(price_dist(gen), 0L), static_cast<int64_t>(MAX_PRICE - 1))
            );

            batch[j].quantity = qty_dist(gen);
            batch[j].side = side_dist(gen) == 0 ? 'B' : 'S';
            batch[j].tif = tif_dist(gen);
            batch[j].type = 0;

            int64_t drift = mid_price_movement(gen);
            int64_t next_mid = (int64_t)current_mid + drift;

            if (next_mid <= 100) {
                current_mid = 100;
            } else if (next_mid >= 9000) {
                current_mid = 9000;
            } else {
                current_mid = next_mid;
            }
            current_mid += drift;
            net_drift += drift;
        }

        // Send all bytes reliably over TCP
        const char* data_ptr = reinterpret_cast<const char*>(batch.data());
        size_t bytes_to_send = batch.size() * sizeof(WireMessage);
        size_t total_sent = 0;
        int has_error = 0;

        while (total_sent < bytes_to_send) {
            int ret = send(sockfd, data_ptr + total_sent, bytes_to_send - total_sent, 0);
            if (ret <= 0) {
                std::cout << "ERROR WITH SEND: " << strerror(errno) << "\n";
                has_error = 1;
                break;
            }
            total_sent += ret;
        }

        if (has_error) break;

        // Add total drift after sending a batch
        shared_price.fetch_add(net_drift, std::memory_order_relaxed);
    }

    running.store(false, std::memory_order_relaxed);

    // Custom kill order to make sure the program stops
    WireMessage killMessage;

    killMessage.id = 0;
    killMessage.user_id = 0;
    killMessage.price = 100;
    killMessage.quantity = 10;
    killMessage.side = 'S';
    killMessage.tif = 0;
    killMessage.type = 99;

    std::cout << "Sending kill message\n";
    for (int i = 0; i < 10; i++) {
        if (send(sockfd, &killMessage, sizeof(killMessage), 0) == -1) {
            std::cout << "Error sending kill message";
            return 1;
        }
    }

    std::cout << "Done.\n";

    std::ofstream price_out(".last_price.txt");
    if (price_out.good()) {
        price_out << shared_price.load();
    }
    price_out.close();

    close(sockfd);

    listener.detach();
    return 0;
}