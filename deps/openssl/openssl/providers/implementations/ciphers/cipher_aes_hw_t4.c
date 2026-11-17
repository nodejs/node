/*
 * Copyright 2001-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * Sparc t4 support for all hardware accelerated AES modes.
 */

#include "internal/deprecated.h"
#include <openssl/proverr.h>
#include "cipher_aes.h"
#include "cipher_aes_gcm.h"
#include "cipher_aes_ccm.h"
#include "cipher_aes_xts.h"

#if defined(SPARC_AES_CAPABLE)

/* MODES: ecb, cbc, cfb, ofb, ctr */

static int t4_set_encrypt_key(const unsigned char *key, int bits, AES_KEY *ks)
{
    aes_t4_set_encrypt_key(key, bits, ks);
    return 0;
}

static int t4_set_decrypt_key(const unsigned char *key, int bits, AES_KEY *ks)
{
    aes_t4_set_decrypt_key(key, bits, ks);
    return 0;
}

static int cipher_hw_aes_t4_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    cbc128_f fn_cbc = NULL;
    if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
        && !ctx->enc) {
        switch (keylen) {
        case 16:
            fn_cbc = (cbc128_f)aes128_t4_cbc_decrypt;
            break;
        case 24:
            fn_cbc = (cbc128_f)aes192_t4_cbc_decrypt;
            break;
        case 32:
            fn_cbc = (cbc128_f)aes256_t4_cbc_decrypt;
            break;
        default:
            ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
            return 0;
        }
        return ossl_cipher_set_aes_initkey(ctx, key, keylen, t4_set_decrypt_key,
            aes_t4_decrypt, NULL, fn_cbc, NULL);
    } else {
        ctr128_f fn_ctr = NULL;
        switch (keylen) {
        case 16:
            fn_cbc = (cbc128_f)aes128_t4_cbc_encrypt;
            fn_ctr = (ctr128_f)aes128_t4_ctr32_encrypt;
            break;
        case 24:
            fn_cbc = (cbc128_f)aes192_t4_cbc_encrypt;
            fn_ctr = (ctr128_f)aes192_t4_ctr32_encrypt;
            break;
        case 32:
            fn_cbc = (cbc128_f)aes256_t4_cbc_encrypt;
            fn_ctr = (ctr128_f)aes256_t4_ctr32_encrypt;
            break;
        default:
            ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
            return 0;
        }
        return ossl_cipher_set_aes_initkey(ctx, key, keylen, t4_set_encrypt_key,
            aes_t4_encrypt, NULL, fn_cbc, fn_ctr);
    }
}

static const PROV_CIPHER_HW aes_t4_ecb = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_ecb,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_cbc = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_cbc,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_cfb128 = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_cfb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_cfb8 = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_cfb8,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_cfb1 = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_cfb1,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_ofb128 = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_ofb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aes_t4_ctr = {
    cipher_hw_aes_t4_initkey,
    ossl_cipher_hw_generic_ctr,
    ossl_cipher_aes_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_t4(enum aes_modes mode)
{
    if (SPARC_AES_CAPABLE) {
        switch (mode) {
        case AES_MODE_ECB:
            return &aes_t4_ecb;
        case AES_MODE_CBC:
            return &aes_t4_cbc;
        case AES_MODE_CFB128:
            return &aes_t4_cfb128;
        case AES_MODE_CFB8:
            return &aes_t4_cfb8;
        case AES_MODE_CFB1:
            return &aes_t4_cfb1;
        case AES_MODE_OFB128:
            return &aes_t4_ofb128;
        case AES_MODE_CTR:
            return &aes_t4_ctr;
        default:
            return NULL;
        }
    }
    return NULL;
}

/* MODES: GCM */

static int t4_aes_gcm_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    switch (keylen) {
    case 16:
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            t4_set_encrypt_key, aes_t4_encrypt,
            (ctr128_f)aes128_t4_ctr32_encrypt);
    case 24:
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            t4_set_encrypt_key, aes_t4_encrypt,
            (ctr128_f)aes192_t4_ctr32_encrypt);
    case 32:
        return ossl_aes_gcm_hw_initkey(ctx, key, keylen,
            t4_set_encrypt_key, aes_t4_encrypt,
            (ctr128_f)aes256_t4_ctr32_encrypt);
    default:
        return 0;
    }
}

static const PROV_GCM_HW t4_aes_gcm = {
    t4_aes_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

const PROV_GCM_HW *ossl_prov_aes_hw_gcm_t4(void)
{
    return SPARC_AES_CAPABLE ? &t4_aes_gcm : NULL;
}

/* MODES: CCM */

static int ccm_t4_aes_initkey(PROV_CCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
        t4_set_encrypt_key, aes_t4_encrypt, NULL, NULL);
}

static const PROV_CCM_HW t4_aes_ccm = {
    ccm_t4_aes_initkey,
    ossl_ccm_generic_setiv,
    ossl_ccm_generic_setaad,
    ossl_ccm_generic_auth_encrypt,
    ossl_ccm_generic_auth_decrypt,
    ossl_ccm_generic_gettag
};

const PROV_CCM_HW *ossl_prov_aes_hw_ccm_t4(void)
{
    if (SPARC_AES_CAPABLE)
        return &t4_aes_ccm;
    return NULL;
}

/* MODES: XTS */

static int cipher_hw_aes_xts_t4_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    OSSL_xts_stream_fn stream_enc = NULL;
    OSSL_xts_stream_fn stream_dec = NULL;

    /* Note: keylen is the size of 2 keys */
    switch (keylen) {
    case 32:
        stream_enc = aes128_t4_xts_encrypt;
        stream_dec = aes128_t4_xts_decrypt;
        break;
    case 64:
        stream_enc = aes256_t4_xts_encrypt;
        stream_dec = aes256_t4_xts_decrypt;
        break;
    default:
        return 0;
    }

    return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
        t4_set_encrypt_key, t4_set_decrypt_key,
        aes_t4_encrypt, aes_t4_decrypt, stream_enc, stream_dec);
}

static const PROV_CIPHER_HW aes_xts_t4 = {
    cipher_hw_aes_xts_t4_initkey,
    NULL,
    ossl_cipher_hw_aes_xts_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_t4(void)
{
    if (SPARC_AES_CAPABLE)
        return &aes_xts_t4;
    return NULL;
}

#endif
