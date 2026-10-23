/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_H

#include <openssl/aes.h>
#include "prov/ciphercommon.h"
#include "crypto/aes_platform.h"

typedef struct prov_aes_ctx_st {
    PROV_CIPHER_CTX base; /* Must be first */
    union {
        OSSL_UNION_ALIGN;
        AES_KEY ks;
    } ks;

    /* Platform specific data */
    union {
        int dummy;
#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__)
        struct {
            union {
                OSSL_UNION_ALIGN;
                /*-
                 * KM-AES parameter block - begin
                 * (see z/Architecture Principles of Operation >= SA22-7832-06)
                 */
                struct {
                    unsigned char k[32];
                } km;
                /* KM-AES parameter block - end */
                /*-
                 * KMO-AES/KMF-AES parameter block - begin
                 * (see z/Architecture Principles of Operation >= SA22-7832-08)
                 */
                struct {
                    unsigned char cv[16];
                    unsigned char k[32];
                } kmo_kmf;
                /* KMO-AES/KMF-AES parameter block - end */
            } param;
            unsigned int fc;
        } s390x;
#endif /* defined(OPENSSL_CPUID_OBJ) && defined(__s390__) */
    } plat;

} PROV_AES_CTX;

/* Note that XTS, CCM and GCM modes are handled with separate abstractions
 * so they are not listed here */
enum aes_modes {
    AES_MODE_ECB = 1,
    AES_MODE_CBC,
    AES_MODE_CFB128,
    AES_MODE_CFB8,
    AES_MODE_CFB1,
    AES_MODE_OFB128,
    AES_MODE_CTR,
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ecb(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cbc(size_t keybits);
#define ossl_prov_cipher_hw_aes_cfb ossl_prov_cipher_hw_aes_cfb128
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb128(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb8(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb1(size_t keybits);
#define ossl_prov_cipher_hw_aes_ofb ossl_prov_cipher_hw_aes_ofb128
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ofb128(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ctr(size_t keybits);

int ossl_cipher_set_aes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_key, aes_block128_f fn_block,
    ecb128_f fn_ecb, cbc128_f fn_cbc, ctr128_f fn_ctr);

int ossl_cipher_hw_aes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen);

void ossl_cipher_aes_copyctx(PROV_CIPHER_CTX *dst, const PROV_CIPHER_CTX *src);

#if defined(AESNI_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aesni(enum aes_modes mode);
#elif defined(ARMv8_HWAES_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_arm(enum aes_modes mode);
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
const PROV_CIPHER_HW *ossl_prov_cipher_hw_rv32i(enum aes_modes mode);
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
const PROV_CIPHER_HW *ossl_prov_cipher_hw_rv64i(enum aes_modes mode);
#elif defined(S390X_aes_128_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_s390x(enum aes_modes mode,
    size_t keybits);
#elif defined(SPARC_AES_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_t4(enum aes_modes mode);
#endif

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_H) */
