#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <functional>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include "tapLanServer.hpp"

#define ETHERNET_HEADER_LEN 14
#define IPV4_HEADER_LEN 20
#define IPV6_HEADER_LEN 60


TapLanServer::TapLanServer(uint16_t serverPort) {   // server
    tap_fd = -1;
    udp_fd = -1;
    run_flag = openTapDevice("tapLanServer") && openUdpSocket(serverPort);
}

TapLanServer::~TapLanServer() {
    close(tap_fd);
    close(udp_fd);
}

bool TapLanServer::openTapDevice(const char* devName) {
    tap_fd = open("/dev/net/tun", O_RDWR);
    if (tap_fd == -1) {
        std::cerr << "Error can not open /dev/net/tun" << std::endl;
        return false;
    }
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (devName) {
        strncpy(ifr.ifr_name, devName, IFNAMSIZ);
    }
    if (ioctl(tap_fd, TUNSETIFF, (void*)&ifr) == -1) {
        std::cerr << "ioctl(TUNSETIFF)" << std::endl;
        close(tap_fd);
        return false;
    }
    std::cout << "TAP device " << ifr.ifr_name << " opened successfully" << std::endl;
    return true;
}

bool TapLanServer::openUdpSocket(uint16_t sin6_port) {
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        std::cerr << "Error creating UDP socket." << std::endl;
        return false;
    }
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_any;
    sa.sin6_port = htons(sin6_port);
    if (bind(udp_fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1) {
        std::cerr << "Error binding UDP socket." << std::endl;
        close(udp_fd);
        return false;
    }
    return true;
}

void TapLanServer::readFromTapAndSendToSocket() {
    uint8_t tapTxBuffer[1500];
    int readBytes = read(tap_fd, tapTxBuffer, sizeof(tapTxBuffer));
    if (readBytes >= ETHERNET_HEADER_LEN) {
        struct ether_header* eH = (struct ether_header *)tapTxBuffer;
        uint64_t macDst = 0;
        memcpy(&macDst, eH->ether_dhost, 6);
        auto it = macToIPv6Map.find(macDst);
        if (it != macToIPv6Map.end()) {
            int sentBytes = sendto(udp_fd, tapTxBuffer, readBytes, 0, (struct sockaddr*)&(it->second), sizeof(struct sockaddr_in6));
            if (sentBytes == -1) {
                std::cerr << "Error sending to UDP socket." << std::endl;
            }
        } else {
            std::cerr << "Error can not find mac " << std::hex << macDst << std::endl;
        }
    } else {
        std::cerr << "Error reading from TAP device." << std::endl;
    }
}

void TapLanServer::recvFromSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65536];
    struct sockaddr_in6 ipv6SrcAddr;
    socklen_t ipv6SrcAddrLength = sizeof(ipv6SrcAddr);
    int recvBytes = recvfrom(udp_fd, udpRxBuffer, sizeof(udpRxBuffer), 0, (struct sockaddr*)&ipv6SrcAddr, &ipv6SrcAddrLength);
    if (recvBytes >= ETHERNET_HEADER_LEN) {
        struct ether_header* eH = (struct ether_header *)udpRxBuffer;
        uint64_t macSrc = 0;
        memcpy(&macSrc, eH->ether_shost, 6);
        macToIPv6Map[macSrc] = ipv6SrcAddr;
        int writeBytes = write(tap_fd, udpRxBuffer, recvBytes);
        if (writeBytes == -1) {
            std::cerr << "Error writing to TAP device." << std::endl;
        }
    } else {
        std::cerr << "Error receiving from UDP socket." << std::endl;
    }
}

void TapLanServer::main() {
    struct pollfd pfds[2];
    pfds[0].fd = tap_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = udp_fd;
    pfds[1].events = POLLIN;

    while (run_flag) {
        int ret = poll(pfds, 2, 100); // 超时100ms
        if (ret == -1) {
            std::cerr << "Error in poll()." << std::endl;
            break;
        } else if (ret == 0) {
            // no events timeout
        } else {
            if (pfds[0].revents & POLLIN) {     // tap
                readFromTapAndSendToSocket();
            }
            if (pfds[1].revents & POLLIN) {     // udp
                recvFromSocketAndForwardToTap();
            }
        }
    }
}

void TapLanServer::start() {
    mainThread = std::thread(std::bind(&TapLanServer::main, this));
    std::cout << "TapLanServer is running." << std::endl;
}

void TapLanServer::stop() {
    run_flag = false;
    if (mainThread.joinable())
        mainThread.join();
    std::cout << "TapLanServer has stopped." << std::endl;
}
