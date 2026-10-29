/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_GCM_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_GCM_H

#include <openssl/aes.h>
#include "prov/ciphercommon.h"
#include "prov/ciphercommon_gcm.h"
#include "crypto/aes_platform.h"

typedef struct prov_aes_gcm_ctx_st {
    PROV_GCM_CTX base; /* must be first entry in struct */
    union {
        OSSL_UNION_ALIGN;
        AES_KEY ks;
    } ks; /* AES key schedule to use */

    /* Platform specific data */
    union {
        int dummy;
#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__)
        struct {
            union {
                OSSL_UNION_ALIGN;
                S390X_KMA_PARAMS kma;
            } param;
            unsigned int fc;
            unsigned int hsflag; /* hash subkey set flag */
            unsigned char ares[16];
            unsigned char mres[16];
            unsigned char kres[16];
            int areslen;
            int mreslen;
            int kreslen;
            int res;
        } s390x;
#endif /* defined(OPENSSL_CPUID_OBJ) && defined(__s390__) */
    } plat;
} PROV_AES_GCM_CTX;

int ossl_aes_gcm_hw_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen, aes_set_encrypt_key_fn fn_set_key,
    aes_block128_f fn_block, ctr128_f fn_ctr);

int ossl_generic_aes_gcm_cipher_update(PROV_GCM_CTX *ctx,
    const unsigned char *in, size_t len, unsigned char *out);

const PROV_GCM_HW *ossl_prov_aes_hw_gcm(size_t keybits);
#if defined(AESNI_CAPABLE)
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_aesni(void);
#endif
#if defined(AES_PMULL_CAPABLE) && defined(AES_GCM_ASM)
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_armv8(void);
#endif
#if defined(PPC_AES_GCM_CAPABLE) && defined(_ARCH_PPC64)
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_ppc(size_t keybits);
#endif
#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_rv32i(void);
#endif
#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_rv64i(void);
#endif
#if defined(S390X_aes_128_CAPABLE)
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_s390x(size_t keybits);
#endif
#if defined(SPARC_AES_CAPABLE)
const PROV_GCM_HW *ossl_prov_aes_hw_gcm_t4(void);
#endif

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_GCM_H) */
