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

#define BATCH_SIZE 32

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct iovec iovecs[BATCH_SIZE];
    struct mmsghdr msgvec[BATCH_SIZE];
    WireMessage msgs[BATCH_SIZE];

    for (int i = 0; i < BATCH_SIZE; i++) {
        memset(&iovecs[i], 0, sizeof(iovecs[i]));
        iovecs[i].iov_base = &msgs[i];
        iovecs[i].iov_len = sizeof(WireMessage);

        memset(&msgvec[i], 0, sizeof(msgvec[i]));
        msgvec[i].msg_hdr.msg_name = &servaddr;
        msgvec[i].msg_hdr.msg_namelen = sizeof(servaddr);
        msgvec[i].msg_hdr.msg_iov = &iovecs[i];
        msgvec[i].msg_hdr.msg_iovlen = 1;

        msgs[i].id = i;
        msgs[i].price = 100;
        msgs[i].quantity = 10;
        msgs[i].side = 'B';
        msgs[i].tif = 0;
        msgs[i].type = 0;
    }

    std::cout << "Blasting " << NUM_ORDERS << " orders in batches of " << BATCH_SIZE << "..." << std::endl;

    for (uint64_t i = 0; i < NUM_ORDERS; i += BATCH_SIZE) {
        int ret = sendmmsg(sockfd, msgvec, BATCH_SIZE, 0);
        if (ret == -1) {
            std::cout << "ERROR WITH SENDMMSG\n";
            break;
        }
    }

    std::cout << "Done." << std::endl;
    close(sockfd);
    return 0;
}