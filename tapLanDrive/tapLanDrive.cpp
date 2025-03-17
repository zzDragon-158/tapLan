#include "tapLanDrive.hpp"

#define likely(x) __builtin_expect(!!(x), 1) 
#define unlikely(x) __builtin_expect(!!(x), 0)

uint64_t tapWriteErrorCnt = 0;
uint64_t tapReadErrorCnt = 0;

#ifdef _WIN32
struct WinAdapterInfo {
    HANDLE handle;
    CHAR adapterId[BUFFER_SIZE];
    DWORD adapterIdLen;
    CHAR adapterName[BUFFER_SIZE];
    DWORD adapterNameLen;
    CHAR adapterMAC[6];
    DWORD adapterMACLen;
    DWORD mediaStatus;
    DWORD mediaStatusLen;
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

static WinAdapterInfo tapLanTapDevice;

static bool tapLanFindTapDevice() {
    bool ret = false;
    HKEY openKey0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &openKey0)) {
        TapLanDriveLogError("Openning %s failed.", NETWORK_CONNECTIONS_KEY);
        return ret;
    }
    for (int i = 0; ; ++i) {
        WinAdapterInfo adapter;
        if (RegEnumKeyExA(openKey0, i, adapter.adapterId, &adapter.adapterIdLen, nullptr, nullptr, nullptr, nullptr))
            break;
        std::ostringstream regpath;
        regpath << NETWORK_CONNECTIONS_KEY << "\\" << adapter.adapterId << "\\Connection";
        HKEY openKey1;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regpath.str().c_str(), 0, KEY_READ, &openKey1))
            continue;
        int err = RegQueryValueExA(openKey1, "Name", nullptr, nullptr, (LPBYTE)adapter.adapterName, &adapter.adapterNameLen);
        if (err) {
            RegCloseKey(openKey1);
            continue;
        }
        RegCloseKey(openKey1);
        if (0 != strcmp(TAP_NAME, (const char*)adapter.adapterName))
            continue;
        memcpy(&tapLanTapDevice, &adapter, sizeof(WinAdapterInfo));
        ret = true;
        break;
    }
    RegCloseKey(openKey0);
    return ret;
}

static bool tapLanCreateTapDevice() {
    bool ret = false;
    DWORD64 startTimestamp; {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        memcpy(&startTimestamp, &ft, 8);
    }
    DWORD attributes = GetFileAttributesA(TAP_INSTALL);
    if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        if (system(TAP_INSTALL " install OemVista.inf TAP0901")) {
            TapLanDriveLogError("Creating tap device failed.");
            return ret;
        }
    } else {
        TapLanDriveLogError("%s does not exist. Please place this program in the current directory.", TAP_INSTALL);
        return ret;
    }
    HKEY openKey0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &openKey0)) {
        TapLanDriveLogError("Openning %s failed.", ADAPTER_KEY);
        return false;
    }
    for (int i = 0; ; ++i) {
        WinAdapterInfo adapter;
        CHAR driveId[BUFFER_SIZE];
        DWORD driveIdLen = BUFFER_SIZE;
        if (RegEnumKeyExA(openKey0, i, driveId, &driveIdLen, nullptr, nullptr, nullptr, nullptr))
            break;
        std::ostringstream regpath;
        regpath << ADAPTER_KEY << "\\" << driveId;
        HKEY openKey1;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regpath.str().c_str(), 0, KEY_READ, &openKey1))
            continue;
        DWORD64 installTimestamp;
        DWORD installTimestampLen = sizeof(installTimestamp);
        LONG err;
        err = RegQueryValueExA(openKey1, "NetworkInterfaceInstallTimestamp", nullptr, nullptr, (UCHAR*)&installTimestamp, &installTimestampLen);
        if (err || installTimestamp < startTimestamp) {
            RegCloseKey(openKey1);
            continue;
        }
        CHAR providerName[BUFFER_SIZE];
        DWORD providerNameLen = BUFFER_SIZE;
        err = RegQueryValueExA(openKey1, "ProviderName", nullptr, nullptr, (UCHAR*)&providerName, &providerNameLen);
        if (err || strcmp("TAP-Windows Provider V9", (const char*)providerName)) {
            RegCloseKey(openKey1);
            continue;
        }
        err = RegQueryValueExA(openKey1, "NetCfgInstanceId", nullptr, nullptr, (UCHAR*)adapter.adapterId, &adapter.adapterIdLen);
        if (err) {
            RegCloseKey(openKey1);
            continue;
        }
        RegCloseKey(openKey1);
        regpath.str("");
        regpath << NETWORK_CONNECTIONS_KEY << "\\" << adapter.adapterId << "\\Connection";
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regpath.str().c_str(), 0, KEY_READ, &openKey1)) {
            TapLanDriveLogError("Openning %s failed.", regpath.str().c_str());
            break;
        }
        err = RegQueryValueExA(openKey1, "Name", nullptr, nullptr, (LPBYTE)adapter.adapterName, &adapter.adapterNameLen);
        if (err) {
            RegCloseKey(openKey1);
            TapLanDriveLogError("Getting tap device name failed.");
            break;
        }
        RegCloseKey(openKey1);
        std::ostringstream cmd;
        cmd << "netsh interface set interface name=\"" << adapter.adapterName << "\" newname=\"" << TAP_NAME << "\"";
        if (system(cmd.str().c_str())) {
            TapLanDriveLogError("Rename tap device failed.");
            break;
        }
        strcpy(adapter.adapterName, TAP_NAME);
        adapter.adapterNameLen = strlen(TAP_NAME) + 1;
        memcpy(&tapLanTapDevice, &adapter, sizeof(WinAdapterInfo));
        ret = true;
        break;
    }
    RegCloseKey(openKey0);
    return ret;
}

