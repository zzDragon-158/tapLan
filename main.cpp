#include "tapLan.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <getopt.h>

int main(int argc, char* argv[]) {
    bool isServer = false;
    char serverAddr[INET6_ADDRSTRLEN] = "::ffff:";
    size_t serverAddrOffset = strlen(serverAddr);
    uint16_t port = 0;
    if (argc < 2) {
        fprintf(stderr, "less argc\n");
        return 1;
    }
    int opt;
    while ((opt = getopt(argc, argv, "sc:p:h")) != -1) {
        switch (opt) {
            case 's':
                isServer = true;
                break;
            case 'c':
                isServer = false;
                strncpy(serverAddr + serverAddrOffset, optarg, INET6_ADDRSTRLEN - serverAddrOffset);
                struct sockaddr_in6 sa;
                if (inet_pton(AF_INET6, serverAddr + serverAddrOffset, &(sa.sin6_addr)) == 0) {
                    if (inet_pton(AF_INET6, serverAddr, &(sa.sin6_addr)) == 0) {
                        fprintf(stderr, "your input IP address is invalid\n");
                        return 1;
                    }
                } else {
                    memmove(serverAddr, serverAddr + serverAddrOffset, strlen(serverAddr + serverAddrOffset) + 1);
                }
                break;
            case 'p':
                port = atoi(optarg);
                if (port > 65535) {
                    fprintf(stderr, "port number is invalid, range 0-65535");
                    return 1;
                }
                break;
            case 'h':
                printf("Usage as server: %s [-s] [-p <server port>]\n", argv[0]);
                printf("Usage as client: %s [-c <server IP addr>] [-p <server port>]\n", argv[0]);
                return 1;
        }
    }
    TapLan* pTapLan;
    if (isServer) {
        pTapLan = new TapLan(port);
        pTapLan->start();
    } else {
        printf("server [%s]:%u\n", serverAddr, port);
        pTapLan = new TapLan(serverAddr, port);
        pTapLan->start();
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3600));
    }

    return 0;
}
