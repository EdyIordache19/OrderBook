#include <cstdio>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <atomic>
#include <thread>

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

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct iovec iovecs[MAX_BATCH_SIZE];
    struct mmsghdr msgvec[MAX_BATCH_SIZE];
    WireMessage msgs[MAX_BATCH_SIZE];

    // Random uniform distribution for generating orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> mid_price_movement(-1, 1);
    std::uniform_int_distribution<> qty_dist(1, 500);
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

    for (uint64_t i = 0; i < numOrders; i += batchSize) {
        // Current mid price and variance of price generated
        uint64_t current_mid = shared_price.load(std::memory_order_relaxed);
        uint64_t net_drift = 0;

        for (int j = 0; j < batchSize; j++) {
            memset(&iovecs[j], 0, sizeof(iovecs[j]));
            iovecs[j].iov_base = &msgs[j];
            iovecs[j].iov_len = sizeof(WireMessage);

            memset(&msgvec[j], 0, sizeof(msgvec[j]));
            msgvec[j].msg_hdr.msg_name = &servaddr;
            msgvec[j].msg_hdr.msg_namelen = sizeof(servaddr);
            msgvec[j].msg_hdr.msg_iov = &iovecs[j];
            msgvec[j].msg_hdr.msg_iovlen = 1;

            // Populate random orders
            msgs[j].id = i + j;
            msgs[j].user_id = 0;

            // Price distribution around mid price
            std::uniform_int_distribution<int64_t> price_dist(
                static_cast<int64_t>(current_mid) - 10,
                static_cast<int64_t>(current_mid) + 10
            );

            msgs[j].price = static_cast<uint64_t>(
                std::min(std::max(price_dist(gen), 0L), static_cast<int64_t>(MAX_PRICE - 1))
            );

            msgs[j].quantity = qty_dist(gen);
            msgs[j].side = side_dist(gen) == 0 ? 'B' : 'S';
            msgs[j].tif = tif_dist(gen);
            msgs[j].type = 0;

            uint64_t drift = mid_price_movement(gen);
            current_mid += drift;
            net_drift += drift;
        }

        // Sendmmsg to send batches of orders
        // Avoids the system overhead of using a syscall for each order
        int ret = sendmmsg(sockfd, msgvec, batchSize, 0);
        if (ret == -1) {
            std::cout << "ERROR WITH SENDMMSG\n";
            break;
        }

        // Add total drift after sending a batch
        shared_price.fetch_add(net_drift, std::memory_order_relaxed);
    }

    running.store(false);

    // Custom kill order to make sure the program stops
    WireMessage killMessage;

    killMessage.id = 0;
    killMessage.user_id = 0;
    killMessage.price = 100;
    killMessage.quantity = 10;
    killMessage.side = 'S';
    killMessage.tif = 0;
    killMessage.type = 99;

    for (int i = 0; i < 10; i++) {
        uint64_t n = sendto(sockfd, &killMessage, sizeof(killMessage), 0,
            (const struct sockaddr *)&servaddr, sizeof(servaddr));
        if (n < sizeof(WireMessage)) {
            std::cout << "Error sending kill message";
            return 1;
        }
    }

    std::cout << "Done." << std::endl;

    std::ofstream price_out(".last_price.txt");
    if (price_out.good()) {
        price_out << shared_price.load();
    }
    price_out.close();

    close(sockfd);

    listener.detach();
    return 0;
}