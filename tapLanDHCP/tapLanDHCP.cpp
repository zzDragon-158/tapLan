#include "tapLanDHCP.hpp"

sockaddr_in6 gatewayAddr;
std::unordered_map<TapLanMACAddress, sockaddr_in6> macToSA6Map;
std::unordered_map<TapLanMACAddress, uint32_t> macToIPv4Map;
std::unordered_map<std::string, size_t> tapLanNodeStatus;
std::vector<TapLanFIBElement> FIBTable;

void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg) {
    memset(&msg, 0, sizeof(msg));
    msg.op = 1;
    memcpy(msg.mac, mac.address, 6);
}

bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, const sockaddr_in6& addr, TapLanDHCPMessage& msg, size_t& msgLen) {
    static uint32_t hostIDStart = 1;
    static uint8_t paddings[16] = {0};
    if (msg.op != 1 || msg.netIDLen != 0 || msg.ipv4addr != 0 || msg.mac[0] & 0x01 || memcmp(paddings, msg.paddings, 16)) {
        return false;
    }
    uint32_t ipv4addr = 0;
    if (!tapLanGetIPv4ByMAC(msg.mac, ipv4addr)) {
        // allocate host id
        ++hostIDStart;
        ipv4addr = netID + hostIDStart;
        tapLanAddNewNode(addr, msg.mac, ipv4addr, DHCP_STATUS_ONLINE);
        TapLanDHCPLogInfo("New connection from [%s]:%u\ntapLanMACAddress[%.2X:%.2X:%.2X:%.2X:%.2X:%.2X] tapLanIPv4Address: [%u.%u.%u.%u]",
            tapLanIPv6ntos(addr.sin6_addr).c_str(), ntohs(addr.sin6_port),
            msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5],
            ((ipv4addr >> 24) & 0xff), ((ipv4addr >> 16) & 0xff), ((ipv4addr >> 8) & 0xff), (ipv4addr & 0xff));
    }
    tapLanSetNodeStatusByIPv6(tapLanIPv6ntos(addr.sin6_addr), DHCP_STATUS_ONLINE);
    msg.op = 2;
    msg.ipv4addr = ipv4addr;
    msg.netIDLen = netIDLen;
    msg.FIBLen = FIBTable.size();
    memcpy(((&msg) + 1), FIBTable.data(), msg.FIBLen * sizeof(TapLanFIBElement));
    msgLen += msg.FIBLen * sizeof(TapLanFIBElement);
    return true;
}

bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg) {
    static uint32_t last_ipaddr = 0;
    if (msg.op != 2 || msg.ipv4addr == 0)
        return false;
    TapLanFIBElement* pFIBTable = (TapLanFIBElement*)((&msg) + 1);
    for(int i = 0; i < msg.FIBLen; ++i) {
        uint32_t ipv4addr = 0;
        bool isExist = tapLanGetIPv4ByMAC(pFIBTable[i].mac, ipv4addr);
        if (!isExist) {
            sockaddr_in6 sa;
            if (pFIBTable[i].sin6_port == 0) {
                memcpy(&sa, &gatewayAddr, sizeof(sockaddr_in6));
            } else {
                memset(&sa, 0, sizeof(sa));
                sa.sin6_family = AF_INET6;
                memcpy(&sa.sin6_addr, &pFIBTable[i].sin6_addr, sizeof(in6_addr));
                sa.sin6_port = pFIBTable[i].sin6_port;
            }
            tapLanAddNewNode(sa, pFIBTable[i].mac, pFIBTable[i].ipv4addr, pFIBTable[i].dhcp_status);
        } else {
            tapLanSetNodeStatusByIPv6(tapLanIPv6ntos(pFIBTable[i].sin6_addr), pFIBTable[i].dhcp_status);
        }
    }
    if (last_ipaddr == msg.ipv4addr)
        return false;
    last_ipaddr = msg.ipv4addr;
    std::ostringstream cmd;
    in_addr ipv4addr;
    ipv4addr.s_addr = htonl(msg.ipv4addr);
#ifdef _WIN32
    cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipv4addr) << "/" << +msg.netIDLen;
#else
    cmd << "ip addr flush dev tapLan\n";
    cmd << "ip addr add " << inet_ntoa(ipv4addr) << "/" << +msg.netIDLen << " dev tapLan";
#endif
    if (system(cmd.str().c_str())) {
        TapLanDHCPLogError("Setting tapLan IP address to %s/%d failed.", inet_ntoa(ipv4addr), msg.netIDLen);
    } else {
        TapLanDHCPLogInfo("tapLan IP address has been set to %s/%d.", inet_ntoa(ipv4addr), msg.netIDLen);
    }
    return true;
}

bool tapLanGetSA6ByMAC(const TapLanMACAddress& mac, sockaddr_in6& sa6) {
    auto it = macToSA6Map.find(mac);
    if (it != macToSA6Map.end()) {
        memcpy(&sa6, &it->second, sizeof(sockaddr_in6));
        return true;
    }
    return false;
}

bool tapLanGetIPv4ByMAC(const TapLanMACAddress& mac, uint32_t& ipv4addr) {
    auto it = macToIPv4Map.find(mac);
    if (it != macToIPv4Map.end()) {
        ipv4addr = it->second;
        return true;
    }
    return false;
}

uint8_t tapLanGetNodeStatusByIPv6(const std::string& ipv6) {
    auto it = tapLanNodeStatus.find(ipv6);
    if (it != tapLanNodeStatus.end()) {
        return FIBTable[it->second].dhcp_status;
    }
    return DHCP_STATUS_OFFLINE;
}

bool tapLanSetNodeStatusByIPv6(const std::string& ipv6, uint8_t status) {
    auto it = tapLanNodeStatus.find(ipv6);
    if (it != tapLanNodeStatus.end()) {
        FIBTable[it->second].dhcp_status = status;
        return true;
    }
    return false;
}

bool tapLanAddNewNode(const sockaddr_in6& sa, const TapLanMACAddress& mac, const uint32_t& ipv4addr, uint8_t status) {
    macToSA6Map[mac] = sa;
    macToIPv4Map[mac] = ipv4addr;
    TapLanFIBElement fe = {0};
    memcpy(&fe.sin6_addr, &sa.sin6_addr, sizeof(sockaddr_in6));
    fe.sin6_port = sa.sin6_port;
    fe.ipv4addr = ipv4addr;
    memcpy(fe.mac, mac.address, 6);
    fe.dhcp_status = status;
    tapLanNodeStatus[tapLanIPv6ntos(sa.sin6_addr)] = FIBTable.size();
    FIBTable.push_back(fe);
    return true;
}
