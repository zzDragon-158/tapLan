#include "tapLan.hpp"

TapLan::TapLan(uint16_t serverPort, uint32_t netID, int netIDLen, const char* key) {   // server
    isServer = true;
    this->netID = netID;
    this->netIDLen = netIDLen;
    if (isSecurity = strcmp("", key))
        myKey = TapLanKey(key);
    run_flag = tapLanOpenTapDevice()
               && tapLanOpenUdpSocket(serverPort)
               && tapLanOpenTcpSocket(serverPort)
               && tapLanListen(5);
    if (run_flag) {
        myIP = netID + 1;
        tapLanGetMACAddress(myMAC.address, sizeof(myMAC.address));
        macToIPv6Map[myMAC] = {0};
        macToHostIDMap[myMAC] = 1;
    }
}

TapLan::TapLan(const char* serverAddr, uint16_t serverPort, const char* key) {
    isServer = false;
    isForwardSupport = true;
    memset(&gatewayAddr, 0, sizeof(gatewayAddr));
    gatewayAddr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, serverAddr, &gatewayAddr.sin6_addr);
    gatewayAddr.sin6_port = htons(serverPort);
    if (isSecurity = strcmp("", key))
        myKey = TapLanKey(key);
    run_flag = tapLanOpenTapDevice()
               && tapLanOpenUdpSocket(serverPort)
               && tapLanOpenTcpSocket(serverPort)
               && tapLanConnect((const sockaddr*)&gatewayAddr, sizeof(gatewayAddr));
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
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer), 5000);
        if (readBytes <= 0)
            continue;
        if (readBytes <= ETHERNET_HEADER_LEN) {
            continue;
        }
        ether_header eh;
        memcpy(&eh, tapRxBuffer, sizeof(ether_header));
        if (isSecurity && !tapLanEncryptDataWithAes(tapRxBuffer, (size_t&)readBytes, myKey))
            continue;
        /* tapRxBuffer may be encrypted, so it is best not to do anything other than forwarding data */
        bool isBroadcast = eh.ether_dhost[0] & 0x01;
        if (isServer) {
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
            if (isForwardSupport && !isBroadcast) {
                auto it = macToIPv6Map.find(eh.ether_dhost);
                if (it != macToIPv6Map.end()) {
                    tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&it->second, sizeof(sockaddr_in6));
                }
            } else {
                tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&gatewayAddr, sizeof(sockaddr_in6));
            }
        }
    }
}

void TapLan::recvFromUdpSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65536];
    TapLanPollFD pfd = {udp_fd, POLLIN, 0};
    while (run_flag) {
        if (TapLanPoll(&pfd, 1, 5000) <= 0) 
            continue;
        sockaddr_in6 srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        ssize_t recvBytes = tapLanRecvFromUdpSocket(udpRxBuffer, sizeof(udpRxBuffer), (sockaddr*)&srcAddr, &srcAddrLen);
        if (recvBytes == -1)
            continue;
        if (isSecurity && !tapLanDecryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, myKey))
            continue;
        if (recvBytes <= ETHERNET_HEADER_LEN) {
            continue;
        }
        ether_header eh;
        memcpy(&eh, udpRxBuffer, sizeof(ether_header));
        auto it = macToIPv6Map.find(eh.ether_shost);
        if (it == macToIPv6Map.end())
            continue;
        bool isSendToMe = !memcmp(&myMAC.address, &eh.ether_dhost, 6);
        bool isBroadcast = (eh.ether_dhost[0] & 0x01);
        if (isSendToMe || isBroadcast) {
            tapLanWriteToTapDevice(udpRxBuffer, recvBytes);
        }
        if (isSecurity && !tapLanEncryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, myKey))
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
        } else {
            if (!isBroadcast && !isSendToMe) {
                auto it = macToIPv6Map.find(eh.ether_dhost);
                if (it != macToIPv6Map.end()) {
                    tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                }
            }
        }
    }
}

