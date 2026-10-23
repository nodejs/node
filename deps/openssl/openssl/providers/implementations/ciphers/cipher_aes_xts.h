/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_XTS_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_XTS_H

#include <openssl/aes.h>
#include "prov/ciphercommon.h"
#include "crypto/aes_platform.h"

/*
 * Available in cipher_fips.c, and compiled with different values depending
 * on we're in the FIPS module or not.
 */
extern const int ossl_aes_xts_allow_insecure_decrypt;

PROV_CIPHER_FUNC(void, xts_stream,
    (const unsigned char *in, unsigned char *out, size_t len,
        const AES_KEY *key1, const AES_KEY *key2,
        const unsigned char iv[16]));

#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__)
typedef struct S390X_km_xts_params_st {
    unsigned char key[64];
    unsigned char tweak[16];
    unsigned char nap[16];
} S390X_KM_XTS_PARAMS;
#endif

typedef struct prov_aes_xts_ctx_st {
    PROV_CIPHER_CTX base; /* Must be first */
    union {
        OSSL_UNION_ALIGN;
        AES_KEY ks;
    } ks1, ks2; /* AES key schedules to use */
    XTS128_CONTEXT xts;
    OSSL_xts_stream_fn stream;

    /* Platform specific data */
    union {
        int dummy;
#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__)
        struct {
            union {
                OSSL_UNION_ALIGN;
                S390X_KM_XTS_PARAMS km;
            } param;
            size_t offset;
            unsigned int fc;
            unsigned int iv_set : 1;
            unsigned int key_set : 1;
        } s390x;
#endif
    } plat;
} PROV_AES_XTS_CTX;

int ossl_cipher_set_aes_xts_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_enc_key,
    aes_set_encrypt_key_fn fn_set_dec_key,
    aes_block128_f fn_block_enc, aes_block128_f fn_block_dec,
    OSSL_xts_stream_fn fn_stream_enc, OSSL_xts_stream_fn fn_stream_dec);

void ossl_cipher_hw_aes_xts_copyctx(PROV_CIPHER_CTX *dst,
    const PROV_CIPHER_CTX *src);

#if defined(AESNI_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_aesni(void);
#endif

#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_rv32i(void);
#endif

#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_rv64i(void);
#endif

#ifdef AES_XTS_S390X
int s390x_aes_xts_cipher_stream(PROV_AES_XTS_CTX *xctx,
    unsigned char *out, size_t *outl,
    const unsigned char *in, size_t inl);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_s390x(size_t keybits);
#endif

#if defined(SPARC_AES_CAPABLE)
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_t4(void);
#endif

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts(size_t keybits);

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_XTS_H) */
