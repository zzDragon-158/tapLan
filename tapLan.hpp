#pragma once
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <vector>
#include "./tapLanSocket/tapLanSocket.hpp"
#include "./tapLanDrive/tapLanDrive.hpp"
#include "./tapLanDHCP/tapLanDHCP.hpp"
#include "./tapLanSecurity/tapLanSecurity.hpp"
#define TapLanLogInfo(fmt, ...)         fprintf(stdout, "[TapLan] [INFO] " fmt "\n", ##__VA_ARGS__)
#define TapLanLogError(fmt, ...)        fprintf(stderr, "[TapLan] [ERROR] " fmt "\n", ##__VA_ARGS__)

class TapLan {
public:
    TapLan(uint16_t serverPort, uint32_t netID, int netIDLen, const char* key);      // server
    TapLan(const char* serverAddr, const uint16_t serverPort, const char* key, bool isDirectSupport);      // client
    ~TapLan();
    void showErrorCount();
    void showFIB();
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
    TapLanKey myKey;
    bool isDirectSupport;
    std::thread threadRecvFromUdpSocketAndForwardToTap, threadReadFromTapAndSendToSocket;
    std::thread threadKeepConnectedWithServer;
    void recvFromUdpSocketAndForwardToTap();
    void readFromTapAndSendToSocket();
    void handleDHCPMsgServer();
    void handleDHCPMsgClient();
};
