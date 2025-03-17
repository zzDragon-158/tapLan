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
    if (this->run_flag) {
        myIP = netID + 1;
        tapLanGetMACAddress(myMAC.address, 6);
        tapLanAddNewNode({0}, myMAC, myIP, DHCP_STATUS_ONLINE);
    }
}

TapLan::TapLan(const char* serverAddr, uint16_t serverPort, const char* key, bool isDirectSupport) {
    isServer = false;
    this->isDirectSupport = isDirectSupport;
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
        tapLanGetMACAddress(myMAC.address, 6);
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
                for (auto it = macToSA6Map.begin(); it != macToSA6Map.end(); ++it) {
                    if (DHCP_STATUS_OFFLINE == tapLanGetNodeStatusByIPv6(tapLanIPv6ntos(it->second.sin6_addr))
                        || memcmp(&it->first.address, &eh.ether_shost, 6))
                        continue;
                    tapLanSendToUdpSocket(tapRxBuffer, readBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                }
            } else {
                sockaddr_in6 dstAddr;
                if (tapLanGetSA6ByMAC(eh.ether_dhost, dstAddr)
                    && DHCP_STATUS_ONLINE == tapLanGetNodeStatusByIPv6(tapLanIPv6ntos(dstAddr.sin6_addr))) {
                    tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&dstAddr, sizeof(sockaddr_in6));
                }
            }
        } else {
            if (isDirectSupport && !isBroadcast) {
                sockaddr_in6 dstAddr;
                if (tapLanGetSA6ByMAC(eh.ether_dhost, dstAddr)
                    && DHCP_STATUS_ONLINE == tapLanGetNodeStatusByIPv6(tapLanIPv6ntos(dstAddr.sin6_addr))) {
                    tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&dstAddr, sizeof(sockaddr_in6));
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
        if (isSecurity && !tapLanDecryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, myKey)) {
            continue;
        }
        if (recvBytes <= ETHERNET_HEADER_LEN) {
            continue;
        }
        ether_header eh;
        memcpy(&eh, udpRxBuffer, sizeof(ether_header));
        if (DHCP_STATUS_OFFLINE == tapLanGetNodeStatusByIPv6(tapLanIPv6ntos(srcAddr.sin6_addr)))
            continue;
        bool isBroadcast = (eh.ether_dhost[0] & 0x01);
        bool isSendToMe = isBroadcast || (memcmp(&myMAC.address, &eh.ether_dhost, 6) == 0);
        if (isSendToMe) {
            tapLanWriteToTapDevice(udpRxBuffer, recvBytes);
        }
        if (isSecurity && !tapLanEncryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, myKey))
            continue;
        /* udpRxBuffer may be encrypted, so it is best not to do anything other than forwarding data */
        if (isServer) {
            if (isBroadcast) {
                for (auto it = macToSA6Map.begin(); it != macToSA6Map.end(); ++it) {
                    if (memcmp(&it->first.address, &eh.ether_shost, 6) == 0
                        || memcmp(&it->first.address, &myMAC.address, 6) == 0)
                        continue;
                    tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&(it->second), sizeof(sockaddr_in6));
                }
            } else if (!isSendToMe) {
                sockaddr_in6 dstAddr;
                if (tapLanGetSA6ByMAC(eh.ether_dhost, dstAddr)) {
                    tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&dstAddr, sizeof(sockaddr_in6));
                }
            }
        } else {
            // client support forward
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
            if (client != -1) {
                TapLanLogInfo("%s is online.", tapLanIPv6ntos(clientAddr.sin6_addr).c_str());
                pfds.push_back({client, POLLIN, 0});
                addrs.push_back(clientAddr);
            }
        }
        std::vector<int> popList;
        for (int i = 1; pollCnt > 0 && i < pfdsLen; ++i) {
            if (pfds[i].revents != 0) {
                --pollCnt;
                ssize_t recvBytes = tapLanRecvFromTcpSocket(msgBuffer, sizeof(msgBuffer), pfds[i].fd);
                if (recvBytes == 0) {
                    tapLanSetNodeStatusByIPv6(tapLanIPv6ntos(addrs[i].sin6_addr), DHCP_STATUS_OFFLINE);
                    tapLanCloseTcpSocket(pfds[i].fd);
                    popList.push_back(i);
                    continue;
                } else if (recvBytes == -1) {
                    continue;
                }
                if (isSecurity && !tapLanDecryptDataWithAes(msgBuffer, (size_t&)recvBytes, myKey)) {
                    // maybe client's key is incorrect, so close tcp socket
                    tapLanCloseTcpSocket(pfds[i].fd);
                    popList.push_back(i);
                    continue;
                }
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
            TapLanLogInfo("%s is offline.", ipv6buf);
            pfds.erase(pfds.begin() + popList[i]);
            addrs.erase(addrs.begin() + popList[i]);
        }
    }
}

