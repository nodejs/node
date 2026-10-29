/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* AES CCM mode */

/*
 * This file uses the low level AES functions (which are deprecated for
 * non-internal use) in order to implement provider AES ciphers.
 */
#include "internal/deprecated.h"
#include <openssl/proverr.h>
#include "cipher_aes_ccm.h"

int ossl_cipher_set_ccm_aes_initkey(PROV_CCM_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_key, aes_block128_f fn_block,
    ccm128_f fn_ccm_enc, ccm128_f fn_ccm_dec)
{
    PROV_AES_CCM_CTX *actx = (PROV_AES_CCM_CTX *)ctx;
    AES_KEY *ks = &actx->ccm.ks.ks;

    int ret = fn_set_key(key, (int)(keylen * 8), ks);
    if (ret < 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
        return 0;
    }
    CRYPTO_ccm128_init(&ctx->ccm_ctx, (unsigned int)ctx->m,
        (unsigned int)ctx->l, ks, (block128_f)fn_block);

    ctx->str = ctx->enc ? fn_ccm_enc : fn_ccm_dec;
    ctx->key_set = 1;

    return 1;
}

static int ccm_generic_aes_initkey(PROV_CCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
#ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
        return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
            HWAES_set_encrypt_key, HWAES_encrypt, NULL, NULL);
    }
#endif

#ifdef VPAES_CAPABLE
    if (VPAES_CAPABLE) {
        return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
            vpaes_set_encrypt_key, vpaes_encrypt, NULL, NULL);
    }
#endif

    return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
        AES_set_encrypt_key, AES_encrypt, NULL, NULL);
}

static const PROV_CCM_HW aes_ccm = {
    ccm_generic_aes_initkey,
    ossl_ccm_generic_setiv,
    ossl_ccm_generic_setaad,
    ossl_ccm_generic_auth_encrypt,
    ossl_ccm_generic_auth_decrypt,
    ossl_ccm_generic_gettag
};

const PROV_CCM_HW *ossl_prov_aes_hw_ccm(size_t keybits)
{
    const PROV_CCM_HW *aes_ccm_hw = NULL;
#if defined(AESNI_CAPABLE)
    aes_ccm_hw = ossl_prov_aes_hw_ccm_aesni();
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
    aes_ccm_hw = ossl_prov_aes_hw_ccm_rv32i();
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
    aes_ccm_hw = ossl_prov_aes_hw_ccm_rv64i();
#elif defined(S390X_aes_128_CAPABLE)
    aes_ccm_hw = ossl_prov_aes_hw_ccm_s390x(keybits);
#elif defined(SPARC_AES_CAPABLE)
    aes_ccm_hw = ossl_prov_aes_hw_ccm_t4();
#endif
    if (aes_ccm_hw != NULL)
        return aes_ccm_hw;
    return &aes_ccm;
}
