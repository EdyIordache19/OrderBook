#include "gateway.hpp"
#include "orderbook.hpp"
#include "main.hpp"
#include "decoder.hpp"

#include <asm-generic/socket.h>
#include <atomic>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
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

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "ERROR OPENING SOCKET\n";
        exit(-1);
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    // Disable Nagle's algorithm for localhost ultra-low latency
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

    setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &optval, sizeof(optval));

    int rcv_buff_size = 16000000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcv_buff_size, sizeof(rcv_buff_size));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "ERROR BINDING: " << strerror(errno) << "\n";
        close(sockfd);
        exit(-1);
    }

    // Set listener to non-blocking
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    listen(sockfd, 10);

    char buffer[65536];
    size_t leftover_bytes = 0;

    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    uint64_t orders_received = 0;

    while (running) {
        // Call accept4 to set the client socket to non-blocking using one atomic step, avoiding multiple syscalls (accept + fcntl)
        int client_sock = accept4(sockfd, nullptr, nullptr, SOCK_NONBLOCK);

        while (true) {
            long int n_bytes = recv(client_sock, buffer + leftover_bytes,
                                    sizeof(buffer) - leftover_bytes, 0);

            if (n_bytes == 0) {
                // TCP FIN, end
                close(client_sock);
                break;
            } else if (n_bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Nothing was received
                    continue;
                }

                // Kill everything
                close(client_sock);
                break;
            }

            size_t total_bytes = leftover_bytes + n_bytes;
            size_t offset = 0;

            while (total_bytes - offset >= sizeof(WireMessage)) {
                if (orders_received == 0) {
                    start = std::chrono::high_resolution_clock::now();
                }

                Order order;
                if (Decoder::decode(buffer + offset, sizeof(WireMessage), order, MAX_PRICE)) {
                    while (ordersBuffer.push(order) != 0);

                    orders_received++;
                    if (orders_received >= numOrders || order.type == OrderType::KILL) {
                        std::cout << "Gateway finished receiving all " << orders_received << " orders\n";
                        auto end = std::chrono::high_resolution_clock::now();

                        std::chrono::duration<double> diff = end - start;
                        double throughput = numOrders / diff.count();

                        std::cout << "Processed " << numOrders << " orders in " << diff.count() << " seconds.\n";
                        std::cout << "Throughput: " << throughput << " orders/second. \n";

                        running.store(false, std::memory_order_release);

                        return;
                    }
                }
                offset += sizeof(WireMessage);
            }

            leftover_bytes = total_bytes - offset;
            if (leftover_bytes > 0) {
                memmove(buffer, buffer + offset, leftover_bytes);
            }
        }

        close(client_sock);
    }

    close(sockfd);
}