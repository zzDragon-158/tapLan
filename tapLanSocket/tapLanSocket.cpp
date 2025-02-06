#include "tapLanSocket.hpp"

#ifdef _WIN32
static int udp_fd;
WSAPOLLFD udp_pfd;

bool tapLanOpenUdpIPv6Socket(uint16_t sin6_port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return false;
    }
    udp_fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd == INVALID_SOCKET) {
        fprintf(stderr, "Can not create socket with WSAGetLastError %d\n", WSAGetLastError());
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
            fprintf(stderr, "Can not bind to %s:%u with WSAGetLastError %d\n", ipv6Addr, ntohs(sa.sin6_port), WSAGetLastError());
            closesocket(udp_fd);
            return false;
        }
        int udpBufferSize = 1024 * 1024;
        if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
            fprintf(stderr, "Can not set udpBufferSize to %d\n", udpBufferSize);
        }
    }
    /* windows bug: udp socket 10054 */ {
        BOOL bEnalbeConnRestError = FALSE;
        DWORD dwBytesReturned = 0;
        if (WSAIoctl(udp_fd, _WSAIOW(IOC_VENDOR, 12), &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), nullptr, 0, &dwBytesReturned, nullptr, nullptr)) {
            fprintf(stderr, "WSAIoctl fail\n");
        }
    }
    /* print addr */ {
        sockaddr_in6 sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        int sockAddrLen = sizeof(sockAddr);
        if (getsockname(udp_fd, (sockaddr*)(&sockAddr), &sockAddrLen)) {
            fprintf(stderr, "Can not getsockname\n");
            return false;
        }
        char ipv6Addr[INET6_ADDRSTRLEN] = {'\0'};
        inet_ntop(AF_INET6, &sockAddr.sin6_addr, ipv6Addr, sizeof(ipv6Addr));
        printf("Create udp socket bind to %s:%u\n", ipv6Addr, ntohs(sockAddr.sin6_port));
    }
    udp_pfd.fd = udp_fd;
    udp_pfd.events = POLLIN;
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

ssize_t tapLanRecvFromUdpIPv6Socket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen, int timeout) {
    int result = WSAPoll(&udp_pfd, 1, timeout);
    if (result == 1) {
        if (udp_pfd.revents & POLLIN) {
            return recvfrom(udp_fd, (char*)buf, bufLen, 0, srcAddr, addrLen);
        } else {
            return -1;
        }
    } else if (result == 0) {
        return -100;
    }
    return -1;
}

#else
static int udp_fd;
static struct pollfd udp_pfd;

bool tapLanOpenUdpIPv6Socket(uint16_t sin6_port) {
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        fprintf(stderr, "Error creating UDP socket\n");
        return false;
    }
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_any;
    sa.sin6_port = htons(sin6_port);
    if (bind(udp_fd, (sockaddr*)(&sa), sizeof(sa)) == -1) {
        fprintf(stderr, "Error binding UDP socket\n");
        close(udp_fd);
        return false;
    }
    /* print addr */ {
        sockaddr_in6 sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        socklen_t sockAddrLen = sizeof(sockAddr);
        if (getsockname(udp_fd, (sockaddr*)(&sockAddr), &sockAddrLen)) {
            fprintf(stderr, "Can not getsockname\n");
            return false;
        }
        char ipv6Addr[INET6_ADDRSTRLEN] = {'\0'};
        inet_ntop(AF_INET6, &sockAddr.sin6_addr, ipv6Addr, sizeof(ipv6Addr));
        printf("Create udp socket bind to %s:%u\n", ipv6Addr, ntohs(sockAddr.sin6_port));
    }
    udp_pfd.fd = udp_fd;
    udp_pfd.events = POLLIN;
    return true;
}

bool tapLanCloseUdpIPv6Socket() {
    close(udp_fd);
    return true;
}

ssize_t tapLanSendToUdpIPv6Socket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen) {
    return sendto(udp_fd, buf, bufLen, 0, dstAddr, addrLen);
}

ssize_t tapLanRecvFromUdpIPv6Socket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen, int timeout) {
    int result = poll(&udp_pfd, 1, timeout);
    if (result == 1) {
        if (udp_pfd.revents & POLLIN) {
            return recvfrom(udp_fd, buf, bufLen, 0, srcAddr, addrLen);
        } else {
            return -1;
        }
    } else if (result == 0) {
        return -100;
    }
    return -1;
}

#endif