bool tapLanOpenTapDevice() {
    if (!tapLanFindTapDevice() && !tapLanCreateTapDevice())
        return false;
    std::ostringstream tapName;
    tapName << USERMODEDEVICEDIR << tapLanTapDevice.adapterId << TAPSUFFIX;
    tapLanTapDevice.handle = CreateFileA(tapName.str().c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
    if (tapLanTapDevice.handle != INVALID_HANDLE_VALUE) {
        if (!DeviceIoControl(tapLanTapDevice.handle, TAP_IOCTL_SET_MEDIA_STATUS,
            &tapLanTapDevice.mediaStatus, tapLanTapDevice.mediaStatusLen,
            &tapLanTapDevice.mediaStatus, tapLanTapDevice.mediaStatusLen, &tapLanTapDevice.mediaStatusLen, nullptr)) {
            TapLanDriveLogError("DeviceIoControl(TAP_IOCTL_SET_MEDIA_STATUS) failed.");
            return false;
        }
        if (!DeviceIoControl(tapLanTapDevice.handle, TAP_IOCTL_GET_MAC,
            tapLanTapDevice.adapterMAC, tapLanTapDevice.adapterMACLen,
            tapLanTapDevice.adapterMAC, tapLanTapDevice.adapterMACLen, &tapLanTapDevice.adapterMACLen, nullptr)) {
            TapLanDriveLogError("DeviceIoControl(TAP_IOCTL_GET_MAC) failed.");
            return false;
        }
    }
    return true;
}

bool tapLanCloseTapDevice() {
    CloseHandle(tapLanTapDevice.handle);
    // if (system(TAP_INSTALL " remove TAP0901"))
    //     TapLanDriveLogError("Removing tap device failed.");
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
    if (unlikely(WriteFile(tapLanTapDevice.handle, buf, bufLen, &writeBytes, &tapLanTapDevice.overlapWrite))) {
        ResetEvent(tapLanTapDevice.overlapWrite.hEvent);
        return writeBytes;
    } else {
        DWORD lastError = GetLastError();
        if (likely(lastError == ERROR_IO_PENDING)) {
            GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapWrite, &writeBytes, TRUE);
            ResetEvent(tapLanTapDevice.overlapWrite.hEvent);
            if (unlikely(writeBytes < bufLen)) {
                TapLanDriveLogError("writeBytes[%ld] is less than expected[%lu].", writeBytes, bufLen);
                ++tapWriteErrorCnt;
            }
            return writeBytes;
        } else {
            TapLanDriveLogError("Writting to tap device failed.");
            ++tapWriteErrorCnt;
            return -1;
        }
    }
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen, int timeout) {
    static DWORD readBytes;
    static uint8_t readStatus = 0;
    if (unlikely(readStatus == 0 && ReadFile(tapLanTapDevice.handle, buf, bufLen, &readBytes, &tapLanTapDevice.overlapRead))) {
        ResetEvent(tapLanTapDevice.overlapRead.hEvent);
        return readBytes;
    }
    if (readStatus == 0) {
        readStatus = 1;
        DWORD lastError = GetLastError();
        if (unlikely(lastError != ERROR_IO_PENDING)) {
            readStatus = 0;
            TapLanDriveLogError("Reading from tap device failed.");
            ++tapReadErrorCnt;
            return -1;
        }
    }
    if (WAIT_OBJECT_0 == WaitForSingleObject(tapLanTapDevice.overlapRead.hEvent, timeout)) {
        readStatus = 0;
        GetOverlappedResult(tapLanTapDevice.handle, &tapLanTapDevice.overlapRead, &readBytes, FALSE);
        ResetEvent(tapLanTapDevice.overlapRead.hEvent);
        return readBytes;
    }
    return 0;
}

#else
static int tap_fd;
uint8_t macAddress[6];

bool tapLanOpenTapDevice() {
    if (system("ip link set dev tapLan up")) {
        if (system("ip tuntap add dev tapLan mode tap"))
            return false;
        if (system("ip link set dev tapLan up"))
            return false;
    }
    tap_fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (tap_fd == -1) {
        TapLanDriveLogError("Can not open [/dev/net/tun].");
        return false;
    }
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, TAP_NAME, IFNAMSIZ);
    if (ioctl(tap_fd, TUNSETIFF, (void*)&ifr) == -1) {
        TapLanDriveLogError("ioctl(TUNSETIFF) failed.");
        return false;
    }
    if (ioctl(tap_fd, SIOCGIFHWADDR, &ifr) == -1) {
        TapLanDriveLogError("ioctl(SIOCGIFHWADDR) failed.");
        return false;
    }
    memcpy(macAddress, ifr.ifr_hwaddr.sa_data, 6);
    return true;
}

bool tapLanCloseTapDevice() {
    close(tap_fd);
    system("ip link del dev tapLan");
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
    ssize_t writeBytes = write(tap_fd, buf, bufLen);
    if (unlikely(writeBytes < bufLen)) {
        TapLanDriveLogError("writeBytes[%ld] is less than expected[%lu].", writeBytes, bufLen);
        ++tapWriteErrorCnt;
    }
    return writeBytes;
}

ssize_t tapLanReadFromTapDevice(void* buf, size_t bufLen, int timeout) {
    pollfd pfd = {tap_fd, POLLIN, 0};
    if (poll(&pfd, 1, timeout) <= 0) {
        return 0;
    }
    ssize_t readBytes = read(tap_fd, buf, bufLen);
    if (unlikely(readBytes == -1)) {
        TapLanDriveLogError("Reading from tap device failed.");
        ++tapReadErrorCnt;
    }
    return readBytes;
}

#endif
