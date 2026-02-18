#include "gateway.hpp"
#include "orderbook.hpp"
#include "main.hpp"
#include "decoder.hpp"

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

Gateway::Gateway(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, uint64_t _numOrders)
    : ordersBuffer(_ordersBuffer),
      running(_running),
      numOrders(_numOrders)
    { }

void Gateway::run() {
    pin_thread_to_core(2);

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

    // Spin for 50 us, to not wake up every time
    int busy_poll_usec = 50;
    setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_usec, sizeof(busy_poll_usec));

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
                    0, (struct sockaddr *)&serv_addr, &len);
        if (orders_received == 0) {
            start = std::chrono::high_resolution_clock::now();
        }

        Order order;
        if (Decoder::decode(buffer, n_bytes, order, numOrders)) {
            while (ordersBuffer.push(order) != 0);

            orders_received++;
            if (order.type == OrderType::KILL) {
                std::cout << "Gateway finished receiving all " << orders_received << " orders\n";
                running.store(false, std::memory_order_release);

                auto end = std::chrono::high_resolution_clock::now();

                std::chrono::duration<double> diff = end - start;
                double throughput = numOrders / diff.count();

                std::cout << "Processed " << numOrders << " orders in " << diff.count() << " seconds.\n";
                std::cout << "Throughput: " << throughput << " orders/second. \n";

                running = false;
                break;
            }
        }
    }

    close(sockfd);
}