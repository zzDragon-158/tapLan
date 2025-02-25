#pragma once
#include <cstdint>
#include <thread>
#include <unordered_map>
#include "./tapLanSocket/tapLanSocket.hpp"
#include "./tapLanDrive/tapLanDrive.hpp"
#include "./tapLanDHCP/tapLanDHCP.hpp"
#define TapLanLogInfo(fmt, ...)         fprintf(stdout, "[TapLan] [INFO] " fmt "\n", ##__VA_ARGS__)
#define TapLanLogError(fmt, ...)        fprintf(stderr, "[TapLan] [ERROR] " fmt "\n", ##__VA_ARGS__)

class TapLan {
public:
    TapLan(uint16_t serverPort, uint32_t netID, int netIDLen);      // server
    TapLan(const char* serverAddr, const uint16_t serverPort);      // client
    ~TapLan();
    bool openTapDevice();
    bool openUdpSocket(uint16_t port);
    void recvFromSocketAndForwardToTap();
    void readFromTapAndSendToSocket();
    bool start();
    bool stop();

private:
    bool run_flag;
    bool isServer;
    uint32_t netID;
    int netIDLen;
    uint64_t myMAC;
    std::thread threadRecvFromSocketAndForwardToTap, threadReadFromTapAndSendToSocket;
    std::thread threadKeepConnectedWithServer;
    std::unordered_map<uint64_t, struct sockaddr_in6> macToIPv6Map;
};
