#include "gateway.hpp"
#include "orderbook.hpp"
#include "main.hpp"
#include "decoder.hpp"

#include <atomic>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <unordered_map>
#include <vector>
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

    struct ClientState {
        char buffer[65536];
        size_t leftover_bytes = 0;
    };

    std::vector<struct pollfd> fds;
    fds.push_back({sockfd, POLLIN, 0});
    std::unordered_map<int, ClientState> states;

    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    uint64_t orders_received = 0;

    while (running) {
        // Poll for incoming data across all connections with 50ms timeout
        int ret = poll(fds.data(), fds.size(), 50);
        if (ret < 0) break;

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                if (fds[i].fd == sockfd) {
                    // Accept new clients
                    int client_sock = accept(sockfd, nullptr, nullptr);
                    if (client_sock >= 0) {
                        fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL, 0) | O_NONBLOCK);
                        int optval = 1;
                        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

                        fds.push_back({client_sock, POLLIN, 0});
                        states[client_sock] = ClientState();
                    }
                } else {
                    int client_fd = fds[i].fd;
                    ClientState& state = states[client_fd];

                    long int n_bytes = recv(client_fd, state.buffer + state.leftover_bytes,
                                            sizeof(state.buffer) - state.leftover_bytes, 0);

                    if (n_bytes < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            continue;
                        }
                        close(client_fd);
                        states.erase(client_fd);
                        fds.erase(fds.begin() + i);
                        i--;
                        continue;
                    } else if (n_bytes == 0) {
                        close(client_fd);
                        states.erase(client_fd);
                        fds.erase(fds.begin() + i);
                        i--;
                        continue;
                    }

                    size_t total_bytes = state.leftover_bytes + n_bytes;
                    size_t offset = 0;

                    while (total_bytes - offset >= sizeof(WireMessage)) {
                        if (orders_received == 0) {
                            start = std::chrono::high_resolution_clock::now();
                        }

                        Order order;
                        if (Decoder::decode(state.buffer + offset, sizeof(WireMessage), order, MAX_PRICE)) {
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

                                // Close remaining cleanly
                                for (auto& pfd : fds) close(pfd.fd);
                                return;
                            }
                        }
                        offset += sizeof(WireMessage);
                    }

                    state.leftover_bytes = total_bytes - offset;
                    if (state.leftover_bytes > 0) {
                        memmove(state.buffer, state.buffer + offset, state.leftover_bytes);
                    }
                }
            }
        }
    }

    for (auto& pfd : fds) close(pfd.fd);
}