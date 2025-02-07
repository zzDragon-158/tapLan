#pragma once
#include <iostream>
#include <cstdint>

#ifdef _WIN32
#include <sstream>
#include <windows.h>

#define     NETWORK_CONNECTIONS_KEY                 "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define     USERMODEDEVICEDIR                       "\\\\.\\Global\\"
#define     TAPSUFFIX                               ".tap"
#define     BUFFER_SIZE                             1024
#define     TAP_DEVICE_NAME                         "tapLan"
#define     TAP_CONTROL_CODE(request,method)        CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
#define     TAP_IOCTL_SET_MEDIA_STATUS              TAP_CONTROL_CODE (6, METHOD_BUFFERED)

#else
#include <unistd.h>                 // for close
#include <fcntl.h>                  // for open, O_RDWR
#include <poll.h>                   // for poll
#include <cstring>                  // for memset, strncpy
#include <sys/ioctl.h>              // for ioctl, TUNSETIFF
#include <net/if.h>                 // for struct ifreq, IFNAMSIZ
#include <linux/if_tun.h>           // for IFF_TAP, IFF_NO_PI;

#endif

#define ETHERNET_HEADER_LEN 14
#define ETHERTYPE_ARP 0x0806
struct ether_header {
    uint8_t ether_dhost[6];
    uint8_t ether_shost[6];
    uint16_t ether_type;
};

bool tapLanOpenTapDevice(const char* devName);
bool tapLanCloseTapDevice();
ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen);
ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen);
