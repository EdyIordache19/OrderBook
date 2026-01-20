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

    uint64_t orders_received = 0;
    while (running) {
        socklen_t len = sizeof(serv_addr);
        int n_bytes = recvfrom(sockfd, (char *)buffer, MAXLINE,
                    MSG_WAITALL, (struct sockaddr *)&serv_addr, &len);

        buffer[n_bytes] = '\0';

        WireMessage message = *(WireMessage *)buffer;
        std::cout << "Id of received order: " << message.id << "\n";

        Order order;

        // Copy to order
        order.id = message.id;
        order.price = message.price;
        order.quantity = message.quantity;

        if (message.side == 'B') order.side = Side::BUY;
        else order.side = Side::SELL;

        if (message.type == 0) order.type = OrderType::LIMIT;
        else {
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

        order.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        ring_buffer.push(order);
        orders_received++;
        if (orders_received >= NUM_ORDERS) {
            std::cout << "Gateway finished receiving all " << NUM_ORDERS << " orders\n";
            running.store(false, std::memory_order_release);
            break;
        }
    }

    close(sockfd);
}