#include "tapLan.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <getopt.h>

bool isRunningAsAdmin();

int main(int argc, char* argv[]) {
    if (!isRunningAsAdmin()) {
        fprintf(stderr, "Please run this program as an administrator or root\n");
        return -1;
    }
    bool isServer = false;
    uint32_t netID = 0;
    int netIDLen = 0;
    char serverAddr[INET6_ADDRSTRLEN] = "::ffff:";
    size_t serverAddrOffset = strlen(serverAddr);
    uint16_t port = 0;
    if (argc < 2) {
        fprintf(stderr, "less argc\n");
        return 1;
    }
    int opt;
    while ((opt = getopt(argc, argv, "s:c:p:h")) != -1) {
        switch (opt) {
            case 's': {
                isServer = true;
                size_t cidrLen = strlen(optarg);
                if (optarg[cidrLen - 3] != '/') {
                    fprintf(stderr, "your input cidr is invalid\n");
                    return -1;
                }
                netIDLen = atoi(optarg + cidrLen - 2);
                if (16 > netIDLen || netIDLen > 24) {
                    fprintf(stderr, "your input netIDLen is invalid\n");
                    return -1;
                }
                char ipv4addr[INET_ADDRSTRLEN];
                memcpy(ipv4addr, optarg, cidrLen - 3);
                ipv4addr[cidrLen - 3] = '\0';
                struct sockaddr_in sa;
                if (inet_pton(AF_INET, ipv4addr, &(sa.sin_addr)) == 0) {
                    std::cerr << "your input netID: " << ipv4addr << std::endl;
                    return -1;
                }
                netID = ntohl(sa.sin_addr.s_addr);
                break;
            } case 'c': {
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
            } case 'p': {
                port = atoi(optarg);
                if (port > 65535) {
                    fprintf(stderr, "port number is invalid, range 0-65535");
                    return 1;
                }
                break;
            } case 'h': {
                printf("Usage as server: %s [-s <CIDR>] [-p <server port>]\n", argv[0]);
                printf("Usage as client: %s [-c <server address>] [-p <server port>]\n", argv[0]);
                return 1;
            }
        }
    }
    TapLan* pTapLan;
    if (isServer) {
        pTapLan = new TapLan(port, netID, netIDLen);
    } else {
        printf("server [%s]:%u\n", serverAddr, port);
        pTapLan = new TapLan(serverAddr, port);
    }
    pTapLan->start();
    std::string input;
    while (true) {
        std::cout << "enter \"quit\" to exit" << std::endl;
        std::getline(std::cin, input);
        if (input == "quit") {
            std::cout << "Waiting for thread termination......" << std::endl;
            delete [] pTapLan;
            break;
        }
    }
    std::cout << "The program has exited" << std::endl;
    return 0;
}

#ifdef _WIN32
bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = nullptr;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    
    return isAdmin;
}

#else
bool isRunningAsAdmin() {
    return geteuid() == 0;
}

#endif
