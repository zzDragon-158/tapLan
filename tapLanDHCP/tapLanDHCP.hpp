#pragma once
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <string>
#include <vector>
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
    uint8_t     netlen;             // 网络号长度
    uint32_t    ipv4addr;           // 分配的 IP
    uint32_t    nodelen;            // 节点长度
};
struct TapLanNodeInfo {
    in6_addr    ipv6addr;
    uint16_t    ipv6port;
    uint32_t    ipv4addr;
    uint8_t     mac[6];
    uint16_t    status;
    uint8_t     paddings[2];
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
enum TapLanDHCPStatus {
    DHCP_STATUS_ONLINE = 0,
    DHCP_STATUS_OFFLINE,
    NUMS_OF_DHCP_STATUS,
};

extern sockaddr_in6 gatewayAddr;
extern std::unordered_map<TapLanMACAddress, TapLanNodeInfo> macToNodeMap;

/**
 * @brief 生成TapLanDHCPMessage Discover
 * @param mac [输入] 你的tapLan设备的MAC地址
 * @param msg [输入 | 输出] 输入需要填充的TapLanDHCPMessage Discover，输出填充完毕的TapLanDHCPMessage Discover
 * @return 无返回值
 */
void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg);

/**
 * @brief 服务端处理来自客户端的TapLanDHCPMessage Discover
 * @param netID [输入] 网络号，用于生成TapLanDHCPMessage Offer
 * @param netIDLen [输入] 网络号长度，用于生成TapLanDHCPMessage Offer
 * @param addr [输入] 客户端的网络套接字地址
 * @param msg [输入 | 输出] 输入来自客户端的TapLanDHCPMessage Discover，输出TapLanDHCPMessage Offer
 * @param msgLen [输入 | 输出] 输入当前msg的大小，输出填充后的msg的大小
 * @return 验证TapLanDHCPMessage Discover的格式，格式正确返回true，不正确返回false
 */
bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, const sockaddr_in6& addr, TapLanDHCPMessage& msg, size_t& msgLen);

/**
 * @brief 客户端处理来自服务端下发的TapLanDHCPMessage Offer
 * @param msg [输入] 来自服务端下发的TapLanDHCPMessage Offer
 * @return 检查TapLanDHCPMessage Offer的内容是否分配了tapLanIPv4地址，如果分配地址和上次一样或者格式不正确会返回false
 */
bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg);

/**
 * @brief 通过mac地址获取IPv6套接字地址
 * @param mac [输入] 需要查找的mac地址
 * @param addr [输出] 对应的IPv6套接字地址
 * @return hash表中不存在这个key时会返回false
 */
bool tapLanGetNodeSA6ByMAC(const TapLanMACAddress& mac, sockaddr_in6& addr);

/**
 * @brief 通过mac地址获取tapLanIPv4地址
 * @param mac [输入] 需要查找的mac地址
 * @param sa6 [输出] 对应的IPv6套接字地址
 * @return hash表中不存在这个key时会返回false
 */
bool tapLanGetNodeIPv4ByMAC(const TapLanMACAddress& mac, uint32_t& ipv4addr);

/**
 * @brief 通过节点的ipv6地址来获取它的在线状态
 * @param ipv6 [输入] 节点的IPv6地址
 * @return 返回IPv6地址对应节点的在线状态，在线返回DHCP_STATUS_ONLINE，离线返回DHCP_STATUS_OFFLINE
 */
uint8_t tapLanGetNodeStatusByMAC(const TapLanMACAddress& mac);

/**
 * @brief 设置IPv6地址对应节点的状态
 * @param mac [输入] 节点的MAC地址
 * @return 节点存在时返回true
 */
bool tapLanSetNodeStatusByMAC(const TapLanMACAddress& mac, uint8_t status);

/**
 * @brief 统一使用该API向macToSA6Map，macToIPv4Map，tapLanNodeStatus，FIBTable这些数据结构中添加节点以方便维护
 * @param sa [输入] 新节点的IPv6套接字地址
 * @param mac [输入] 新节点的tapLanMAC地址
 * @param ipv4 [输入] 新节点的tapLanIPv4地址
 * @param status [输入] 新节点的在线状态
 * @return 返回true
 */
bool tapLanAddNewNode(const sockaddr_in6& sa, const TapLanMACAddress& mac, const uint32_t& ipv4addr, uint8_t status);
inline std::string tapLanIPv4ntos(const in_addr& ipv4addr) {
    char ipv4str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipv4addr, ipv4str, INET_ADDRSTRLEN);
    return std::string(ipv4str);
}
inline std::string tapLanIPv6ntos(const in6_addr& ipv6addr) {
    char ipv6str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ipv6addr, ipv6str, INET6_ADDRSTRLEN);
    return std::string(ipv6str);
}

bool tapLanGetNodeByMAC(const TapLanMACAddress& mac, TapLanNodeInfo& node);
