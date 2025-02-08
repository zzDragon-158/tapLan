#include "tapLanSocket.hpp"

const int udpBufferSize = 1024 * 1024 * 8;

#ifdef _WIN32
static SOCKET udp_fd;

bool tapLanOpenUdpSocket(uint16_t port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed, WSAGetLastError %d\n", WSAGetLastError());
        return false;
    }
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == INVALID_SOCKET) {
        fprintf(stderr, "[ERROR] can not create udp socket, WSAGetLastError %d\n", WSAGetLastError());
        return false;
    }
    /* listen to ipv4 and ipv6 */ {
        int off = 0;
        if (setsockopt(udp_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off))) {
            fprintf(stderr, "[ERROR] setsockopt(IPV6_V6ONLY) failed, WSAGetLastError %d\n", WSAGetLastError());
            return false;
        }
    }
    /* bind udp socket */ {
        sockaddr_in6 sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = in6addr_any;
        sa.sin6_port = htons(port);
        if (bind(udp_fd, (sockaddr*)(&sa), sizeof(sockaddr_in6))) {
            fprintf(stderr, "[ERROR] can not bind to [::]:%u, WSAGetLastError %d\n", port, WSAGetLastError());
            return false;
        }
        /* set udp buffer size */ {
            if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
                fprintf(stderr, "[ERROR] can not setsockopt(SO_RCVBUF) to %d, WSAGetLastError %d\n", udpBufferSize, WSAGetLastError());
            }
            if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
                fprintf(stderr, "[ERROR] can not setsockopt(SO_SNDBUF) to %d, WSAGetLastError %d\n", udpBufferSize, WSAGetLastError());
            }
        }
    }
    /* windows bug: udp socket 10054 */ {
        BOOL bEnalbeConnRestError = FALSE;
        DWORD dwBytesReturned = 0;
        if (WSAIoctl(udp_fd, _WSAIOW(IOC_VENDOR, 12), &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), nullptr, 0, &dwBytesReturned, nullptr, nullptr)) {
            fprintf(stderr, "[ERROR] WSAIoctl failed, WSAGetLastError %d\n", WSAGetLastError());
            return false;
        }
    }
    return true;
}

bool tapLanCloseUdpSocket() {
    closesocket(udp_fd);
    WSACleanup();
    return true;
}

ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen) {
    return sendto(udp_fd, (const char*)buf, bufLen, 0, dstAddr, addrLen);
}

ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen) {
    return recvfrom(udp_fd, (char*)buf, bufLen, 0, srcAddr, addrLen);
}

#else
static int udp_fd;

bool tapLanOpenUdpSocket(uint16_t port) {
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        fprintf(stderr, "[ERROR] can not create udp socket\n");
        return false;
    }
    /* listen to ipv4 and ipv6 */ {
        int off = 0;
        if (setsockopt(udp_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off))) {
            fprintf(stderr, "[ERROR] setsockopt(IPV6_V6ONLY) failed\n");
            return false;
        }
    }
    /* bind udp socket */ {
        sockaddr_in6 sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = in6addr_any;
        sa.sin6_port = htons(port);
        if (bind(udp_fd, (sockaddr*)(&sa), sizeof(sockaddr_in6))) {
            fprintf(stderr, "[ERROR] can not bind to [::]:%u\n", port);
            return false;
        }
        /* set udp buffer size */ {
            if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
                fprintf(stderr, "[ERROR] can not setsockopt(SO_RCVBUF) to %d\n", udpBufferSize);
            }
            if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
                fprintf(stderr, "[ERROR] can not setsockopt(SO_SNDBUF) to %d\n", udpBufferSize);
            }
        }
    }
    return true;
}

bool tapLanCloseUdpSocket() {
    close(udp_fd);
    return true;
}

ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen) {
    return sendto(udp_fd, buf, bufLen, 0, dstAddr, addrLen);
}

ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen) {
    return recvfrom(udp_fd, buf, bufLen, 0, srcAddr, addrLen);
}

#endif
