#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>

int my_encrypt(const uint8_t *plaintext, int pt_len, 
            const uint8_t *key, uint8_t *encryptedtext);

int my_decrypt(const uint8_t *encryptedtext, int ct_len,
            const uint8_t *key, uint8_t *plaintext);

int load_key(const char *path, uint8_t *key);

#endif