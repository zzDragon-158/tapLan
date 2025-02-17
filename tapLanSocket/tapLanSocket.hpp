#pragma once
#include <cstdint>
#ifdef _WIN32
#include <iostream>
#include <WS2tcpip.h>
#define TapLanSocketLogError(fmt, ...)        fprintf(stderr, "[TapLanSocket] [ERROR] " fmt " WSAGetLastError %d\n", ##__VA_ARGS__, WSAGetLastError())
#else
#include <poll.h>                   // for poll
#include <unistd.h>                 // for close
#include <cstring>                  // for memset
#include <iostream>
#include <sys/socket.h>             // for socket
#include <arpa/inet.h>              // for struct in6addr_any
#define TapLanSocketLogError(fmt, ...)        fprintf(stderr, "[TapLanSocket] [ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
#define TapLanSocketLogInfo(fmt, ...)         fprintf(stdout, "[TapLanSocket] [INFO] " fmt "\n", ##__VA_ARGS__)

extern sockaddr_in6 gatewayAddr;

bool tapLanOpenUdpSocket(uint16_t port);
bool tapLanCloseUdpSocket();
ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen);
ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen);
