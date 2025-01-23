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
#include "TapLanClient.hpp"

#define ETHERNET_HEADER_LEN 14
#define IPV4_HEADER_LEN 20
#define IPV6_HEADER_LEN 60

TapLanClient::TapLanClient(const char* serverAddr, uint16_t serverPort, const uint16_t clientPort) {
    tap_fd = -1;
    udp_fd = -1;
    run_flag = openTapDevice("tapLan") && openUdpSocket(clientPort);
    memset(&gatewayAddr, 0, sizeof(gatewayAddr));
    gatewayAddr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, serverAddr, &gatewayAddr.sin6_addr);
    gatewayAddr.sin6_port = htons(serverPort);
}

TapLanClient::~TapLanClient() {
    // todo
    close(tap_fd);
    close(udp_fd);
}

bool TapLanClient::openTapDevice(const char* devName) {
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

bool TapLanClient::openUdpSocket(uint16_t sin6_port) {
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

void TapLanClient::readFromTapAndSendToSocket() {
    uint8_t tapTxBuffer[1500];
    int readBytes = read(tap_fd, tapTxBuffer, sizeof(tapTxBuffer));
    if (readBytes > 0) {
        int sentBytes = sendto(udp_fd, tapTxBuffer, readBytes, 0, reinterpret_cast<sockaddr*>(&gatewayAddr), sizeof(gatewayAddr));
        if (sentBytes == -1) {
            std::cerr << "Error sending to UDP socket." << std::endl;
        }
    } else {
        std::cerr << "Error reading from TAP device." << std::endl;
    }
}

void TapLanClient::recvFromSocketAndForwardToTap() {
    uint8_t udpRxBuffer[65536];
    int recvBytes = recvfrom(udp_fd, udpRxBuffer, sizeof(udpRxBuffer), 0, NULL, NULL);
    if (recvBytes > 0) {
        int writeBytes = write(tap_fd, udpRxBuffer, recvBytes);
        if (writeBytes == -1) {
            std::cerr << "Error writing to TAP device." << std::endl;
        }
    } else {
        std::cerr << "Error receiving from UDP socket." << std::endl;
    }
}

void TapLanClient::main() {
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

void TapLanClient::start() {
    mainThread = std::thread(std::bind(&TapLanClient::main, this));
    std::cout << "TapLanClient is running." << std::endl;
}

void TapLanClient::stop() {
    run_flag = false;
    if (mainThread.joinable())
        mainThread.join();
    std::cout << "TapLanClient has stopped." << std::endl;
}
