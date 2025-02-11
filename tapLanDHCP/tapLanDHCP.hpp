#pragma once
#include "../tapLanSocket/tapLanSocket.hpp"
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <ctime> 
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

#pragma pack(push, 1)
struct TapLanDHCPMessage {
    uint16_t xid;           // 事务 ID
    uint8_t op;             // 消息类型: 1=请求, 2=应答
    uint8_t mac[6];         // 客户端 MAC 地址
    uint32_t addr;        // 分配的 IP
    uint8_t netIDLen;
};
#pragma pack(pop)

bool tapLanSendDHCPDiscover(uint8_t macAddress[6]);
void tapLanClearRunFlag();
bool tapLanHandleDHCPDiscover(uint32_t netID, int netIDLen, struct TapLanDHCPMessage& msg, const struct sockaddr* dstAddr, socklen_t addrLen);
bool tapLanHandleDHCPOffer(struct TapLanDHCPMessage& msg);
