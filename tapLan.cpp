#include "tapLan.hpp"

TapLan::TapLan(uint16_t serverPort, uint32_t netID, uint8_t netIDLen, const char* key) {   // server
    isServer = true;
    networkID = netID;
    networkIDLen = netIDLen;
    if (isSecurity = strcmp("", key))
        myKey = TapLanKey(key);
    run_flag = tapLanOpenTapDevice()
                && tapLanOpenUdpSocket(serverPort)
                && tapLanOpenTcpSocket(serverPort)
                && tapLanListen(5);
    if (run_flag) {
        myIP = networkID + 1;
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
    ether_header eh;

    sockaddr_in6 dstaddr;
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin6_family = AF_INET6;

    TapLanNodeInfo node;
    while (run_flag) {
        ssize_t readBytes = tapLanReadFromTapDevice(tapRxBuffer, sizeof(tapRxBuffer), 5000);
        if (readBytes <= 0)
            continue;
        if (readBytes <= ETHERNET_HEADER_LEN) {
            continue;
        }
        memcpy(&eh, tapRxBuffer, sizeof(eh));
        if (isSecurity && !tapLanEncryptDataWithAes(tapRxBuffer, (size_t&)readBytes, myKey))
            continue;
        /* tapRxBuffer may be encrypted, so it is best not to do anything other than forwarding data */
        bool isBroadcast = eh.ether_dhost[0] & 0x01;
        if (isServer) {
            if (isBroadcast) {
                for (auto pair: macToNodeMap) {
                    if (DHCP_STATUS_OFFLINE == pair.second.status
                        || 0 == memcmp(&pair.first.address, &eh.ether_shost, 6)) {
                        // 避免广播给离线节点或源节点
                        continue;
                    }
                    memcpy(&dstaddr.sin6_addr, &pair.second.ipv6addr, sizeof(dstaddr.sin6_addr));
                    dstaddr.sin6_port = pair.second.ipv6port;
                    tapLanSendToUdpSocket(tapRxBuffer, readBytes, (sockaddr*)&dstaddr, sizeof(dstaddr));
                }
            } else {
                if (!tapLanGetNodeByMAC(eh.ether_dhost, node) || DHCP_STATUS_OFFLINE == node.status)
                    continue;
                memcpy(&dstaddr.sin6_addr, &node.ipv6addr, sizeof(dstaddr.sin6_addr));
                dstaddr.sin6_port = node.ipv6port;
                tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&dstaddr, sizeof(dstaddr));
            }
        } else {
            if (isDirectSupport && !isBroadcast) {
                // 启用P2P传输模式且该帧不是以太网帧，直接发送给目的主机
                if (!tapLanGetNodeByMAC(eh.ether_dhost, node) || DHCP_STATUS_OFFLINE == node.status)
                    continue;
                memcpy(&dstaddr.sin6_addr, &node.ipv6addr, sizeof(dstaddr.sin6_addr));
                dstaddr.sin6_port = node.ipv6port;
                tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&dstaddr, sizeof(dstaddr));
            } else {
                tapLanSendToUdpSocket(tapRxBuffer, readBytes, (const sockaddr*)&gatewayAddr, sizeof(gatewayAddr));
            }
        }
    }
}

