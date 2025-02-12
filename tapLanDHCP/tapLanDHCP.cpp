#include "tapLanDHCP.hpp"

bool run_flag = true;
uint32_t ipAddrStart = 2;
uint16_t xid = 0;
uint32_t last_ipaddr = 0;
std::unordered_map<uint64_t, uint32_t> macToHostIDMap;

bool tapLanSendDHCPDiscover(uint8_t* macAddress) {
    struct TapLanDHCPMessage msg;
    srand((unsigned int)time(0));
    xid = rand() % 65532 + 2;
    while (run_flag) {
        memset(&msg, 0, sizeof(msg));
        msg.xid = xid;
        msg.op = 1;
        memcpy(msg.mac, macAddress, 6);
        ssize_t sendBytes = tapLanSendToUdpSocket(&msg, sizeof(msg), (sockaddr*)&gatewayAddr, sizeof(gatewayAddr));
        if (sendBytes != sizeof(msg)) {
            fprintf(stderr, "[ERROR] sendDHCPDiscover\n");
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (last_ipaddr == 0) fprintf(stderr, "[ERROR] not found server\n");
    }
    delete [] macAddress;
    return true;
}

void tapLanClearRunFlag() 
{
    run_flag = false;
}

bool tapLanHandleDHCPDiscover(uint32_t netID, int netIDLen, struct TapLanDHCPMessage& msg, const struct sockaddr* dstAddr, socklen_t addrLen) {
    if (msg.op != 1 || msg.netIDLen != 0 || msg.addr != 0) {
        return false;
    }
    uint64_t mac = 0;
    memcpy(&mac, msg.mac, 6);
    uint32_t ipAddr = netID;
    auto it = macToHostIDMap.find(mac);
    if (it != macToHostIDMap.end()) {
        ipAddr += it->second;
    } else {
        ipAddr += ipAddrStart;
        macToHostIDMap[mac] = ipAddrStart;
        printf("[INFO] mac[%llX] hostID: %u\n", mac, ipAddrStart);
        ++ipAddrStart;
    }
    msg.op = 2;
    msg.addr = ipAddr;
    msg.netIDLen = netIDLen;
    ssize_t sendBytes = tapLanSendToUdpSocket(&msg, sizeof(msg), dstAddr, addrLen);
    if (sendBytes != sizeof(msg)) {
        return false;
    }
    mac = 0;
    memcpy(&mac, msg.mac, 6);
    return true;
}

bool tapLanHandleDHCPOffer(struct TapLanDHCPMessage& msg) {
    if (msg.op != 2 || msg.addr == 0 || msg.xid != xid)
        return false;
    std::ostringstream cmd;
    if (last_ipaddr != msg.addr) {
        last_ipaddr = msg.addr;
        struct in_addr ipAddr;
        ipAddr.s_addr = htonl(msg.addr);
#ifdef _WIN32
        cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipAddr) << "/" << +msg.netIDLen;
#else
        cmd << "ip addr flush dev tapLan\n";
        cmd << "ip addr add " << inet_ntoa(ipAddr) << "/" << +msg.netIDLen << " dev tapLan";
#endif
        system(cmd.str().c_str());
        printf("[INFO] your tapLan ip addr has been set to %s/%d\n", inet_ntoa(ipAddr), msg.netIDLen);
    } else {
        return false;
    }
    return true;
}
