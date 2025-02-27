#pragma once
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <functional>
#include <cstring>
#include "./tapLanSocket/tapLanSocket.hpp"
#include "./tapLanDrive/tapLanDrive.hpp"
#include "./tapLanDHCP/tapLanDHCP.hpp"
#include "./tapLanSecurity/tapLanSecurity.hpp"
#define TapLanLogInfo(fmt, ...)         fprintf(stdout, "[TapLan] [INFO] " fmt "\n", ##__VA_ARGS__)
#define TapLanLogError(fmt, ...)        fprintf(stderr, "[TapLan] [ERROR] " fmt "\n", ##__VA_ARGS__)

class TapLan {
public:
    TapLan(uint16_t serverPort, uint32_t netID, int netIDLen, const char* key);      // server
    TapLan(const char* serverAddr, const uint16_t serverPort, const char* key);      // client
    ~TapLan();
    bool start();
    bool stop();

private:
    bool run_flag;
    bool isServer;
    uint32_t netID;
    int netIDLen;
    TapLanMACAddress myMAC;
    uint32_t myIP;
    bool isSecurity;
    TapLanKey key;
    std::thread threadRecvFromSocketAndForwardToTap, threadReadFromTapAndSendToSocket;
    std::thread threadKeepConnectedWithServer;
    std::unordered_map<TapLanMACAddress, sockaddr_in6> macToIPv6Map;
    void recvFromSocketAndForwardToTap();
    void readFromTapAndSendToSocket();
    void keepConnectedWithServer();
};
