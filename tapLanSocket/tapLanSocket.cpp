#include "tapLanSocket.hpp"

#define likely(x) __builtin_expect(!!(x), 1) 
#define unlikely(x) __builtin_expect(!!(x), 0)

uint64_t udpSendErrCnt = 0;
uint64_t udpRecvErrCnt = 0;
const int udpBufferSize = 1024 * 1024 * 8;
sockaddr_in6 gatewayAddr;

#ifdef _WIN32
static SOCKET udp_fd = -1;
#else
static int udp_fd = -1;
#endif

bool tapLanOpenUdpSocket(uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        TapLanSocketLogError("WSAStartup failed.");
        return false;
    }
#endif
    udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        TapLanSocketLogError("Can not create udp socket.");
        return false;
    }
    /* listen to ipv4 and ipv6 */ {
        int off = 0;
        if (setsockopt(udp_fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&off, sizeof(off))) {
            TapLanSocketLogError("setsockopt(IPV6_V6ONLY) failed.");
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
            TapLanSocketLogError("Can not bind to [::]:%u.", port);
            return false;
        }
    }
    /* set udp buffer size */ {
        if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
            TapLanSocketLogError("Can not setsockopt(SO_RCVBUF) to %d.", udpBufferSize);
        }
        if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*)&udpBufferSize, sizeof(udpBufferSize))) {
            TapLanSocketLogError("Can not setsockopt(SO_SNDBUF) to %d.", udpBufferSize);
        }
    }
#ifdef _WIN32
    /* windows bug: udp socket 10054 */ {
        BOOL bEnalbeConnRestError = FALSE;
        DWORD dwBytesReturned = 0;
        if (WSAIoctl(udp_fd, _WSAIOW(IOC_VENDOR, 12), &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), nullptr, 0, &dwBytesReturned, nullptr, nullptr)) {
            TapLanSocketLogError("WSAIoctl(_WSAIOW(IOC_VENDOR, 12)) failed.");
            return false;
        }
    }
#endif
    return true;
}

bool tapLanCloseUdpSocket() {
#ifdef _WIN32
    closesocket(udp_fd);
    WSACleanup();
#else
    close(udp_fd);
#endif
    return true;
}

ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const sockaddr* dstAddr, socklen_t addrLen) {
    ssize_t sendBytes = sendto(udp_fd, (const char*)buf, bufLen, 0, dstAddr, addrLen);
    if (unlikely(sendBytes < bufLen)) {
        TapLanSocketLogError("sendBytes[%lld] is less than expected[%lld].", sendBytes, bufLen);
        ++udpSendErrCnt;
    }
    return sendBytes;
}

ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, sockaddr* srcAddr, socklen_t* addrLen) {
    ssize_t recvBytes = recvfrom(udp_fd, (char*)buf, bufLen, 0, srcAddr, addrLen);
    if (unlikely(recvBytes == -1)) {
        TapLanSocketLogError("Receiving from UDP socket failed.");
        ++udpRecvErrCnt;
    }
    return recvBytes;
}
