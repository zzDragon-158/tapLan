#pragma once
#include <cstdint>
#include <thread>
#include <unordered_map>
#include "./tapLanSocket/tapLanSocket.hpp"
#include "./tapLanDrive/tapLanDrive.hpp"
#include "./tapLanDHCP/tapLanDHCP.hpp"

class TapLan {
public:
    TapLan(uint16_t serverPort, uint32_t netID, int netIDLen);      // server
    TapLan(const char* serverAddr, const uint16_t serverPort);      // client
    ~TapLan();
    bool openTapDevice(const char* devName);
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
    std::thread threadRecvFromSocketAndForwardToTap, threadReadFromTapAndSendToSocket;
    std::thread threadKeepConnectedWithServer;
    std::unordered_map<uint64_t, struct sockaddr_in6> macToIPv6Map;
};
