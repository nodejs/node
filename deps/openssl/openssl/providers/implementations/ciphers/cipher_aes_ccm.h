/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_CCM_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_CCM_H

#include <openssl/aes.h>
#include "prov/ciphercommon.h"
#include "prov/ciphercommon_ccm.h"
#include "crypto/aes_platform.h"

typedef struct prov_aes_ccm_ctx_st {
    PROV_CCM_CTX base; /* Must be first */
    union {
        OSSL_UNION_ALIGN;
        /*-
         * Padding is chosen so that s390x.kmac.k overlaps with ks.ks and
         * fc with ks.ks.rounds. Remember that on s390x, an AES_KEY's
         * rounds field is used to store the function code and that the key
         * schedule is not stored (if aes hardware support is detected).
         */
        struct {
            unsigned char pad[16];
            AES_KEY ks;
        } ks;
#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__)
        struct {
            S390X_KMAC_PARAMS kmac;
            unsigned long long blocks;
            union {
                unsigned long long g[2];
                unsigned char b[AES_BLOCK_SIZE];
            } nonce;
            union {
                unsigned long long g[2];
                unsigned char b[AES_BLOCK_SIZE];
            } buf;
            unsigned char dummy_pad[168];
            unsigned int fc; /* fc has same offset as ks.ks.rounds */
        } s390x;
#endif /* defined(OPENSSL_CPUID_OBJ) && defined(__s390__) */
    } ccm;
} PROV_AES_CCM_CTX;

int ossl_cipher_set_ccm_aes_initkey(PROV_CCM_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_key, aes_block128_f fn_block,
    ccm128_f fn_ccm_enc, ccm128_f fn_ccm_dec);

const PROV_CCM_HW *ossl_prov_aes_hw_ccm(size_t keylen);

#if defined(AESNI_CAPABLE)
const PROV_CCM_HW *ossl_prov_aes_hw_ccm_aesni(void);
#endif
#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
const PROV_CCM_HW *ossl_prov_aes_hw_ccm_rv32i(void);
#endif
#if defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
const PROV_CCM_HW *ossl_prov_aes_hw_ccm_rv64i(void);
#endif
#if defined(S390X_aes_128_CAPABLE)
const PROV_CCM_HW *ossl_prov_aes_hw_ccm_s390x(size_t keybits);
#endif
#if defined(SPARC_AES_CAPABLE)
const PROV_CCM_HW *ossl_prov_aes_hw_ccm_t4(void);
#endif

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_AES_CCM_H) */
