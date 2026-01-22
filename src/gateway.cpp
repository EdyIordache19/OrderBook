#include "gateway.hpp"
#include "orderbook.hpp"

#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void Gateway::run(RingBuffer& ring_buffer, std::atomic<bool>& running) {
    int sockfd;
    char buffer[MAXLINE];

    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cout << "ERROR OPENING SOCKET\n";
        close(sockfd);
        exit(-1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "ERROR BINDING: " << strerror(errno) << "\n";
        close(sockfd);
        exit(-1);
    }

    std::chrono::high_resolution_clock::time_point start;

    uint64_t orders_received = 0;
    while (running) {
        socklen_t len = sizeof(serv_addr);
        long unsigned int n_bytes = recvfrom(sockfd, (char *)buffer, MAXLINE,
                    MSG_WAITALL, (struct sockaddr *)&serv_addr, &len);
        if (orders_received == 0) {
            start = std::chrono::high_resolution_clock::now();
        }

        if (n_bytes < sizeof(WireMessage)) {
            continue;
        }

        WireMessage message = *(WireMessage *)buffer;
        // std::cout << "Id of received order: " << message.id << "\n";

        Order order;

        // Copy to order
        order.id = message.id;
        order.price = message.price;
        order.quantity = message.quantity;

        if (message.side == 'B') order.side = Side::BUY;
        else order.side = Side::SELL;

        if (message.type == 0) order.type = OrderType::LIMIT;
        else if (message.type == 1) {
            order.type = OrderType::MARKET;
            if (order.side == Side::BUY) {
                order.price = NUM_ORDERS - 1;
            } else {
                order.price = 0;
            }
        }

        switch (message.tif) {
            case 0:
                order.tif = TimeInForce::GTC;
                break;
            case 1:
                order.tif = TimeInForce::IOC;
                break;
            case 2:
                order.tif = TimeInForce::FOK;
                break;
        }

        orders_received++;
        if (orders_received >= NUM_ORDERS || message.type == 99) {
            std::cout << "Gateway finished receiving all " << orders_received << " orders\n";
            running.store(false, std::memory_order_release);

            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double> diff = end - start;
            double throughput = NUM_ORDERS / diff.count();

            std::cout << "Processed " << NUM_ORDERS << " orders in " << diff.count() << " seconds.\n";
            std::cout << "Throughput: " << throughput << " orders/second. \n";

            break;
        }

        order.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        while (ring_buffer.push(order) != 0);
    }

    close(sockfd);
}