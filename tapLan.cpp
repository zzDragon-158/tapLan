#include "tapLan.hpp"

TapLan::TapLan(uint16_t serverPort, uint32_t netID, int netIDLen, const char* key) {   // server
    isServer = true;
    this->netID = netID;
    this->netIDLen = netIDLen;
    if (isSecurity = strcmp("", key))
        this->key = TapLanKey(key);
    run_flag = tapLanOpenTapDevice() && tapLanOpenUdpSocket(serverPort);
    if (run_flag) {
        myIP = netID + 1;
        tapLanGetMACAddress(myMAC.address, sizeof(myMAC.address));
    }
}

TapLan::TapLan(const char* serverAddr, uint16_t serverPort, const char* key) {
    isServer = false;
    memset(&gatewayAddr, 0, sizeof(gatewayAddr));
    gatewayAddr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, serverAddr, &gatewayAddr.sin6_addr);
    gatewayAddr.sin6_port = htons(serverPort);
    if (isSecurity = strcmp("", key))
        this->key = TapLanKey(key);
    run_flag = tapLanOpenTapDevice() && tapLanOpenUdpSocket(0);
    if (run_flag) {
        myIP = 0;
        tapLanGetMACAddress(myMAC.address, sizeof(myMAC.address));
    }
}

TapLan::~TapLan() {
    stop();
}

void TapLan::readFromTapAndSendToSocket() {
    uint8_t tapRxBuffer[65536];
    while (run_flag) {
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer));
        if (readBytes == -1)
            continue;
        if (readBytes > ETHERNET_HEADER_LEN) {
            ether_header eh;
            memcpy(&eh, tapRxBuffer, sizeof(ether_header));
            if (isSecurity && !tapLanEncryptDataWithAes(tapRxBuffer, (size_t&)readBytes, key))
                continue;
            /* tapRxBuffer may be encrypted, so it is best not to do anything other than forwarding data */
            if (isServer) {
                bool isBroadcast = eh.ether_dhost[0] & 0x01;
                if (isBroadcast) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        if (memcmp(&it->first.address, &eh.ether_shost, 6) != 0)
                            tapLanSendToUdpSocket(tapRxBuffer, readBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                    }
                } else {
                    auto it = macToIPv6Map.find(eh.ether_dhost);
                    if (it != macToIPv6Map.end()) {
                        tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&it->second, sizeof(sockaddr_in6));
                    }
                }
            } else {
                tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&gatewayAddr, sizeof(sockaddr_in6));
            }
        }
    }
}

void TapLan::recvFromSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65536];
    while (run_flag) {
        sockaddr_in6 srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        ssize_t recvBytes = tapLanRecvFromUdpSocket(udpRxBuffer, sizeof(udpRxBuffer), (sockaddr*)&srcAddr, &srcAddrLen);
        if (recvBytes == -1)
            continue;
        if (isSecurity && !tapLanDecryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, key))
            continue;
        if (recvBytes == sizeof(TapLanDHCPMessage)) {
            TapLanDHCPMessage& msg = (TapLanDHCPMessage&)udpRxBuffer;
            size_t msgLen = sizeof(TapLanDHCPMessage);
            if (isServer) {
                if (tapLanHandleDHCPDiscover(netID, netIDLen, msg)) {
                    macToIPv6Map[msg.mac] = srcAddr;
                    if (isSecurity && !tapLanEncryptDataWithAes((uint8_t*)&msg, msgLen, key))
                        continue;
                    tapLanSendToUdpSocket(&msg, msgLen, (sockaddr*)&srcAddr, srcAddrLen);
                }
            } else {
                if (tapLanHandleDHCPOffer(msg))
                    myIP = msg.addr;
            }
        } else if (recvBytes > ETHERNET_HEADER_LEN) {
            ether_header eh;
            memcpy(&eh, udpRxBuffer, sizeof(ether_header));
            bool isSendToMe = !memcmp(&myMAC.address, &eh.ether_dhost, 6);
            bool isBroadcast = (eh.ether_dhost[0] & 0x01);
            if (isSendToMe || isBroadcast) {
                tapLanWriteToTapDevice(udpRxBuffer, recvBytes);
            }
            if (isSecurity && !tapLanEncryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, key))
                continue;
            /* udpRxBuffer may be encrypted, so it is best not to do anything other than forwarding data */
            if (isServer) {
                if (isBroadcast) {
                    for (auto it = macToIPv6Map.begin(); it != macToIPv6Map.end(); ++it) {
                        if (memcmp(&it->first.address, &eh.ether_shost, 6) != 0)
                            tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                    }
                } else if (!isSendToMe) {
                    auto it = macToIPv6Map.find(eh.ether_dhost);
                    if (it != macToIPv6Map.end()) {
                        tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                    }
                }
            }
        }
    }
}

