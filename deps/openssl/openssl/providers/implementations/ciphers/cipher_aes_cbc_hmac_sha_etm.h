/*
 * Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/proverr.h>
#include "prov/ciphercommon.h"
#include "crypto/aes_platform.h"

int ossl_cipher_capable_aes_cbc_hmac_sha1_etm(void);
int ossl_cipher_capable_aes_cbc_hmac_sha256_etm(void);
int ossl_cipher_capable_aes_cbc_hmac_sha512_etm(void);

typedef struct prov_cipher_hw_aes_hmac_sha_ctx_etm_st {
    PROV_CIPHER_HW base; /* must be first */
    void (*init_mac_key)(void *ctx, const unsigned char *inkey, size_t inlen);
} PROV_CIPHER_HW_AES_HMAC_SHA_ETM;

const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *ossl_prov_cipher_hw_aes_cbc_hmac_sha1_etm(void);
const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *ossl_prov_cipher_hw_aes_cbc_hmac_sha256_etm(void);
const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *ossl_prov_cipher_hw_aes_cbc_hmac_sha512_etm(void);

#ifdef AES_CBC_HMAC_SHA_ETM_CAPABLE
# include <openssl/aes.h>
# include <openssl/sha.h>

# define AES_CBC_MAX_HMAC_SIZE 64

typedef struct prov_aes_hmac_sha_etm_ctx_st {
    PROV_CIPHER_CTX base;
    AES_KEY ks;
    const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *hw;
    unsigned char tag[AES_CBC_MAX_HMAC_SIZE];
    unsigned char exp_tag[AES_CBC_MAX_HMAC_SIZE];
    size_t taglen;
} PROV_AES_HMAC_SHA_ETM_CTX;

typedef struct prov_aes_hmac_sha1_etm_ctx_st {
    PROV_AES_HMAC_SHA_ETM_CTX base_ctx;
    SHA_CTX head, tail;
} PROV_AES_HMAC_SHA1_ETM_CTX;

typedef struct prov_aes_hmac_sha256_etm_ctx_st {
    PROV_AES_HMAC_SHA_ETM_CTX base_ctx;
    SHA256_CTX head, tail;
} PROV_AES_HMAC_SHA256_ETM_CTX;

typedef struct prov_aes_hmac_sha512_etm_ctx_st {
    PROV_AES_HMAC_SHA_ETM_CTX base_ctx;
    SHA512_CTX head, tail, md;
} PROV_AES_HMAC_SHA512_ETM_CTX;

typedef struct {
    struct {
        uint8_t *key;
        uint8_t key_rounds;
        uint8_t *iv;
    } cipher;
    struct {
        struct {
            uint8_t *i_key_pad;
            uint8_t *o_key_pad;
        } hmac;
    } digest;
} CIPH_DIGEST;

#endif /* AES_CBC_HMAC_SHA_ETM_CAPABLE */
