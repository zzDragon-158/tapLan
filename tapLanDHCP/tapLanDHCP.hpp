#pragma once
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <unordered_map>
#include <sstream>
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif
#define TapLanDHCPLogInfo(fmt, ...)         fprintf(stdout, "[TapLanDHCP] [INFO] " fmt "\n", ##__VA_ARGS__)
#define TapLanDHCPLogError(fmt, ...)        fprintf(stderr, "[TapLanDHCP] [ERROR] " fmt "\n", ##__VA_ARGS__)

// this struct must be aligned
#pragma pack(push, 1) 
struct TapLanDHCPMessage {
    uint8_t     op;                 // 消息类型: 1=请求, 2=应答
    uint8_t     mac[6];             // 客户端 MAC 地址
    uint8_t     netIDLen;           // 网络号长度
    uint32_t    addr;               // 分配的 IP
    uint16_t    FIB;                // FIB序号
    uint16_t    FIBLen;             // FIB长度
    uint8_t     paddings[16];
};
struct TapLanFIBElement {
    in6_addr    sin6_addr;
    uint16_t    sin6_port;
    uint32_t    hostID;
    uint8_t     mac[6];
    uint8_t     paddings[4];
};
#pragma pack(pop)
struct TapLanMACAddress {
    uint8_t address[6];
    TapLanMACAddress() {
        memset(address, 0, 6);
    }

    TapLanMACAddress(uint8_t mac[6]) {
        memcpy(address, mac, 6);
    }

    bool operator==(const TapLanMACAddress& other) const {
        for (int i = 0; i < 6; ++i) {
            if (address[i] != other.address[i]) {
                return false;
            }
        }
        return true;
    }
};
namespace std {
    template <>
    struct hash<TapLanMACAddress> {
        std::size_t operator()(const TapLanMACAddress& mac) const {
            std::size_t hash_value = 0;
            for (int i = 0; i < 6; ++i) {
                hash_value = (hash_value << 8) + mac.address[i];
            }
            return hash_value;
        }
    };
}

extern uint32_t current_fib;
extern sockaddr_in6 gatewayAddr;
extern std::unordered_map<TapLanMACAddress, sockaddr_in6> macToIPv6Map;
extern std::unordered_map<TapLanMACAddress, uint32_t> macToHostIDMap;

void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg);
bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, const sockaddr_in6& addr, TapLanDHCPMessage& msg, size_t& msgLen);
bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg);
bool tapLanGetHostID(const TapLanMACAddress& mac, uint32_t& hostID);
