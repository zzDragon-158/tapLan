#pragma once

#include <stdint.h>
#include <thread>
#include <netinet/in.h>

class TapLanClient {
public:
    struct routeNode {
        struct in_addr ipV4Addr;
        struct in6_addr ipV6Addr;
        uint16_t sin6_port;
    };
    TapLanClient(const char* serverAddr, const uint16_t serverPort, const uint16_t clientPort);
    ~TapLanClient();
    bool openTapDevice(const char* devName);
    bool openUdpSocket(uint16_t port);
    void recvfromSocketAndForwardToTap();
    void readfromTapAndSendToSocket();
    void main();
    void start();
    void stop();

private:
    int tap_fd;
    int udp_fd;
    bool run_flag;
    sockaddr_in6 gatewayAddr;
    std::thread mainThread;
};
