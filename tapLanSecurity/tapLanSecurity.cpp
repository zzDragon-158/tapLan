#include "tapLanSecurity.hpp"

bool tapLanEncryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key) {
    int out_len, final_len;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.aesKey, key.aesKey) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (EVP_EncryptUpdate(ctx, data, &out_len, data, dataLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (EVP_EncryptFinal_ex(ctx, data + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(ctx);
    dataLen = out_len + final_len;
    return true;
}

bool tapLanDecryptDataWithAes(uint8_t* data, size_t& dataLen, const TapLanKey& key) {
    int out_len, final_len;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.aesKey, key.aesKey) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (EVP_DecryptUpdate(ctx, data, &out_len, data, dataLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (EVP_DecryptFinal_ex(ctx, data + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(ctx);
    dataLen = out_len + final_len;
    return true;
}
