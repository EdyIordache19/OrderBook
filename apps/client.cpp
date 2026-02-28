#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <random>

#include "../include/orderbook.hpp"
#include "../include/order_types.hpp"
#include "../include/decoder.hpp"
#include "../include/gateway.hpp"

#include "../include/cxxopts.hpp"

int main(int argc, char *argv[]) {
    cxxopts::Options options("UDP-Client", "UDP Server for sending orders");

    options.add_options()
        ("h, help", "Print usage")
        ("n, num-orders", "Number of orders to send", cxxopts::value<uint64_t>()->default_value("1000000"))
        ("b, batch-size", "Size of batches to send orders", cxxopts::value<uint16_t>()->default_value("100"));

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    uint64_t numOrders = result["num-orders"].as<uint64_t>();
    uint16_t batchSize = result["batch-size"].as<uint16_t>();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct iovec iovecs[batchSize];
    struct mmsghdr msgvec[batchSize];
    WireMessage msgs[batchSize];

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> mid_price_movement(-1, 1);
    std::uniform_int_distribution<> qty_dist(1, 100);
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_int_distribution<> tif_dist(0, 2);
    std::uniform_int_distribution<> type_dist(0, 2);

    std::cout << "Blasting " << numOrders << " orders in batches of " << batchSize << "..." << std::endl;

    int mid_price = 1000;
    std::ifstream price_in(".last_price.txt");
    if (price_in.good()) {
        price_in >> mid_price;
    }
    price_in.close();

    for (uint64_t i = 0; i < numOrders; i += batchSize) {
        for (int j = 0; j < batchSize; j++) {
            memset(&iovecs[j], 0, sizeof(iovecs[j]));
            iovecs[j].iov_base = &msgs[j];
            iovecs[j].iov_len = sizeof(WireMessage);

            memset(&msgvec[j], 0, sizeof(msgvec[j]));
            msgvec[j].msg_hdr.msg_name = &servaddr;
            msgvec[j].msg_hdr.msg_namelen = sizeof(servaddr);
            msgvec[j].msg_hdr.msg_iov = &iovecs[j];
            msgvec[j].msg_hdr.msg_iovlen = 1;

            msgs[j].id = i + j;

            std::uniform_int_distribution price_dist(mid_price - 1, mid_price + 1);
            msgs[j].price = std::min(std::max(price_dist(gen), 0), MAX_PRICE - 1);
            msgs[j].quantity = qty_dist(gen);
            msgs[j].side = side_dist(gen) == 0 ? 'B' : 'S';
            msgs[j].tif = tif_dist(gen);
            msgs[j].type = 0;

            mid_price += mid_price_movement(gen);
        }

        int ret = sendmmsg(sockfd, msgvec, batchSize, 0);
        if (ret == -1) {
            std::cout << "ERROR WITH SENDMMSG\n";
            break;
        }
    }

    WireMessage killMessage;

    killMessage.id = 0;
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
        price_out << mid_price;
    }
    price_out.close();

    close(sockfd);
    return 0;
}