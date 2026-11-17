/*
 * Copyright 2001-2026 The OpenSSL Project Authors. All Rights Reserved.
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

int ossl_cipher_set_aes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_key, aes_block128_f fn_block,
    ecb128_f fn_ecb, cbc128_f fn_cbc, ctr128_f fn_ctr)
{
    PROV_AES_CTX *actx = (PROV_AES_CTX *)ctx;
    AES_KEY *ks = &actx->ks.ks;

    int ret = fn_set_key(key, (int)(keylen * 8), ks);
    if (ret < 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
        return 0;
    }
    ctx->ks = ks;

    ctx->block = (block128_f)fn_block;

    switch (ctx->mode) {
    case EVP_CIPH_ECB_MODE:
        ctx->stream.ecb = fn_ecb;
        break;
    case EVP_CIPH_CBC_MODE:
        ctx->stream.cbc = fn_cbc;
        break;
    case EVP_CIPH_CTR_MODE:
        ctx->stream.ctr = fn_ctr;
        break;
    default:
        memset(&ctx->stream, 0, sizeof(ctx->stream));
        break;
    }

    return 1;
}

#ifdef HWAES_CAPABLE
static int hwaes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if (HWAES_CAPABLE) {
        ecb128_f fn_ecb = NULL;
        cbc128_f fn_cbc = NULL;
        ctr128_f fn_ctr = NULL;
#ifdef HWAES_ecb_encrypt
        fn_ecb = (ecb128_f)HWAES_ecb_encrypt;
#endif
#ifdef HWAES_cbc_encrypt
        fn_cbc = (cbc128_f)HWAES_cbc_encrypt;
#endif
#ifdef HWAES_ctr32_encrypt_blocks
        fn_ctr = (ctr128_f)HWAES_ctr32_encrypt_blocks;
#endif
        if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
            && !ctx->enc)
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                HWAES_set_decrypt_key, HWAES_decrypt, fn_ecb, fn_cbc, fn_ctr);
        else
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                HWAES_set_encrypt_key, HWAES_encrypt, fn_ecb, fn_cbc, fn_ctr);
    }
    return -1;
}
#endif /* HWAES_CAPABLE */

#ifdef BSAES_CAPABLE
static int bsaes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if (BSAES_CAPABLE) {
        if (ctx->mode == EVP_CIPH_CBC_MODE && !ctx->enc)
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                AES_set_decrypt_key, AES_decrypt, NULL,
                (cbc128_f)ossl_bsaes_cbc_encrypt, NULL);
        else if (ctx->mode == EVP_CIPH_CTR_MODE)
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                AES_set_encrypt_key, AES_encrypt, NULL, NULL,
                (ctr128_f)ossl_bsaes_ctr32_encrypt_blocks);
    }
    return -1;
}
#endif /* BSAES_CAPABLE */

#ifdef VPAES_CAPABLE
static int vpaes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if (VPAES_CAPABLE) {
        if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
            && !ctx->enc) {
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                vpaes_set_decrypt_key, vpaes_decrypt, NULL,
                (cbc128_f)vpaes_cbc_encrypt, NULL);
        } else {
            return ossl_cipher_set_aes_initkey(ctx, key, keylen,
                vpaes_set_encrypt_key, vpaes_encrypt, NULL,
                (cbc128_f)vpaes_cbc_encrypt, NULL);
        }
    }
    return -1;
}
#endif /* VPAES_CAPABLE */

int ossl_cipher_hw_aes_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    int ret = 0;

#ifdef HWAES_CAPABLE
    ret = hwaes_initkey(ctx, key, keylen);
    if (ret >= 0)
        return ret;
#endif

#ifdef BSAES_CAPABLE
    ret = bsaes_initkey(ctx, key, keylen);
    if (ret >= 0)
        return ret;
#endif

#ifdef VPAES_CAPABLE
    ret = vpaes_initkey(ctx, key, keylen);
    if (ret >= 0)
        return ret;
#endif

    if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
        && !ctx->enc) {
        ret = ossl_cipher_set_aes_initkey(ctx, key, keylen,
            AES_set_decrypt_key, AES_decrypt, NULL, (cbc128_f)AES_cbc_encrypt,
            NULL);
    } else {
        ctr128_f fn_ctr = NULL;
#ifdef AES_CTR_ASM
        fn_ctr = (ctr128_f)AES_ctr32_encrypt;
#endif
        ret = ossl_cipher_set_aes_initkey(ctx, key, keylen,
            AES_set_encrypt_key, AES_encrypt, NULL, (cbc128_f)AES_cbc_encrypt,
            fn_ctr);
    }

    return ret;
}

void ossl_cipher_aes_copyctx(PROV_CIPHER_CTX *dst,
    const PROV_CIPHER_CTX *src)
{
    PROV_AES_CTX *sctx = (PROV_AES_CTX *)src;
    PROV_AES_CTX *dctx = (PROV_AES_CTX *)dst;

    *dctx = *sctx;
    dst->ks = &dctx->ks.ks;
}

static const PROV_CIPHER_HW aes_ecb = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_ecb,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_cbc = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_cbc,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_cfb128 = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_cfb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_cfb8 = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_cfb8,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_cfb1 = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_cfb1,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_ofb128 = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_ofb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_ctr = {
    ossl_cipher_hw_aes_initkey,
    ossl_cipher_hw_generic_ctr,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_mode(enum aes_modes mode,
    size_t keybits)
{
    const PROV_CIPHER_HW *aes_hw_mode = NULL;

#if defined(AESNI_CAPABLE)
    aes_hw_mode = ossl_prov_cipher_hw_aesni(mode);
#elif defined(ARMv8_HWAES_CAPABLE)
    aes_hw_mode = ossl_prov_cipher_hw_arm(mode);
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
    aes_hw_mode = ossl_prov_cipher_hw_rv32i(mode);
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
    aes_hw_mode = ossl_prov_cipher_hw_rv64i(mode);
#elif defined(S390X_aes_128_CAPABLE)
    aes_hw_mode = ossl_prov_cipher_hw_s390x(mode, keybits);
#elif defined(SPARC_AES_CAPABLE)
    aes_hw_mode = ossl_prov_cipher_hw_t4(mode);
#endif

    if (aes_hw_mode == NULL) {
        switch (mode) {
        case AES_MODE_ECB:
            return &aes_ecb;
        case AES_MODE_CBC:
            return &aes_cbc;
        case AES_MODE_CFB128:
            return &aes_cfb128;
        case AES_MODE_CFB8:
            return &aes_cfb8;
        case AES_MODE_CFB1:
            return &aes_cfb1;
        case AES_MODE_OFB128:
            return &aes_ofb128;
        case AES_MODE_CTR:
            return &aes_ctr;
        }
    }

    return aes_hw_mode;
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ecb(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_ECB, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cbc(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_CBC, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb128(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_CFB128, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb8(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_CFB8, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb1(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_CFB1, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ofb128(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_OFB128, keybits);
}

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ctr(size_t keybits)
{
    return ossl_prov_cipher_hw_aes_mode(AES_MODE_CTR, keybits);
}