void TapLan::handleDHCPMsgClient() {
    bool isConnected = false;
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
                myIP = msg.ipv4addr;
                netIDLen = msg.netIDLen;
                netID = msg.ipv4addr & ~((1 << (32 - netIDLen)) - 1);
                isConnected = true;
            }
        } else if (recvBytes == 0) {
            if (!isConnected) {
                TapLanDHCPLogError("Trying to connect to server failed, maybe your input password is incorrect.");
            } else {
                isConnected = false;
                TapLanLogInfo("Server has offline.");
                TapLanLogInfo("Trying to reconnect......");
            }
            auto isConnectToServer = []() {
                tapLanCloseTcpSocket();
                if (tapLanOpenTcpSocket(ntohs(gatewayAddr.sin6_port))
                    && tapLanConnect((const sockaddr*)&gatewayAddr, sizeof(gatewayAddr)))
                    return true;
                return false;
            };
            while (run_flag) {
                if (isConnectToServer()) {
                    isConnected = true;
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
    fprintf(stdout, "tapWriteError:             %lu\n", tapWriteErrorCnt);
    fprintf(stdout, "tapReadError:              %lu\n", tapReadErrorCnt);
    fprintf(stdout, "udpSendError:              %lu\n", udpSendErrCnt);
    fprintf(stdout, "udpRecvError:              %lu\n", udpRecvErrCnt);
    fprintf(stdout, "encryptDataErrCntError:    %lu\n", encryptDataErrCnt);
    fprintf(stdout, "encryptDataErrCntError:    %lu\n", decryptDataErrCnt);
    fflush(stdout);
}

void TapLan::showFIB() {
    fprintf(stdout, "status     tapLan MAC address    tapLan IP address    Public IP address\n");
  //fprintf(stdout, "offline    00:00:00:00:00:00     255.255.255.255      aaaa:bbbb:cccc:dddd:eeee:ffff:aaaa:bbbb");
    for (const auto& pair : macToSA6Map) {
        char tapmacbuf[32];
        sprintf(tapmacbuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
            pair.first.address[0], pair.first.address[1], pair.first.address[2], 
            pair.first.address[3], pair.first.address[4], pair.first.address[5]);

        char ipv6buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &pair.second.sin6_addr, ipv6buf, INET6_ADDRSTRLEN);

        char tapipbuf[INET_ADDRSTRLEN];
        uint32_t ipAddr;
        if (tapLanGetIPv4ByMAC(pair.first, ipAddr)) {
            sprintf(tapipbuf, "%u.%u.%u.%u", ((ipAddr >> 24) & 0xff), ((ipAddr >> 16) & 0xff),
                ((ipAddr >> 8) & 0xff), (ipAddr & 0xff));
        }

        char buf[128];
        sprintf(buf, "%-11s%-22s%-21s[%s]:%u\n",
            (tapLanGetNodeStatusByIPv6(tapLanIPv6ntos(pair.second.sin6_addr)) == DHCP_STATUS_ONLINE? "ONLINE": "OFFLINE"),
            tapmacbuf, tapipbuf, ipv6buf, ntohs(pair.second.sin6_port));
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
        ipAddr.s_addr = htonl(myIP);
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