void TapLan::keepConnectedWithServer() {
    uint8_t msgBuffer[256];
    while (run_flag) {
        TapLanDHCPMessage& msg = (TapLanDHCPMessage&)msgBuffer;
        size_t msgLen = sizeof(TapLanDHCPMessage);
        tapLanGenerateDHCPDiscover(myMAC, msg);
        if (isSecurity && !tapLanEncryptDataWithAes((uint8_t*)&msg, msgLen, key))
            continue;
        tapLanSendToUdpSocket(&msg, msgLen, (sockaddr*)&gatewayAddr, sizeof(gatewayAddr));
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (myIP == 0) {
            TapLanDHCPLogError("No TapLanDHCP server found.");
        }
    }
}

void TapLan::showErrorCount() {
    fprintf(stdout, "tapWriteError: %llu [dwc: %llu]\n", tapWriteErrorCnt, dwc);
    fprintf(stdout, "tapReadError: %llu [drc: %llu]\n", tapReadErrorCnt, drc);
    fprintf(stdout, "udpSendError: %llu\n", udpSendErrCnt);
    fprintf(stdout, "udpRecvError: %llu\n", udpRecvErrCnt);
    fflush(stdout);
}

void TapLan::showFIB() {
    fprintf(stdout, "tapLan MAC address    tapLan IP address    Public IP address\n");
  //fprintf(stdout, "00:00:00:00:00:00     255.255.255.255      aaaa:bbbb:cccc:dddd:eeee:ffff:aaaa:bbbb");
    for (const auto& pair : macToIPv6Map) {
        char tapmacbuf[32];
        sprintf(tapmacbuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
            pair.first.address[0], pair.first.address[1], pair.first.address[2], 
            pair.first.address[3], pair.first.address[4], pair.first.address[5]);

        char ipv6buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &pair.second.sin6_addr, ipv6buf, INET6_ADDRSTRLEN);

        char tapipbuf[INET_ADDRSTRLEN];
        uint32_t ipAddr;
        if (tapLanGetHostID(pair.first, ipAddr)) {
            ipAddr += netID;
            sprintf(tapipbuf, "%u.%u.%u.%u", ((ipAddr >> 24) & 0xff), ((ipAddr >> 16) & 0xff),
                ((ipAddr >> 8) & 0xff), (ipAddr & 0xff));
        }

        char buf[128];
        sprintf(buf, "%-22s%-21s[%s]:%u\n", tapmacbuf, tapipbuf, ipv6buf, ntohs(pair.second.sin6_port));
        fprintf(stdout, "%s", buf);
    }
    fflush(stdout);
}

bool TapLan::start() {
    if (!run_flag) return false;
    threadReadFromTapAndSendToSocket = std::thread(std::bind(&TapLan::readFromTapAndSendToSocket, this));
    threadRecvFromSocketAndForwardToTap = std::thread(std::bind(&TapLan::recvFromSocketAndForwardToTap, this));
    if (run_flag) {
        TapLanLogInfo("TapLan %s is running.", (isServer? "server": "client"));
        if (isServer) {
            std::ostringstream cmd;
            in_addr ipAddr;
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
            threadKeepConnectedWithServer = std::thread(std::bind(&TapLan::keepConnectedWithServer, this));
        }
        return true;
    }
    return false;
}

bool TapLan::stop() {
    if (run_flag) return false;
    run_flag = false;
    tapLanCloseTapDevice();
    tapLanCloseUdpSocket();
    if (threadReadFromTapAndSendToSocket.joinable())
        threadReadFromTapAndSendToSocket.join();
    if (threadRecvFromSocketAndForwardToTap.joinable())
        threadRecvFromSocketAndForwardToTap.join();
    if (threadKeepConnectedWithServer.joinable())
        threadKeepConnectedWithServer.join();
    TapLanLogInfo("TapLan %s has stopped.", (isServer? "server": "client"));
    return true;
}
