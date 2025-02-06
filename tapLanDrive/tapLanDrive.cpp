#include "tapLanDrive.hpp"

#ifdef _WIN32
struct WinAdapterInfo {
    HANDLE handle;
    char adapterId[BUFFER_SIZE];
    unsigned long int adapterIdLen;
    unsigned char adapterName[BUFFER_SIZE];
    unsigned long int adapterNameLen;
    ULONG mediaStatus;
    unsigned long int mediaStatusLen;
    OVERLAPPED overlapRead, overlapWrite;

    WinAdapterInfo(): handle(nullptr), adapterIdLen(BUFFER_SIZE), adapterNameLen(BUFFER_SIZE), mediaStatus(TRUE), mediaStatusLen(sizeof(mediaStatusLen)) {
        memset(adapterId, 0, sizeof(adapterId));
        memset(adapterName, 0, sizeof(adapterName));
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
    if (int rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &openkey0)) {
        fprintf(stderr, "Unable to read registry: [rc = %d]\n", rc);
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

ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen) {
    ResetEvent(tapLanTapDevice.overlapWrite.hEvent);
    DWORD writeBytes;
    if (WriteFile(tapLanTapDevice.handle, buf, bufLen, &writeBytes, &tapLanTapDevice.overlapWrite)) {
        return writeBytes;
    }
    DWORD lastError = GetLastError();
    switch (lastError) {
        case ERROR_IO_PENDING:
            WaitForSingleObject(tapLanTapDevice.overlapWrite.hEvent, INFINITE);
            GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapWrite, &writeBytes, FALSE);
            return writeBytes;
        default:
            fprintf(stderr, "GetLastError %u\n", lastError);
            return -100;
    }
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen, int timeout) {
    ResetEvent(tapLanTapDevice.overlapRead.hEvent);
    DWORD readBytes;
    if (ReadFile(tapLanTapDevice.handle, buf, bufLen, &readBytes, &tapLanTapDevice.overlapRead)) {
        return readBytes;
    }
    DWORD lastError = GetLastError();
    switch (lastError) {
        case ERROR_IO_PENDING:
            WaitForSingleObject(tapLanTapDevice.overlapRead.hEvent, INFINITE);
            GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapRead, &readBytes, FALSE);
            return readBytes;
        default:
            fprintf(stderr, "GetLastError %u\n", lastError);
            return -100;
    }
}

#else
static int tap_fd;
static struct pollfd tap_pfd;

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
    printf("TAP device %s opened successfully\n", ifr.ifr_name);
    tap_pfd.fd = tap_fd;
    tap_pfd.events = POLLIN;
    return true;
}

bool tapLanCloseTapDevice() {
    close(tap_fd);
    return true;
}

ssize_t tapLanWriteToTapDevice(const void* buf, size_t bufLen) {
    return write(tap_fd, buf, bufLen);
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen, int timeout) {
    int result = poll(&tap_pfd, 1, timeout);
    if (result == 1) {
        if (tap_pfd.revents & POLLIN) {
            return read(tap_fd, buf, bufLen);
        } else {
            return -1;
        }
    } else if (result == 0) {
        return -100;
    }
    return -1;
}

#endif
