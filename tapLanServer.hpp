#pragma once

#include <stdint.h>
#include <thread>
#include <unordered_map>
#include <netinet/in.h>

class TapLanServer {
public:
    TapLanServer(uint16_t serverPort);
    ~TapLanServer();
    bool openTapDevice(const char* devName);
    bool openUdpSocket(uint16_t port);
    void recvFromSocketAndForwardToTap();
    void readFromTapAndSendToSocket();
    void main();
    void start();
    void stop();

private:
    int tap_fd;
    int udp_fd;
    bool run_flag;
    std::thread mainThread;
    std::unordered_map<uint64_t, struct sockaddr_in6> macToIPv6Map;
};
