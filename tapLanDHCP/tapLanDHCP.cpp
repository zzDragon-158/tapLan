#include "tapLanDHCP.hpp"

int32_t last_xid = 0;
uint32_t last_ipaddr = 0;
uint32_t ipAddrStart = 2;
std::unordered_map<TapLanMACAddress, uint32_t> macToHostIDMap;

void tapLanGenerateDHCPDiscover(const TapLanMACAddress& mac, TapLanDHCPMessage& msg) {
    srand((unsigned int)time(0));
    memset(&msg, 0, sizeof(msg));
    msg.xid = rand();
    last_xid = msg.xid;
    msg.op = 1;
    memcpy(msg.mac, mac.address, 6);
}

bool tapLanHandleDHCPDiscover(const uint32_t& netID, const int& netIDLen, TapLanDHCPMessage& msg) {
    if (msg.op != 1 || msg.netIDLen != 0 || msg.addr != 0 || msg.mac[0] & 0x01) {
        return false;
    }
    tapLanGenerateDHCPOffer(netID, netIDLen, msg);
    return true;
}

void tapLanGenerateDHCPOffer(const uint32_t& netID, const int& netIDLen, TapLanDHCPMessage& msg) {
    uint32_t ipAddr = netID;
    auto it = macToHostIDMap.find(msg.mac);
    if (it != macToHostIDMap.end()) {
        ipAddr += it->second;
    } else {
        ipAddr += ipAddrStart;
        macToHostIDMap[msg.mac] = ipAddrStart;
        ++ipAddrStart;
        TapLanDHCPLogInfo("New device connected, macAddress[%.2X:%.2X:%.2X:%.2X:%.2X:%.2X] "
            "ipAddress: %u.%u.%u.%u",
            msg.mac[0], msg.mac[1], msg.mac[2], msg.mac[3], msg.mac[4], msg.mac[5],
            ((ipAddr >> 24) & 0xff), ((ipAddr >> 16) & 0xff), ((ipAddr >> 8) & 0xff), (ipAddr & 0xff));
    }
    msg.op = 2;
    msg.addr = ipAddr;
    msg.netIDLen = netIDLen;
}

bool tapLanHandleDHCPOffer(const TapLanDHCPMessage& msg) {
    if (msg.op != 2 || msg.addr == 0 || msg.xid != last_xid)
        return false;
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
