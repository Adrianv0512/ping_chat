#include "crypto.h"
#include <openssl/evp.h>
#include <stdio.h>

int load_key(const char *path, uint8_t *key) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return -1;
    }
    if (fread(key, 1, 32, f) != 32) {
        fprintf(stderr, "Key must be 32 bytes");
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int my_encrypt(const uint8_t *plaintext, int pt_len,
            const uint8_t *key, uint8_t *encryptedtext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
    
    int out_len = 0;
    int total = 0;
    EVP_EncryptUpdate(ctx, encryptedtext, &out_len, plaintext, pt_len);
    total += out_len;
    EVP_EncryptFinal_ex(ctx, encryptedtext + total, &out_len);
    total += out_len;
    EVP_CIPHER_CTX_free(ctx);
    return total;

}

int my_decrypt(const uint8_t *encryptedtext, int ct_len,
            const uint8_t *key, uint8_t *plaintext) {
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);
    int out_len = 0;
    int total = 0;
    EVP_DecryptUpdate(ctx, plaintext, &out_len, encryptedtext, ct_len);
    total += out_len;

    EVP_DecryptFinal_ex(ctx, plaintext + total, &out_len);
    total += out_len;

    EVP_CIPHER_CTX_free(ctx);
    return total;
}