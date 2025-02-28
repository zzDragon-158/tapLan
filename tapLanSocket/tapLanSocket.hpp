#pragma once
#include <cstdint>
#include <iostream>
#ifdef _WIN32
#include <WS2tcpip.h>
#define TapLanSocketLogError(fmt, ...)        fprintf(stderr, "[TapLanSocket] [ERROR] " fmt " WSAGetLastError %d\n", ##__VA_ARGS__, WSAGetLastError())
#else
#include <poll.h>                   // for poll
#include <unistd.h>                 // for close
#include <cstring>                  // for memset
#include <sys/socket.h>             // for socket
#include <arpa/inet.h>              // for in6addr_any
#define TapLanSocketLogError(fmt, ...)        fprintf(stderr, "[TapLanSocket] [ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
#define TapLanSocketLogInfo(fmt, ...)         fprintf(stdout, "[TapLanSocket] [INFO] " fmt "\n", ##__VA_ARGS__)

extern uint64_t udpSendErrCnt;
extern uint64_t udpRecvErrCnt;
extern sockaddr_in6 gatewayAddr;

bool tapLanOpenUdpSocket(uint16_t port);
bool tapLanCloseUdpSocket();
ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const sockaddr* dstAddr, socklen_t addrLen);
ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, sockaddr* srcAddr, socklen_t* addrLen);
