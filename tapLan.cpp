#include <functional>
#include <cstring>
#include "tapLan.hpp"

TapLan::TapLan(uint16_t serverPort, uint32_t netID, int netIDLen) {   // server
    isServer = true;
    this->netID = netID;
    this->netIDLen = netIDLen;
    run_flag = openTapDevice() && openUdpSocket(serverPort);
}

TapLan::TapLan(const char* serverAddr, uint16_t serverPort) {
    isServer = false;
    run_flag = openTapDevice() && openUdpSocket(0);
    memset(&gatewayAddr, 0, sizeof(gatewayAddr));
    gatewayAddr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, serverAddr, &gatewayAddr.sin6_addr);
    gatewayAddr.sin6_port = htons(serverPort);
}

TapLan::~TapLan() {
    tapLanCloseTapDevice();
    tapLanCloseUdpSocket();
    tapLanClearRunFlag();
    stop();
}

bool TapLan::openTapDevice() {
    return tapLanOpenTapDevice();
}

bool TapLan::openUdpSocket(uint16_t port) {
    return tapLanOpenUdpSocket(port);
}

void TapLan::readFromTapAndSendToSocket() {
    uint8_t tapRxBuffer[65535];
    while (run_flag) {
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer));
        if (readBytes > ETHERNET_HEADER_LEN) {
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)tapRxBuffer;
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                if (macDst == 0xffffffffffff) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                    }
                } else {
                    auto it = macToIPv6Map.find(macDst);
                    if (it != macToIPv6Map.end()) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&it->second, sizeof(sockaddr_in6));
                    }
                }
            } else {
                ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&gatewayAddr, sizeof(sockaddr_in6));
            }
        } else {
            // read failed
        }
    }
}

void TapLan::recvFromSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65535];
    while (run_flag) {
        struct sockaddr_in6 srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        ssize_t recvBytes = tapLanRecvFromUdpSocket(udpRxBuffer, sizeof(udpRxBuffer), (struct sockaddr*)&srcAddr, &srcAddrLen);
        if (recvBytes == sizeof(TapLanDHCPMessage)) {
            TapLanDHCPMessage msg;
            memcpy(&msg, udpRxBuffer, sizeof(msg));
            if (isServer) {
                if (tapLanHandleDHCPDiscover(netID, netIDLen, msg, (struct sockaddr*)&srcAddr, srcAddrLen)) {
                    uint64_t macSrc = 0;
                    memcpy(&macSrc, msg.mac, 6);
                    macToIPv6Map[macSrc] = srcAddr;
                }
            } else {
                tapLanHandleDHCPOffer(msg);
            }
        } else if (recvBytes > ETHERNET_HEADER_LEN) {
            ssize_t writeBytes = tapLanWriteToTapDevice(udpRxBuffer, recvBytes);
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)udpRxBuffer;
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                if (macDst == 0xffffffffffff) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                    }
                } else {
                    auto it = macToIPv6Map.find(macDst);
                    if (it != macToIPv6Map.end()) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                    }
                }
            }
        } else {
            // recvfrom failed
        }
    }
}

bool TapLan::start() {
    if (!run_flag) return false;
    threadReadFromTapAndSendToSocket = std::thread(std::bind(&TapLan::readFromTapAndSendToSocket, this));
    threadRecvFromSocketAndForwardToTap = std::thread(std::bind(&TapLan::recvFromSocketAndForwardToTap, this));
    if (run_flag) {
        TapLanLogInfo("TapLan %s is running.", (isServer? "server": "client"));
        if (isServer) {
            std::ostringstream cmd;
            struct in_addr ipAddr;
            ipAddr.s_addr = htonl(netID + 1);
#ifdef _WIN32
            cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipAddr) << "/" << +netIDLen;
#else
            cmd << "ip addr flush dev tapLan\n";
            cmd << "ip addr add " << inet_ntoa(ipAddr) << "/" << +netIDLen << " dev tapLan";
#endif
            if (system(cmd.str().c_str())) {
                TapLanLogError("Setting tapLan IP address to %s/%d failed.", inet_ntoa(ipAddr), netIDLen);
            } else {
                TapLanLogInfo("tapLan IP address has been set to %s/%d.", inet_ntoa(ipAddr), netIDLen);
            }
        } else {
            uint8_t* mac = new uint8_t [6];
            tapLanGetMACAddress(mac, 6);
            threadKeepConnectedWithServer = std::thread(tapLanSendDHCPDiscover, mac);
        }
        return true;
    }
    return false;
}

bool TapLan::stop() {
    if (run_flag) return false;
    run_flag = false;
    if (threadReadFromTapAndSendToSocket.joinable())
        threadReadFromTapAndSendToSocket.join();
    if (threadRecvFromSocketAndForwardToTap.joinable())
        threadRecvFromSocketAndForwardToTap.join();
    if (threadKeepConnectedWithServer.joinable())
        threadKeepConnectedWithServer.join();
    TapLanLogInfo("TapLan %s has stopped.", (isServer? "server": "client"));
    return true;
}
