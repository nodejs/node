/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Dispatch functions for AES GCM mode */

/*
 * This file uses the low level AES functions (which are deprecated for
 * non-internal use) in order to implement provider AES ciphers.
 */
#include "internal/deprecated.h"
#include <openssl/proverr.h>
#include "cipher_aes_gcm.h"

int ossl_aes_gcm_hw_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen, aes_set_encrypt_key_fn fn_set_key,
    aes_block128_f fn_block, ctr128_f fn_ctr)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    AES_KEY *ks = &actx->ks.ks;

    int ret = fn_set_key(key, (int)(keylen * 8), ks);
    if (ret < 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
        return 0;
    }

    CRYPTO_gcm128_init(&ctx->gcm, ks, (block128_f)fn_block);
    ctx->ctr = fn_ctr;
    ctx->key_set = 1;

    return 1;
}

static int aes_gcm_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
#ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
#ifdef HWAES_ctr32_encrypt_blocks
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, HWAES_set_encrypt_key,
            HWAES_encrypt, HWAES_ctr32_encrypt_blocks);
#else
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, HWAES_set_encrypt_key,
            HWAES_encrypt, NULL);
#endif /* HWAES_ctr32_encrypt_blocks */
    } else
#endif /* HWAES_CAPABLE */

#ifdef BSAES_CAPABLE
        if (BSAES_CAPABLE) {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, AES_set_encrypt_key,
            AES_encrypt, (ctr128_f)ossl_bsaes_ctr32_encrypt_blocks);
    } else
#endif /* BSAES_CAPABLE */

#ifdef VPAES_CAPABLE
        if (VPAES_CAPABLE) {
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, vpaes_set_encrypt_key,
            vpaes_encrypt, NULL);
    } else
#endif /* VPAES_CAPABLE */

    {
#ifdef AES_CTR_ASM
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, AES_set_encrypt_key,
            AES_encrypt, (ctr128_f)AES_ctr32_encrypt);
#else
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen, AES_set_encrypt_key,
            AES_encrypt, NULL);
#endif /* AES_CTR_ASM */
    }
}

int ossl_generic_aes_gcm_cipher_update(PROV_GCM_CTX *ctx,
    const unsigned char *in, size_t len, unsigned char *out)
{
    if (ctx->enc) {
        if (ctx->ctr != NULL) {
#if defined(AES_GCM_ASM)
            size_t bulk = 0;

            if (len >= AES_GCM_ENC_BYTES && AES_GCM_ASM(ctx)) {
                size_t res = (16 - ctx->gcm.mres) % 16;

                if (CRYPTO_gcm128_encrypt(&ctx->gcm, in, out, res))
                    return 0;

                bulk = AES_gcm_encrypt(in + res, out + res, len - res,
                    ctx->gcm.key,
                    ctx->gcm.Yi.c, ctx->gcm.Xi.u);

                ctx->gcm.len.u[1] += bulk;
                bulk += res;
            }
            if (CRYPTO_gcm128_encrypt_ctr32(&ctx->gcm, in + bulk, out + bulk,
                    len - bulk, ctx->ctr))
                return 0;
#else
            if (CRYPTO_gcm128_encrypt_ctr32(&ctx->gcm, in, out, len, ctx->ctr))
                return 0;
#endif /* AES_GCM_ASM */
        } else {
            if (CRYPTO_gcm128_encrypt(&ctx->gcm, in, out, len))
                return 0;
        }
    } else {
        if (ctx->ctr != NULL) {
#if defined(AES_GCM_ASM)
            size_t bulk = 0;

            if (len >= AES_GCM_DEC_BYTES && AES_GCM_ASM(ctx)) {
                size_t res = (16 - ctx->gcm.mres) % 16;

                if (CRYPTO_gcm128_decrypt(&ctx->gcm, in, out, res))
                    return 0;

                bulk = AES_gcm_decrypt(in + res, out + res, len - res,
                    ctx->gcm.key,
                    ctx->gcm.Yi.c, ctx->gcm.Xi.u);

                ctx->gcm.len.u[1] += bulk;
                bulk += res;
            }
            if (CRYPTO_gcm128_decrypt_ctr32(&ctx->gcm, in + bulk, out + bulk,
                    len - bulk, ctx->ctr))
                return 0;
#else
            if (CRYPTO_gcm128_decrypt_ctr32(&ctx->gcm, in, out, len, ctx->ctr))
                return 0;
#endif /* AES_GCM_ASM */
        } else {
            if (CRYPTO_gcm128_decrypt(&ctx->gcm, in, out, len))
                return 0;
        }
    }
    return 1;
}

static const PROV_GCM_HW aes_gcm = {
    aes_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

const PROV_GCM_HW *ossl_prov_aes_hw_gcm(size_t keybits)
{
    const PROV_GCM_HW *aes_gcm_hw = NULL;

#if defined(AESNI_CAPABLE)
    aes_gcm_hw = ossl_prov_aes_hw_gcm_aesni();
#elif defined(AES_PMULL_CAPABLE) && defined(AES_GCM_ASM)
    aes_gcm_hw = ossl_prov_aes_hw_gcm_armv8();
#elif defined(PPC_AES_GCM_CAPABLE) && defined(_ARCH_PPC64)
    aes_gcm_hw = ossl_prov_aes_hw_gcm_ppc(keybits);
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
    aes_gcm_hw = ossl_prov_aes_hw_gcm_rv32i();
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
    aes_gcm_hw = ossl_prov_aes_hw_gcm_rv64i();
#elif defined(S390X_aes_128_CAPABLE)
    aes_gcm_hw = ossl_prov_aes_hw_gcm_s390x(keybits);
#elif defined(SPARC_AES_CAPABLE)
    aes_gcm_hw = ossl_prov_aes_hw_gcm_t4();
#endif

    if (aes_gcm_hw == NULL)
        aes_gcm_hw = &aes_gcm;

    return aes_gcm_hw;
}
