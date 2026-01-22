#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

#include "../include/orderbook.hpp"
#include "../include/order_types.hpp"
#include "../include/gateway.hpp"

// struct __attribute__((packed)) WireMessage {
//     uint64_t id;
//     uint64_t price;
//     uint32_t quantity;
//     char side;
//     uint8_t type;
//     uint8_t tif;
// };

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    WireMessage msg;
    msg.price = 100;
    msg.quantity = 10;
    msg.side = 'B';
    msg.type = 0;
    msg.tif = 0;
    msg.id = 1;

    std::cout << "Blasting " << NUM_ORDERS << " orders..." << std::endl;

    for (uint64_t i = 0; i < NUM_ORDERS; i++) {
        msg.id = i;
        sendto(sockfd, &msg, sizeof(msg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    }

    msg.type = 99;
    sendto(sockfd, &msg, sizeof(msg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    std::cout << "Done." << std::endl;
    close(sockfd);
    return 0;
}