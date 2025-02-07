#include <functional>
#include <cstring>
#include "tapLan.hpp"

static sockaddr_in6 gatewayAddr;

TapLan::TapLan(uint16_t serverPort) {   // server
    isServer = true;
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
    tapLanCloseUdpIPv6Socket();
}

bool TapLan::openTapDevice(const char* devName) {
    return tapLanOpenTapDevice(devName);
}

bool TapLan::openUdpSocket(uint16_t sin6_port) {
    return tapLanOpenUdpIPv6Socket(sin6_port);
}

void TapLan::readFromTapAndSendToSocket() {
    uint8_t tapRxBuffer[65535];
    while (run_flag) {
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer), 100);
        if (readBytes >= ETHERNET_HEADER_LEN) {
            sockaddr_in6* pDstAddr = nullptr;
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)tapRxBuffer;
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                auto it = macToIPv6Map.find(macDst);
                if (it != macToIPv6Map.end()) {
                    pDstAddr = &(it->second);
                } else {
                    // fprintf(stderr, "ERROR can not find mac %X\n", macDst);
                }
            } else {
                pDstAddr = &gatewayAddr;
            }
            if (pDstAddr) {
                ssize_t sendBytes = tapLanSendToUdpIPv6Socket(tapRxBuffer, readBytes, (const sockaddr*)pDstAddr, sizeof(sockaddr_in6));
                if (sendBytes < readBytes) {
                    fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                }
            }
        } else if (readBytes == -100) {
            // timeout
            continue;
        } else {
            fprintf(stderr, "[ERROR] reading from TAP device, readBytes %d\n", readBytes);
        }
    }
}

void TapLan::recvFromSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65535];
    while (run_flag) {
        struct sockaddr_in6 srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        ssize_t recvBytes = tapLanRecvFromUdpIPv6Socket(udpRxBuffer, sizeof(udpRxBuffer), (struct sockaddr*)&srcAddr, &srcAddrLen, 100);
        if (recvBytes >= 0) {
            ssize_t writeBytes = tapLanWriteToTapDevice(udpRxBuffer, recvBytes);
            if (writeBytes < recvBytes) {
                fprintf(stderr, "[ERROR] writing to TAP device, writeBytes %ld\n", writeBytes);
            }
            if (isServer) {
                struct ether_header* eH = (struct ether_header *)udpRxBuffer;
                uint64_t macSrc = 0;
                memcpy(&macSrc, eH->ether_shost, 6);
                if (eH->ether_type == htons(ETHERTYPE_ARP)) {
                    macToIPv6Map[macSrc] = srcAddr;
                }
                uint64_t macDst = 0;
                memcpy(&macDst, eH->ether_dhost, 6);
                if (macDst == 0xffffffffffff) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        ssize_t sendBytes = tapLanSendToUdpIPv6Socket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                        if (sendBytes < recvBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                } else {
                    auto it = macToIPv6Map.find(macDst);
                    if (it != macToIPv6Map.end()) {
                        int sendBytes = tapLanSendToUdpIPv6Socket(udpRxBuffer, recvBytes, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
                        if (sendBytes < recvBytes) {
                            fprintf(stderr, "[ERROR] sending to UDP socket, sendBytes %ld\n", sendBytes);
                        }
                    }
                }
            }
        } else if (recvBytes == -100) {
            // timeout
            continue;
        } else {
            fprintf(stderr, "[ERROR] receiving from UDP socket, recvBytes %d\n", recvBytes);
        }
    }
}

bool TapLan::start() {
    if (!run_flag) return false;
    threadReadFromTapAndSendToSocket = std::thread(std::bind(&TapLan::readFromTapAndSendToSocket, this));
    threadRecvFromSocketAndForwardToTap = std::thread(std::bind(&TapLan::recvFromSocketAndForwardToTap, this));
    printf("[INFO] TapLan %s is running\n", (isServer? "server": "client"));
    return true;
}

bool TapLan::stop() {
    if (run_flag) return false;
    run_flag = false;
    if (threadReadFromTapAndSendToSocket.joinable())
        threadReadFromTapAndSendToSocket.join();
    if (threadRecvFromSocketAndForwardToTap.joinable())
        threadRecvFromSocketAndForwardToTap.join();
    printf("[INFO] TapLan %s has stopped\n", (isServer? "server": "client"));
    return true;
}
