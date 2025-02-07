#include "tapLanSocket.hpp"

const int udpBufferSize = 1024 * 1024 * 16;

#ifdef _WIN32
static int udp_fd;

bool tapLanOpenUdpIPv6Socket(uint16_t sin6_port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed\n");
        return false;
    }
    udp_fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd == INVALID_SOCKET) {
        fprintf(stderr, "[ERROR] can not create socket with WSAGetLastError %d\n", WSAGetLastError());
        return false;
    }
    /* bind udp socket */ {
        sockaddr_in6 sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = in6addr_any;
        sa.sin6_port = htons(sin6_port);
        char ipv6Addr[INET6_ADDRSTRLEN] = {'\0'};
        inet_ntop(AF_INET6, &sa.sin6_addr, ipv6Addr, sizeof(ipv6Addr));
        if (bind(udp_fd, (sockaddr*)(&sa), sizeof(sockaddr_in6)) == SOCKET_ERROR) {
            fprintf(stderr, "[ERROR] can not bind to %s:%u with WSAGetLastError %d\n", ipv6Addr, ntohs(sa.sin6_port), WSAGetLastError());
            closesocket(udp_fd);
            return false;
        }
        if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
            fprintf(stderr, "[ERROR] can not set udpRecvBufferSize to %d\n", udpBufferSize);
        } else {
            int actualUdpRecvBufferSizeSize;
            int actualUdpRecvBufferSizeSizeLen = sizeof(actualUdpRecvBufferSizeSize);
            if (getsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&actualUdpRecvBufferSizeSize, &actualUdpRecvBufferSizeSizeLen) == 0) {
                printf("[INFO] udpRecvBufferSize has been set to %d\n", actualUdpRecvBufferSizeSize);
            }
        }
        if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
            fprintf(stderr, "[ERROR] can not set udpSendBufferSize to %d\n", udpBufferSize);
        } else {
            int actualUdpSendBufferSizeSize;
            int actualUdpSendBufferSizeSizeLen = sizeof(actualUdpSendBufferSizeSize);
            if (getsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&actualUdpSendBufferSizeSize, &actualUdpSendBufferSizeSizeLen) == 0) {
                printf("[INFO] udpSendBufferSize has been set to %d\n", actualUdpSendBufferSizeSize);
            }
        }
    }
    /* windows bug: udp socket 10054 */ {
        BOOL bEnalbeConnRestError = FALSE;
        DWORD dwBytesReturned = 0;
        if (WSAIoctl(udp_fd, _WSAIOW(IOC_VENDOR, 12), &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), nullptr, 0, &dwBytesReturned, nullptr, nullptr)) {
            fprintf(stderr, "[ERROR] WSAIoctl failed\n");
        }
    }
    /* print addr */ {
        sockaddr_in6 sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        int sockAddrLen = sizeof(sockAddr);
        if (getsockname(udp_fd, (sockaddr*)(&sockAddr), &sockAddrLen)) {
            fprintf(stderr, "[ERROR] can not getsockname\n");
            return false;
        }
        char ipv6Addr[INET6_ADDRSTRLEN] = {'\0'};
        inet_ntop(AF_INET6, &sockAddr.sin6_addr, ipv6Addr, sizeof(ipv6Addr));
        printf("[INFO] create udp socket bind to [%s]:%u\n", ipv6Addr, ntohs(sockAddr.sin6_port));
    }
    return true;
}

bool tapLanCloseUdpIPv6Socket() {
    closesocket(udp_fd);
    WSACleanup();
    return true;
}

ssize_t tapLanSendToUdpIPv6Socket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen) {
    return sendto(udp_fd, (const char*)buf, bufLen, 0, dstAddr, addrLen);
}

ssize_t tapLanRecvFromUdpIPv6Socket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen) {
    return recvfrom(udp_fd, (char*)buf, bufLen, 0, srcAddr, addrLen);
}

#else
static int udp_fd;

bool tapLanOpenUdpIPv6Socket(uint16_t sin6_port) {
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        fprintf(stderr, "[Error] creating UDP socket\n");
        return false;
    }
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_any;
    sa.sin6_port = htons(sin6_port);
    if (bind(udp_fd, (sockaddr*)(&sa), sizeof(sa)) == -1) {
        fprintf(stderr, "[Error] binding UDP socket\n");
        close(udp_fd);
        return false;
    }
    /* print addr */ {
        sockaddr_in6 sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        socklen_t sockAddrLen = sizeof(sockAddr);
        if (getsockname(udp_fd, (sockaddr*)(&sockAddr), &sockAddrLen)) {
            fprintf(stderr, "[Error] can not getsockname\n");
            return false;
        }
        char ipv6Addr[INET6_ADDRSTRLEN] = {'\0'};
        inet_ntop(AF_INET6, &sockAddr.sin6_addr, ipv6Addr, sizeof(ipv6Addr));
        printf("[INFO] create udp socket bind to [%s]:%u\n", ipv6Addr, ntohs(sockAddr.sin6_port));
    }
    if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
        fprintf(stderr, "[Error] can not set udpRecvBufferSize to %d\n", udpBufferSize);
    } else {
        int actualUdpRecvBufferSizeSize;
        socklen_t actualUdpRecvBufferSizeSizeLen = sizeof(actualUdpRecvBufferSizeSize);
        if (getsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&actualUdpRecvBufferSizeSize, &actualUdpRecvBufferSizeSizeLen) == 0) {
            printf("[INFO] udpRecvBufferSize has been set to %d\n", actualUdpRecvBufferSizeSize);
        }
    }
    if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
        fprintf(stderr, "[Error] can not set udpSendBufferSize to %d\n", udpBufferSize);
    } else {
        int actualUdpSendBufferSizeSize;
        socklen_t actualUdpSendBufferSizeSizeLen = sizeof(actualUdpSendBufferSizeSize);
        if (getsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&actualUdpSendBufferSizeSize, &actualUdpSendBufferSizeSizeLen) == 0) {
            printf("[INFO] udpSendBufferSize has been set to %d\n", actualUdpSendBufferSizeSize);
        }
    }
    return true;
}

bool tapLanCloseUdpIPv6Socket() {
    close(udp_fd);
    return true;
}

ssize_t tapLanSendToUdpIPv6Socket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen) {
    return sendto(udp_fd, buf, bufLen, 0, dstAddr, addrLen);
}

ssize_t tapLanRecvFromUdpIPv6Socket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen) {
    return recvfrom(udp_fd, buf, bufLen, 0, srcAddr, addrLen);
}

#endif
