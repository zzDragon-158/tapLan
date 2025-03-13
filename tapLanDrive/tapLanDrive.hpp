#pragma once
#include    <iostream>
#include    <cstdint>
#ifdef _WIN32
#include    <sstream>
#include    <windows.h>
#define     ADAPTER_KEY                             "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define     NETWORK_CONNECTIONS_KEY                 "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"
#define     TAP_INSTALL                             ".\\tapinstall.exe"
#define     USERMODEDEVICEDIR                       "\\\\.\\Global\\"
#define     TAPSUFFIX                               ".tap"
#define     BUFFER_SIZE                             1024
#define     TAP_CONTROL_CODE(request,method)        CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)
#define     TAP_IOCTL_GET_MAC                       TAP_CONTROL_CODE (1, METHOD_BUFFERED)
#define     TAP_IOCTL_SET_MEDIA_STATUS              TAP_CONTROL_CODE (6, METHOD_BUFFERED)
#define     TapLanDriveLogError(fmt, ...)           fprintf(stderr, "[TapLanDrive] [ERROR] " fmt " GetLastError %d\n", ##__VA_ARGS__, GetLastError())
#else
#include    <unistd.h>                              // for close
#include    <fcntl.h>                               // for open, O_RDWR
#include    <cstring>                               // for memset, strncpy
#include    <sys/ioctl.h>                           // for ioctl, TUNSETIFF
#include    <net/if.h>                              // for struct ifreq, IFNAMSIZ
#include    <linux/if_tun.h>                        // for IFF_TAP, IFF_NO_PI;
#include    <poll.h>                                // for poll, pollfd
#define     TapLanDriveLogError(fmt, ...)           fprintf(stderr, "[TapLanDrive] [ERROR] " fmt "\n", ##__VA_ARGS__)
#endif
#define     TapLanDriveLogInfo(fmt, ...)            fprintf(stdout, "[TapLanDrive] [INFO] " fmt "\n", ##__VA_ARGS__)
#define     TAP_NAME                                "tapLan"
#define     ETHERNET_HEADER_LEN                     14
#define     ETHERTYPE_ARP                           0x0806

struct ether_header {
    uint8_t ether_dhost[6];
    uint8_t ether_shost[6];
    uint16_t ether_type;
};

extern uint64_t tapWriteErrorCnt;
extern uint64_t tapReadErrorCnt;
extern uint64_t dwc;
extern uint64_t drc;

bool tapLanOpenTapDevice();
bool tapLanCloseTapDevice();
bool tapLanGetMACAddress(uint8_t* buf, size_t bufLen);
ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen);
ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen, int timeout = -1);
