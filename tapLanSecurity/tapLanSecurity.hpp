#pragma once
#include <cstdint>
#include <cstring>
#include <openssl/evp.h>

#define TapLanSecLogInfo(fmt, ...)         fprintf(stdout, "[TapLanSec] [INFO] " fmt "\n", ##__VA_ARGS__)
#define TapLanSecLogError(fmt, ...)        fprintf(stderr, "[TapLanSec] [ERROR] " fmt "\n", ##__VA_ARGS__)

struct TapLanKey {
    uint8_t aesKey[16];
    TapLanKey() {
        memset(aesKey, 0, 16);
    }

    TapLanKey(const char* s) {
        size_t sl = strlen(s);
        if (sl > 16)
            sl = 16;
        memcpy(aesKey, s, sl);
        for (int i = sl; i < 16; ++i) {
            aesKey[i] = 16 - sl;
        }
    }

    TapLanKey& operator=(const TapLanKey& other) {
        if (this != &other) {
            memcpy(aesKey, other.aesKey, 16);
        }
        return *this;
    }
};
extern int encryptDataErrCnt;
extern int decryptDataErrCnt;

bool tapLanEncryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key);
bool tapLanDecryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key);
