#pragma once

#include <stdint.h>
#include <thread>
#include <netinet/in.h>

class TapLanServer {
public:
    struct NodeInfo {
        struct in_addr ipV4Addr;
        struct in6_addr ipV6Addr;
        uint16_t sin6_port;
    };
    TapLanServer(uint16_t serverPort);
    ~TapLanServer();
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
    std::thread mainThread;
    struct NodeInfo nodeInfoList[256];
};
