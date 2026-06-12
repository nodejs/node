/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This file uses the low level AES functions (which are deprecated for
 * non-internal use) in order to implement provider AES ciphers.
 */
#include "internal/deprecated.h"

#include <openssl/proverr.h>
#include "cipher_aes.h"
#include "cipher_aes_cfb.h"

static int cipher_hw_aes_initkey(PROV_CIPHER_CTX *dat,
                                 const unsigned char *key, size_t keylen)
{
    int ret;
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    AES_KEY *ks = &adat->ks.ks;

    dat->ks = ks;

#ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
        ret = HWAES_set_encrypt_key(key, (int)(keylen * 8), ks);
        dat->block = (block128_f)HWAES_encrypt;
        dat->stream.cbc = NULL;
    } else {
#endif
#ifdef VPAES_CAPABLE
        if (VPAES_CAPABLE) {
            ret = vpaes_set_encrypt_key(key, (int)(keylen * 8), ks);
            dat->block = (block128_f)vpaes_encrypt;
            dat->stream.cbc = NULL;
        } else {
#endif
            {
                ret = AES_set_encrypt_key(key, (int)(keylen * 8), ks);
                dat->block = (block128_f)AES_encrypt;
                dat->stream.cbc = NULL;
            }
#ifdef VPAES_CAPABLE
        }
#endif
#ifdef HWAES_CAPABLE
    }
#endif

    if (ret < 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
        return 0;
    }

    return 1;
}

IMPLEMENT_CIPHER_HW_COPYCTX(cipher_hw_aes_copyctx, PROV_AES_CTX)

#define PROV_CIPHER_HW_aes_mode(mode)                                          \
    static const PROV_CIPHER_HW aes_##mode = {                                 \
        cipher_hw_aes_initkey,                                                 \
        ossl_cipher_hw_generic_##mode,                                         \
        cipher_hw_aes_copyctx                                                  \
    };                                                                         \
    PROV_CIPHER_HW_declare(mode)                                               \
    const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_##mode(size_t keybits)       \
    {                                                                          \
        PROV_CIPHER_HW_select(mode)                                            \
        return &aes_##mode;                                                    \
    }

#if defined(AESNI_CAPABLE)
# include "cipher_aes_cfb_hw_aesni.inc"
#elif defined(SPARC_AES_CAPABLE)
# include "cipher_aes_hw_t4.inc"
#elif defined(S390X_aes_128_CAPABLE)
# include "cipher_aes_cfb_hw_s390x.inc"
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
# include "cipher_aes_hw_rv64i.inc"
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
# include "cipher_aes_hw_rv32i.inc"
#elif defined(ARMv8_HWAES_CAPABLE)
# include "cipher_aes_hw_armv8.inc"
#else
/* The generic case */
# define PROV_CIPHER_HW_declare(mode)
# define PROV_CIPHER_HW_select(mode)
#endif

PROV_CIPHER_HW_aes_mode(cfb128)
PROV_CIPHER_HW_aes_mode(cfb1)
PROV_CIPHER_HW_aes_mode(cfb8)
