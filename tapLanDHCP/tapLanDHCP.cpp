#include "tapLanDHCP.hpp"

sockaddr_in6 gatewayAddr;
std::unordered_map<TapLanMACAddress, TapLanNodeInfo> macToNodeMap;

void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg) {
    memset(&msg, 0, sizeof(msg));
    msg.op = 1;
    memcpy(msg.mac, mac.address, 6);
}

bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, const sockaddr_in6& addr, TapLanDHCPMessage& msg, size_t& msgLen) {
    static uint32_t hostIDStart = 1;
    if (msg.op != 1 || msg.netlen != 0 || msg.ipv4addr != 0 || msg.mac[0] & 0x01) {
        return false;
    }
    uint32_t ipv4addr = 0;
    if (!tapLanGetNodeIPv4ByMAC(msg.mac, ipv4addr)) {
        // allocate host id
        ++hostIDStart;
        ipv4addr = netID + hostIDStart;
        tapLanAddNewNode(addr, msg.mac, ipv4addr, DHCP_STATUS_ONLINE);
        TapLanDHCPLogInfo("New connection from [%s]:%u\ntapLanMACAddress[%.2X:%.2X:%.2X:%.2X:%.2X:%.2X] tapLanIPv4Address: [%u.%u.%u.%u]",
            tapLanIPv6ntos(addr.sin6_addr).c_str(), ntohs(addr.sin6_port),
            msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5],
            ((ipv4addr >> 24) & 0xff), ((ipv4addr >> 16) & 0xff), ((ipv4addr >> 8) & 0xff), (ipv4addr & 0xff));
    }
    tapLanSetNodeStatusByMAC(msg.mac, DHCP_STATUS_ONLINE);
    msg.op = 2;
    msg.ipv4addr = ipv4addr;
    msg.netlen = netIDLen;
    /* fill node info table */ {
        msg.nodelen = macToNodeMap.size();
        TapLanNodeInfo* pFIBTable = (TapLanNodeInfo*)((&msg) + 1);
        for (auto pair: macToNodeMap) {
            msgLen += sizeof(TapLanNodeInfo);
            memcpy(pFIBTable++, &pair.second, sizeof(TapLanNodeInfo));
        }
    }
    return true;
}

bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg) {
    static uint32_t last_ipaddr = 0;
    if (msg.op != 2 || msg.ipv4addr == 0)
        return false;
    TapLanNodeInfo* pFIBTable = (TapLanNodeInfo*)((&msg) + 1);
    for(int i = 0; i < msg.nodelen; ++i) {
        uint32_t ipv4addr = 0;
        bool isExist = tapLanGetNodeIPv4ByMAC(pFIBTable[i].mac, ipv4addr);
        if (!isExist) {
            sockaddr_in6 addr;
            if (pFIBTable[i].ipv6port == 0) {
                memcpy(&addr, &gatewayAddr, sizeof(sockaddr_in6));
            } else {
                memset(&addr, 0, sizeof(addr));
                addr.sin6_family = AF_INET6;
                memcpy(&addr.sin6_addr, &pFIBTable[i].ipv6addr, sizeof(in6_addr));
                addr.sin6_port = pFIBTable[i].ipv6port;
            }
            tapLanAddNewNode(addr, pFIBTable[i].mac, pFIBTable[i].ipv4addr, pFIBTable[i].status);
        } else {
            tapLanSetNodeStatusByMAC(pFIBTable[i].mac, pFIBTable[i].status);
        }
    }
    if (last_ipaddr == msg.ipv4addr)
        return false;
    last_ipaddr = msg.ipv4addr;
    std::ostringstream cmd;
    in_addr ipv4addr;
    ipv4addr.s_addr = htonl(msg.ipv4addr);
#ifdef _WIN32
    cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipv4addr) << "/" << +msg.netlen;
#else
    cmd << "ip addr flush dev tapLan\n";
    cmd << "ip addr add " << inet_ntoa(ipv4addr) << "/" << +msg.netlen << " dev tapLan";
#endif
    if (system(cmd.str().c_str())) {
        TapLanDHCPLogError("Setting tapLan IP address to %s/%d failed.", inet_ntoa(ipv4addr), msg.netlen);
    } else {
        TapLanDHCPLogInfo("tapLan IP address has been set to %s/%d.", inet_ntoa(ipv4addr), msg.netlen);
    }
    return true;
}

bool tapLanGetNodeIPv4ByMAC(const TapLanMACAddress& mac, uint32_t& ipv4addr) {
    auto it = macToNodeMap.find(mac);
    if (it != macToNodeMap.end()) {
        ipv4addr = it->second.ipv4addr;
        return true;
    }
    return false;
}

bool tapLanSetNodeStatusByMAC(const TapLanMACAddress& mac, uint8_t status) {
    auto it = macToNodeMap.find(mac);
    if (it != macToNodeMap.end()) {
        it->second.status = status;
        return true;
    }
    return false;
}

bool tapLanAddNewNode(const sockaddr_in6& sa, const TapLanMACAddress& mac, const uint32_t& ipv4addr, uint8_t status) {
    TapLanNodeInfo fe = {0};
    memcpy(&fe.ipv6addr, &sa.sin6_addr, sizeof(sockaddr_in6));
    fe.ipv6port = sa.sin6_port;
    fe.ipv4addr = ipv4addr;
    memcpy(fe.mac, mac.address, 6);
    fe.status = status;
    macToNodeMap[mac] = fe;
    return true;
}

bool tapLanGetNodeByMAC(const TapLanMACAddress& mac, TapLanNodeInfo& node) {
    auto it = macToNodeMap.find(mac);
    if (it != macToNodeMap.end()) {
        memcpy(&node, &it->second, sizeof(node));
        return true;
    }
    return false;
}
