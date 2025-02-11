#include "tapLanDrive.hpp"

#ifdef _WIN32
struct WinAdapterInfo {
    HANDLE handle;
    char adapterId[BUFFER_SIZE];
    unsigned long int adapterIdLen;
    unsigned char adapterName[BUFFER_SIZE];
    unsigned long int adapterNameLen;
    unsigned char adapterMAC[6];
    unsigned long int adapterMACLen;
    ULONG mediaStatus;
    unsigned long int mediaStatusLen;
    OVERLAPPED overlapRead, overlapWrite;

    WinAdapterInfo(): handle(nullptr), adapterIdLen(BUFFER_SIZE), adapterNameLen(BUFFER_SIZE), adapterMACLen(6),
        mediaStatus(TRUE), mediaStatusLen(sizeof(mediaStatusLen)) {
        memset(adapterId, 0, sizeof(adapterId));
        memset(adapterName, 0, sizeof(adapterName));
        memset(adapterMAC, 0, sizeof(adapterMAC));
        memset(&overlapRead, 0, sizeof(overlapRead));
        memset(&overlapWrite, 0, sizeof(overlapWrite));
        overlapRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        overlapWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
};

static struct WinAdapterInfo tapLanTapDevice;

bool tapLanOpenTapDevice(const char* devName) {
    bool ret = false;
    HKEY openkey0;
    // 获取网络适配器列表
    if (LONG rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &openkey0)) {
        fprintf(stderr, "[ERROR] unable to read registry, RegOpenKeyExA %d\n", rc);
        return false;
    }
    // 遍历注册表目录 NETWORK_CONNECTIONS_KEY 下的所有网络适配器
    for (int i = 0; ; ++i) {
        struct WinAdapterInfo adapter;
        // 获取 adapterId
        if (RegEnumKeyExA(openkey0, i, adapter.adapterId, &adapter.adapterIdLen, nullptr, nullptr, nullptr, nullptr)) break;
        // 获取 adapterName
        std::ostringstream regpath;
        regpath << NETWORK_CONNECTIONS_KEY << "\\" << adapter.adapterId << "\\Connection";
        HKEY openkey1;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regpath.str().c_str(), 0, KEY_READ, &openkey1)) continue;
        int err = RegQueryValueExA(openkey1, "Name", nullptr, nullptr, adapter.adapterName, &adapter.adapterNameLen);
        RegCloseKey(openkey1);
        if (err) continue;
        // 获取 handle
        std::ostringstream tapName;
        tapName << USERMODEDEVICEDIR << adapter.adapterId << TAPSUFFIX;
        adapter.handle = CreateFileA(tapName.str().c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
        if (adapter.handle != INVALID_HANDLE_VALUE) {
            if (0 == strcmp(TAP_DEVICE_NAME, (const char*)adapter.adapterName)) {
                memcpy(&tapLanTapDevice, &adapter, sizeof(struct WinAdapterInfo));
                DeviceIoControl(tapLanTapDevice.handle, TAP_IOCTL_SET_MEDIA_STATUS,
                    &tapLanTapDevice.mediaStatus, tapLanTapDevice.mediaStatusLen,
                    &tapLanTapDevice.mediaStatus, tapLanTapDevice.mediaStatusLen, &tapLanTapDevice.mediaStatusLen, nullptr);
                printf("[INFO] TAP device [%s] opened successfully\n", tapLanTapDevice.adapterName);
                DeviceIoControl(tapLanTapDevice.handle, TAP_IOCTL_GET_MAC,
                    tapLanTapDevice.adapterMAC, tapLanTapDevice.adapterMACLen,
                    tapLanTapDevice.adapterMAC, tapLanTapDevice.adapterMACLen, &tapLanTapDevice.adapterMACLen, nullptr);
                ret = true;
                break;
            } else {
                CloseHandle(adapter.handle);
            }
        }
    }
    RegCloseKey(openkey0);
    return ret;
}

bool tapLanCloseTapDevice() {
    CloseHandle(tapLanTapDevice.handle);
    return true;
}

bool tapLanGetMACAddress(uint8_t* buf, size_t bufLen) {
    if (bufLen < 6) {
        return false;
    }
    memcpy(buf, &tapLanTapDevice.adapterMAC, 6);
    return true;
}

ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen) {
    static DWORD writeBytes;
    if (WriteFile(tapLanTapDevice.handle, buf, bufLen, &writeBytes, &tapLanTapDevice.overlapWrite)) {
        ResetEvent(tapLanTapDevice.overlapWrite.hEvent);
        return writeBytes;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_IO_PENDING) {
            GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapWrite, &writeBytes, TRUE);
            ResetEvent(tapLanTapDevice.overlapWrite.hEvent);
            return writeBytes;
        } else {
            fprintf(stderr, "[ERROR] writting to tap device, GetLastError %u\n", lastError);
            return -1;
        }
    }
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen) {
    static DWORD readBytes;
    if (ReadFile(tapLanTapDevice.handle, buf, bufLen, &readBytes, &tapLanTapDevice.overlapRead)) {
        ResetEvent(tapLanTapDevice.overlapRead.hEvent);
        return readBytes;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_IO_PENDING) {
            GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapRead, &readBytes, TRUE);
            ResetEvent(tapLanTapDevice.overlapRead.hEvent);
            return readBytes;
        } else {
            fprintf(stderr, "[ERROR] reading from tap device, GetLastError %u\n", lastError);
            return -1;
        }
    }
}

#else
static int tap_fd;
uint8_t macAddress[6];

bool tapLanOpenTapDevice(const char* devName) {
    tap_fd = open("/dev/net/tun", O_RDWR);
    if (tap_fd == -1) {
        fprintf(stderr, "Error can not open /dev/net/tun\n");
        return false;
    }
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (devName) {
        strncpy(ifr.ifr_name, devName, IFNAMSIZ);
    }
    if (ioctl(tap_fd, TUNSETIFF, (void*)&ifr) == -1) {
        fprintf(stderr, "ioctl(TUNSETIFF)\n");
        close(tap_fd);
        return false;
    }
    printf("[INFO] TAP device [%s] opened successfully\n", ifr.ifr_name);
    if (ioctl(tap_fd, SIOCGIFHWADDR, &ifr) == -1) {
        fprintf(stderr, "ioctl(SIOCGIFHWADDR)\n");
        close(tap_fd);
        return false;
    }
    memcpy(macAddress, ifr.ifr_hwaddr.sa_data, 6);
    return true;
}

bool tapLanCloseTapDevice() {
    close(tap_fd);
    return true;
}

bool tapLanGetMACAddress(uint8_t* buf, size_t bufLen) {
    if (bufLen < 6) {
        return false;
    }
    memcpy(buf, macAddress, 6);
    return true;
}

ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen) {
    return write(tap_fd, buf, bufLen);
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen) {
    return read(tap_fd, buf, bufLen);
}

#endif
