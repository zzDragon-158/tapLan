#pragma once
#include    <cstdint>
#include    <iostream>
#ifdef _WIN32
#include    <WS2tcpip.h>
#define     TapLanSocketLogError(fmt, ...)          fprintf(stderr, "[TapLanSocket] [ERROR] " fmt " WSAGetLastError %d\n", ##__VA_ARGS__, WSAGetLastError())
#define     TapLanPoll                              WSAPoll
#else
#include    <poll.h>                                // for poll
#include    <unistd.h>                              // for close
#include    <cstring>                               // for memset
#include    <cerrno>                                // for errno
#include    <sys/socket.h>                          // for socket
#include    <arpa/inet.h>                           // for in6addr_any
#define     TapLanSocketLogError(fmt, ...)          fprintf(stderr, "[TapLanSocket] [ERROR] " fmt "\n", ##__VA_ARGS__)
#define     TapLanPoll                              poll
#endif
#define     TapLanSocketLogInfo(fmt, ...)           fprintf(stdout, "[TapLanSocket] [INFO] " fmt "\n", ##__VA_ARGS__)

#ifdef _WIN32
typedef SOCKET TapLanSocket;
#else
typedef int TapLanSocket;
#endif
typedef pollfd TapLanPollFD;
extern TapLanSocket tcp_fd;
extern TapLanSocket udp_fd;
extern uint64_t udpSendErrCnt;
extern uint64_t udpRecvErrCnt;

bool tapLanOpenUdpSocket(uint16_t port);
bool tapLanCloseUdpSocket();
ssize_t tapLanSendToUdpSocket(const void* buf, size_t bufLen, const sockaddr* dstAddr, socklen_t addrLen);
ssize_t tapLanRecvFromUdpSocket(void* buf, size_t bufLen, sockaddr* srcAddr, socklen_t* addrLen);
bool tapLanOpenTcpSocket(uint16_t port);
bool tapLanCloseTcpSocket(TapLanSocket fd = tcp_fd);
bool tapLanListen(int backlog);
TapLanSocket tapLanAccept(sockaddr* addr, socklen_t *addrlen);
bool tapLanConnect(const sockaddr* addr, socklen_t addrlen);
ssize_t tapLanSendToTcpSocket(const void* buf, size_t bufLen, TapLanSocket dest = tcp_fd);
ssize_t tapLanRecvFromTcpSocket(void* buf, size_t bufLen, TapLanSocket src = tcp_fd);