void TapLan::handleDHCPMsgServer() {
    uint8_t msgBuffer[65536];
    std::vector<TapLanPollFD> pfds;
    std::vector<sockaddr_in6> addrs;
    pfds.push_back({tcp_fd, POLLIN, 0});
    addrs.push_back({0});
    while (run_flag) {
        int pollCnt = TapLanPoll(pfds.data(), pfds.size(), 5000);
        if (pollCnt < 0) {
            TapLanSocketLogError("TapLanPoll failed.");
            continue;
        }
        size_t pfdsLen = pfds.size();
        if (pfds[0].revents != 0) {
            --pollCnt;
            sockaddr_in6 clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            TapLanSocket client = tapLanAccept((sockaddr*)&clientAddr, &clientAddrLen);
            pfds.push_back({client, POLLIN, 0});
            addrs.push_back(clientAddr);
        }
        std::vector<int> popList;
        for (int i = 1; pollCnt > 0 && i < pfdsLen; ++i) {
            if (pfds[i].revents != 0) {
                --pollCnt;
                ssize_t recvBytes = tapLanRecvFromTcpSocket(msgBuffer, sizeof(msgBuffer), pfds[i].fd);
                if (recvBytes == 0) {
                    tapLanCloseTcpSocket(pfds[i].fd);
                    popList.push_back(i);
                    continue;
                } else if (recvBytes == -1) {
                    continue;
                }
                if (isSecurity && !tapLanDecryptDataWithAes(msgBuffer, (size_t&)recvBytes, myKey))
                    continue;
                TapLanDHCPMessage& msg = (TapLanDHCPMessage&)msgBuffer;
                size_t msgLen = sizeof(TapLanDHCPMessage);
                if (tapLanHandleDHCPDiscover(netID, netIDLen, addrs[i], msg, msgLen)) {
                    if (isSecurity && !tapLanEncryptDataWithAes((uint8_t*)&msg, msgLen, myKey))
                        continue;
                    tapLanSendToTcpSocket(&msg, msgLen, pfds[i].fd);
                }
            }
        }
        for (int i = popList.size() - 1; i >= 0; --i) {
            char ipv6buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addrs[popList[i]].sin6_addr, ipv6buf, INET6_ADDRSTRLEN);
            TapLanLogInfo("%s has offline.", ipv6buf);
            pfds.erase(pfds.begin() + popList[i]);
            addrs.erase(addrs.begin() + popList[i]);
        }
    }
}

void TapLan::handleDHCPMsgClient() {
    uint8_t msgBuffer[65536];
    TapLanPollFD pfd = {tcp_fd, POLLIN, 0};
    while (run_flag) {
        // generate dhcp discover
        TapLanDHCPMessage& msg = (TapLanDHCPMessage&)msgBuffer;
        size_t msgLen = sizeof(TapLanDHCPMessage);
        tapLanGenerateDHCPDiscover(myMAC, msg);
        if (isSecurity && !tapLanEncryptDataWithAes((uint8_t*)&msg, msgLen, myKey))
            continue;
        ssize_t sendBytes = tapLanSendToTcpSocket(&msg, msgLen);
        // handle dhcp offer
        if (TapLanPoll(&pfd, 1, 5000) <= 0) {
            TapLanDHCPLogError("Server response timeout.");
            continue;
        }
        ssize_t recvBytes = tapLanRecvFromTcpSocket(&msg, 65536);
        if (recvBytes > 0) {
            if (isSecurity && !tapLanDecryptDataWithAes(msgBuffer, (size_t&)recvBytes, myKey))
                continue;
            if (tapLanHandleDHCPOffer(msg)) {
                myIP = msg.addr;
                netIDLen = msg.netIDLen;
                netID = msg.addr & ~((1 << (32 - netIDLen)) - 1);
            }
        } else if (recvBytes == 0) {
            TapLanLogInfo("Server has offline.");
            TapLanLogInfo("Trying to reconnect......");
            auto isConnectToServer = []() {
                tapLanCloseTcpSocket();
                if (tapLanOpenTcpSocket(ntohs(gatewayAddr.sin6_port))
                    && tapLanConnect((const sockaddr*)&gatewayAddr, sizeof(gatewayAddr)))
                    return true;
                return false;
            };
            while (true) {
                if (isConnectToServer()) {
                    TapLanLogInfo("Reconnecting to server successful.");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        } else {
            // todo
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
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
    threadRecvFromUdpSocketAndForwardToTap = std::thread(std::bind(&TapLan::recvFromUdpSocketAndForwardToTap, this));
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
        threadKeepConnectedWithServer = std::thread(std::bind(&TapLan::handleDHCPMsgServer, this));
    } else {
        threadKeepConnectedWithServer = std::thread(std::bind(&TapLan::handleDHCPMsgClient, this));
    }
    return true;
}

bool TapLan::stop() {
    if (!run_flag) return false;
    run_flag = false;
    if (threadReadFromTapAndSendToSocket.joinable())
        threadReadFromTapAndSendToSocket.join();
    if (threadRecvFromUdpSocketAndForwardToTap.joinable())
        threadRecvFromUdpSocketAndForwardToTap.join();
    if (threadKeepConnectedWithServer.joinable())
        threadKeepConnectedWithServer.join();
    tapLanCloseTapDevice();
    tapLanCloseUdpSocket();
    tapLanCloseTcpSocket();
    TapLanLogInfo("TapLan %s has stopped.", (isServer? "server": "client"));
    return true;
}
