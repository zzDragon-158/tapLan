#include "tapLanDHCP.hpp"

uint32_t last_ipaddr = 0;
uint32_t ipAddrStart = 2;
uint32_t current_fib = 0;
sockaddr_in6 gatewayAddr;
std::unordered_map<TapLanMACAddress, sockaddr_in6> macToIPv6Map;
std::unordered_map<TapLanMACAddress, uint32_t> macToHostIDMap;

void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg) {
    memset(&msg, 0, sizeof(msg));
    msg.op = 1;
    memcpy(msg.mac, mac.address, 6);
}

bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, const sockaddr_in6& addr, TapLanDHCPMessage& msg, size_t& msgLen) {
    static uint8_t paddings[16] = {0};
    if (msg.op != 1 || msg.netIDLen != 0 || msg.addr != 0 || msg.mac[0] & 0x01 || memcmp(paddings, msg.paddings, 16)) {
        return false;
    }
    uint32_t ipAddr = netID;
    auto it = macToHostIDMap.find(msg.mac);
    if (it != macToHostIDMap.end()) {
        ipAddr += it->second;
    } else {
        ipAddr += ipAddrStart;
        macToHostIDMap[msg.mac] = ipAddrStart;
        ++ipAddrStart;
        macToIPv6Map[msg.mac] = addr;
        ++current_fib;
        TapLanDHCPLogInfo("New device connected, macAddress[%.2X:%.2X:%.2X:%.2X:%.2X:%.2X] "
            "ipAddress: %u.%u.%u.%u",
            msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5],
            ((ipAddr >> 24) & 0xff), ((ipAddr >> 16) & 0xff), ((ipAddr >> 8) & 0xff), (ipAddr & 0xff));
    }
    msg.op = 2;
    msg.addr = ipAddr;
    msg.netIDLen = netIDLen;
    msg.FIB = current_fib;
    msg.FIBLen = macToIPv6Map.size();
    TapLanFIBElement* feArray = (TapLanFIBElement*)((&msg) + 1);
    int cnt = 0;
    for(const auto& pair: macToIPv6Map) {
        TapLanFIBElement& fe = *(TapLanFIBElement*)feArray;
        memset(&fe, 0, sizeof(TapLanFIBElement));
        memcpy(&fe.sin6_addr, &pair.second.sin6_addr, sizeof(in6_addr));
        fe.sin6_port = pair.second.sin6_port;
        memcpy(fe.mac, pair.first.address, 6);
        tapLanGetHostID(pair.first, fe.hostID);
        ++feArray;
        ++cnt;
        if (cnt == msg.FIBLen) break;
    }
    msgLen += msg.FIBLen * sizeof(TapLanFIBElement);
    return true;
}

bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg) {
    if (msg.op != 2 || msg.addr == 0 || msg.FIB == current_fib)
        return false;
    current_fib = msg.FIB;
    TapLanFIBElement* feArray = (TapLanFIBElement*)((&msg) + 1);
    for(int i = 0; i < msg.FIBLen; ++i) {
        sockaddr_in6 sa;
        if (feArray[i].sin6_port == 0) {
            memcpy(&sa, &gatewayAddr, sizeof(sockaddr_in6));
        } else {
            memset(&sa, 0, sizeof(sa));
            sa.sin6_family = AF_INET6;
            memcpy(&sa.sin6_addr, &feArray[i].sin6_addr, sizeof(in6_addr));
            sa.sin6_port = feArray[i].sin6_port;
        }
        macToIPv6Map[feArray[i].mac] = sa;
        macToHostIDMap[feArray[i].mac] = feArray[i].hostID;
    }
    std::ostringstream cmd;
    if (last_ipaddr != msg.addr) {
        last_ipaddr = msg.addr;
        in_addr ipAddr;
        ipAddr.s_addr = htonl(msg.addr);
#ifdef _WIN32
        cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipAddr) << "/" << +msg.netIDLen;
#else
        cmd << "ip addr flush dev tapLan\n";
        cmd << "ip addr add " << inet_ntoa(ipAddr) << "/" << +msg.netIDLen << " dev tapLan";
#endif
        if (system(cmd.str().c_str())) {
            TapLanDHCPLogError("Setting tapLan IP address to %s/%d failed.", inet_ntoa(ipAddr), msg.netIDLen);
        } else {
            TapLanDHCPLogInfo("tapLan IP address has been set to %s/%d.", inet_ntoa(ipAddr), msg.netIDLen);
        }
    } else {
        return false;
    }
    return true;
}

bool tapLanGetHostID(const TapLanMACAddress& mac, uint32_t& hostID) {
    auto it = macToHostIDMap.find(mac);
    if (it != macToHostIDMap.end()) {
        hostID = it->second;
        return true;
    }
    return false;
}
