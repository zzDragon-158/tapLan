#pragma once
#include <cstdint>

#ifdef _WIN32
#include <iostream>
#include <WS2tcpip.h>

#else
#include <poll.h>                   // for poll
#include <unistd.h>                 // for close
#include <cstring>                  // for memset
#include <iostream>
#include <sys/socket.h>             // for socket
#include <arpa/inet.h>              // for struct in6addr_any

#endif

bool tapLanOpenUdpIPv6Socket(uint16_t sin6_port);
bool tapLanCloseUdpIPv6Socket();
ssize_t tapLanSendToUdpIPv6Socket(const void* buf, size_t bufLen, const struct sockaddr* dstAddr, socklen_t addrLen);
ssize_t tapLanRecvFromUdpIPv6Socket(void* buf, size_t bufLen, struct sockaddr* srcAddr, socklen_t* addrLen, int timeout);