void TapLan::recvFromUdpSocketAndWriteToTap() {
    uint8_t udpRxBuffer[65536];
    ether_header eh;

    sockaddr_in6 srcaddr;
    socklen_t srcAddrLen = sizeof(srcaddr);

    sockaddr_in6 dstaddr;
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin6_family = AF_INET6;

    TapLanNodeInfo node;
    while (run_flag) {
        ssize_t recvBytes = tapLanRecvFromUdpSocket(udpRxBuffer, sizeof(udpRxBuffer), (sockaddr*)&srcaddr, &srcAddrLen);
        if (recvBytes == -1)
            continue;
        if (isSecurity && !tapLanDecryptDataWithAes(udpRxBuffer, (size_t&)recvBytes, myKey)) {
            continue;
        }
        if (recvBytes <= ETHERNET_HEADER_LEN) {
            continue;
        }
        memcpy(&eh, udpRxBuffer, sizeof(eh));
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
                for (auto pair: macToNodeMap) {
                    if (0 == memcmp(&pair.first.address, &eh.ether_shost, 6)
                        || 0 == memcmp(&pair.first.address, &myMAC.address, 6))
                        continue;
                    memcpy(&dstaddr.sin6_addr, &pair.second.ipv6addr, sizeof(dstaddr.sin6_addr));
                    dstaddr.sin6_port = pair.second.ipv6port;
                    tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&dstaddr, sizeof(sockaddr_in6));
                }
            } else if (!isSendToMe) {
                if (!tapLanGetNodeByMAC(eh.ether_dhost, node) || DHCP_STATUS_OFFLINE == node.status)
                    continue;
                memcpy(&dstaddr.sin6_addr, &node.ipv6addr, sizeof(dstaddr.sin6_addr));
                dstaddr.sin6_port = node.ipv6port;
                tapLanSendToUdpSocket(udpRxBuffer, recvBytes, (sockaddr*)&dstaddr, sizeof(sockaddr_in6));
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
    std::vector<TapLanMACAddress> macs;
    pfds.push_back({tcp_fd, POLLIN, 0});
    addrs.push_back({0});
    macs.push_back(myMAC);
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
                macs.push_back(TapLanMACAddress());
            }
        }
        std::vector<int> popList;
        for (int i = 1; pollCnt > 0 && i < pfdsLen; ++i) {
            if (pfds[i].revents != 0) {
                --pollCnt;
                ssize_t recvBytes = tapLanRecvFromTcpSocket(msgBuffer, sizeof(msgBuffer), pfds[i].fd);
                if (recvBytes == 0) {
                    tapLanSetNodeStatusByMAC(macs[i], DHCP_STATUS_OFFLINE);
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
                if (tapLanHandleDHCPDiscover(networkID, networkIDLen, addrs[i], msg, msgLen)) {
                    memcpy(&macs[i].address, &msg.mac, 6);
                    if (isSecurity && !tapLanEncryptDataWithAes((uint8_t*)&msg, msgLen, myKey))
                        continue;
                    tapLanSendToTcpSocket(&msg, msgLen, pfds[i].fd);
                }
            }
        }
        for (int i = popList.size() - 1; i >= 0; --i) {
            TapLanLogInfo("%s is offline.", tapLanIPv6ntos(addrs[popList[i]].sin6_addr).c_str());
            pfds.erase(pfds.begin() + popList[i]);
            addrs.erase(addrs.begin() + popList[i]);
            macs.erase(macs.begin() + popList[i]);
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
                networkIDLen = msg.netlen;
                networkID = msg.ipv4addr & ~((1 << (32 - networkIDLen)) - 1);
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
    for (const auto& pair : macToNodeMap) {
        char tapmacbuf[32];
        sprintf(tapmacbuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
            pair.first.address[0], pair.first.address[1], pair.first.address[2], 
            pair.first.address[3], pair.first.address[4], pair.first.address[5]);

        std::string ipv6str = tapLanIPv6ntos(pair.second.ipv6addr);

        char tapipbuf[INET_ADDRSTRLEN];
        uint32_t ipv4addr = pair.second.ipv4addr;
        sprintf(tapipbuf, "%u.%u.%u.%u", ((ipv4addr >> 24) & 0xff), ((ipv4addr >> 16) & 0xff),
            ((ipv4addr >> 8) & 0xff), (ipv4addr & 0xff));

        char buf[128];
        sprintf(buf, "%-11s%-22s%-21s[%s]:%u\n",
            (DHCP_STATUS_ONLINE == pair.second.status? "ONLINE": "OFFLINE"),
            tapmacbuf, tapipbuf, ipv6str.c_str(), ntohs(pair.second.ipv6port));
        fprintf(stdout, "%s", buf);
    }
    fflush(stdout);
}

bool TapLan::start() {
    if (!run_flag) return false;
    threadReadFromTapAndSendToSocket = std::thread(std::bind(&TapLan::readFromTapAndSendToSocket, this));
    threadRecvFromUdpSocketAndWriteToTap = std::thread(std::bind(&TapLan::recvFromUdpSocketAndWriteToTap, this));
    TapLanLogInfo("TapLan %s is running.", (isServer? "server": "client"));
    if (isServer) {
        std::ostringstream cmd;
        in_addr ipv4addr;
        ipv4addr.s_addr = htonl(myIP);
#ifdef _WIN32
        cmd << "netsh interface ip set address \"tapLan\" static " << inet_ntoa(ipv4addr) << "/" << +networkIDLen;
#else
        cmd << "ip addr flush dev tapLan\n";
        cmd << "ip addr add " << inet_ntoa(ipv4addr) << "/" << +networkIDLen << " dev tapLan";
#endif
        if (system(cmd.str().c_str())) {
            TapLanLogError("Setting tapLan IP address to %s/%d failed.", inet_ntoa(ipv4addr), networkIDLen);
        } else {
            TapLanLogInfo("tapLan IP address has been set to %s/%d.", inet_ntoa(ipv4addr), networkIDLen);
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
    if (threadRecvFromUdpSocketAndWriteToTap.joinable())
        threadRecvFromUdpSocketAndWriteToTap.join();
    if (threadKeepConnectedWithServer.joinable())
        threadKeepConnectedWithServer.join();
    tapLanCloseTapDevice();
    tapLanCloseUdpSocket();
    tapLanCloseTcpSocket();
    TapLanLogInfo("TapLan %s has stopped.", (isServer? "server": "client"));
    return true;
}
