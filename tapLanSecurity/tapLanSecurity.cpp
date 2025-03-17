#include "tapLanSecurity.hpp"

uint64_t encryptDataErrCnt = 0;
uint64_t decryptDataErrCnt = 0;

bool tapLanEncryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key) {
    bool ret = false;
    int out_len, final_len;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        TapLanSecLogError("EVP_CIPHER_CTX_new() failed.");
        return false;
    }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.aesKey, key.aesKey) != 1) {
        TapLanSecLogError("EVP_EncryptInit_ex() failed.");
        goto EncryptDataExit;
    }
    if (EVP_EncryptUpdate(ctx, data, &out_len, data, dataLen) != 1) {
        TapLanSecLogError("EVP_EncryptUpdate() failed.");
        goto EncryptDataExit;
    }
    if (EVP_EncryptFinal_ex(ctx, data + out_len, &final_len) != 1) {
        TapLanSecLogError("EVP_EncryptFinal_ex() failed.");
        goto EncryptDataExit;
    }
    ret = true;
    dataLen = out_len + final_len;
EncryptDataExit:
    EVP_CIPHER_CTX_free(ctx);
    if (!ret) ++encryptDataErrCnt;
    return ret;
}

bool tapLanDecryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key) {
    bool ret = false;
    int out_len, final_len;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        TapLanSecLogError("EVP_CIPHER_CTX_new() failed.");
        return false;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.aesKey, key.aesKey) != 1) {
        TapLanSecLogError("EVP_DecryptInit_ex() failed.");
        goto DecryptDataExit;
    }
    if (EVP_DecryptUpdate(ctx, data, &out_len, data, dataLen) != 1) {
        TapLanSecLogError("EVP_DecryptUpdate() failed.");
        goto DecryptDataExit;
    }
    if (EVP_DecryptFinal_ex(ctx, data + out_len, &final_len) != 1) {
        TapLanSecLogError("EVP_DecryptFinal_ex() failed.");
        goto DecryptDataExit;
    }
    dataLen = out_len + final_len;
DecryptDataExit:
    EVP_CIPHER_CTX_free(ctx);
    if (!ret) ++decryptDataErrCnt;
    return true;
}
