#include <functional>
#include <cstring>
#include "tapLan.hpp"

TapLan::TapLan(uint16_t serverPort, uint32_t netID, int netIDLen) {   // server
    isServer = true;
    this->netID = netID;
    this->netIDLen = netIDLen;
    run_flag = openTapDevice("tapLan") && openUdpSocket(serverPort);
}

TapLan::TapLan(const char* serverAddr, uint16_t serverPort) {
    isServer = false;
    run_flag = openTapDevice("tapLan") && openUdpSocket(0);
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

bool TapLan::openTapDevice(const char* devName) {
    return tapLanOpenTapDevice(devName);
}

bool TapLan::openUdpSocket(uint16_t port) {
    return tapLanOpenUdpSocket(port);
}

void TapLan::readFromTapAndSendToSocket() {
    uint8_t tapRxBuffer[65535];
    while (run_flag) {
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer));
        if (readBytes > ETHERNET_HEADER_LEN) {
            sockaddr_in6* pDstAddr = nullptr;
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)tapRxBuffer;
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                if (macDst == 0xffffffffffff) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                        if (sendBytes < readBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                } else {
                    auto it = macToIPv6Map.find(macDst);
                    if (it != macToIPv6Map.end()) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&it->second, sizeof(sockaddr_in6));
                        if (sendBytes < readBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                }
            } else {
                ssize_t sendBytes = tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&gatewayAddr, sizeof(sockaddr_in6));
                if (sendBytes < readBytes) {
                    fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                }
            }
        } else {
            fprintf(stderr, "[ERROR] reading from TAP device, readBytes %ld\n", readBytes);
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
            if (writeBytes < recvBytes) {
                fprintf(stderr, "[ERROR] writing to TAP device, writeBytes %ld\n", writeBytes);
            }
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)udpRxBuffer;
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                if (macDst == 0xffffffffffff) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                        if (sendBytes < recvBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                } else {
                    auto it = macToIPv6Map.find(macDst);
                    if (it != macToIPv6Map.end()) {
                        ssize_t sendBytes = tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                        if (sendBytes < recvBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                }
            }
        } else {
            fprintf(stderr, "[ERROR] receiving from UDP socket, recvBytes %ld\n", recvBytes);
        }
    }
}

bool TapLan::start() {
    if (!run_flag) return false;
    threadReadFromTapAndSendToSocket = std::thread(std::bind(&TapLan::readFromTapAndSendToSocket, this));
    threadRecvFromSocketAndForwardToTap = std::thread(std::bind(&TapLan::recvFromSocketAndForwardToTap, this));
    if (run_flag) {
        printf("[INFO] TapLan %s is running\n", (isServer? "server": "client"));
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
            system(cmd.str().c_str());
            printf("[INFO] your tapLan ip addr has been set to %s/%d\n", inet_ntoa(ipAddr), netIDLen);
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
    printf("[INFO] TapLan %s has stopped\n", (isServer? "server": "client"));
    return true;
}
