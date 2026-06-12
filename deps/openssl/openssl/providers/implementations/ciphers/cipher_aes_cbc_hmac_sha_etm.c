/*
 * Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/deprecated.h"

#include "cipher_aes_cbc_hmac_sha_etm.h"
#include "prov/providercommon.h"
#include "prov/ciphercommon_aead.h"
#include "prov/implementations.h"

#ifndef AES_CBC_HMAC_SHA_ETM_CAPABLE
# define IMPLEMENT_CIPHER(nm, sub, kbits, blkbits, ivbits, flags)              \
const OSSL_DISPATCH ossl_##nm##kbits##sub##_functions[] = {                    \
    OSSL_DISPATCH_END                                                          \
};
#else

# include "providers/implementations/ciphers/cipher_aes_cbc_hmac_sha_etm.inc"

static OSSL_FUNC_cipher_encrypt_init_fn aes_einit;
static OSSL_FUNC_cipher_decrypt_init_fn aes_dinit;
static OSSL_FUNC_cipher_gettable_ctx_params_fn aes_gettable_ctx_params;
static OSSL_FUNC_cipher_settable_ctx_params_fn aes_settable_ctx_params;
# define aes_gettable_params ossl_cipher_generic_gettable_params
# define aes_update ossl_cipher_generic_stream_update
# define aes_final ossl_cipher_generic_stream_final
# define aes_cipher ossl_cipher_generic_cipher

static int aes_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_AES_HMAC_SHA_ETM_CTX *ctx = (PROV_AES_HMAC_SHA_ETM_CTX *)vctx;
    PROV_CIPHER_HW_AES_HMAC_SHA_ETM *hw;
    struct aes_cbc_hmac_sha_etm_set_ctx_params_st p;

    if (ctx == NULL || !aes_cbc_hmac_sha_etm_set_ctx_params_decoder(params, &p))
        return 0;

    hw = (PROV_CIPHER_HW_AES_HMAC_SHA_ETM *)ctx->hw;
    if (p.key != NULL) {
        if (p.key->data_type != OSSL_PARAM_OCTET_STRING) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        hw->init_mac_key(ctx, p.key->data, p.key->data_size);
    }

    if (p.keylen != NULL) {
        size_t keylen;

        if (!OSSL_PARAM_get_size_t(p.keylen, &keylen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (ctx->base.keylen != keylen) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
            return 0;
        }
    }

    if (p.mac != NULL) {
        size_t sz;
        void *vp;

        vp = &ctx->exp_tag;
        if (!OSSL_PARAM_get_octet_string(p.mac, &vp, AES_CBC_MAX_HMAC_SIZE, &sz)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (sz == 0) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_TAG);
            return 0;
        }
        ctx->taglen = sz;
    }

    return 1;
}

static int aes_einit(void *ctx, const unsigned char *key, size_t keylen,
                     const unsigned char *iv, size_t ivlen,
                     const OSSL_PARAM params[])
{
    if (!ossl_cipher_generic_einit(ctx, key, keylen, iv, ivlen, NULL))
        return 0;
    return aes_set_ctx_params(ctx, params);
}

static int aes_dinit(void *ctx, const unsigned char *key, size_t keylen,
                     const unsigned char *iv, size_t ivlen,
                     const OSSL_PARAM params[])
{
    if (!ossl_cipher_generic_dinit(ctx, key, keylen, iv, ivlen, NULL))
        return 0;
    return aes_set_ctx_params(ctx, params);
}

static int aes_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    PROV_AES_HMAC_SHA_ETM_CTX *ctx = (PROV_AES_HMAC_SHA_ETM_CTX *)vctx;
    struct aes_cbc_hmac_sha_etm_get_ctx_params_st p;
    size_t sz;

    if (ctx == NULL || !aes_cbc_hmac_sha_etm_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.keylen != NULL && !OSSL_PARAM_set_size_t(p.keylen, ctx->base.keylen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.ivlen != NULL && !OSSL_PARAM_set_size_t(p.ivlen, ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.iv != NULL
        && !OSSL_PARAM_set_octet_string(p.iv, ctx->base.oiv, ctx->base.ivlen)
        && !OSSL_PARAM_set_octet_ptr(p.iv, &ctx->base.oiv, ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.upd_iv != NULL
        && !OSSL_PARAM_set_octet_string(p.upd_iv, ctx->base.iv, ctx->base.ivlen)
        && !OSSL_PARAM_set_octet_ptr(p.upd_iv, &ctx->base.iv, ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.mac != NULL) {
        sz = p.mac->data_size;
        if (sz == 0
            || sz > AES_CBC_MAX_HMAC_SIZE
            || !ctx->base.enc
            || ctx->taglen == UNINITIALISED_SIZET) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_TAG);
            return 0;
        }
        if (!OSSL_PARAM_set_octet_string(p.mac, ctx->tag, sz)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
            return 0;
        }
    }
    return 1;
}

const OSSL_PARAM *aes_gettable_ctx_params(ossl_unused void *cctx,
                                          ossl_unused void *provctx)
{
    return aes_cbc_hmac_sha_etm_get_ctx_params_list;
}

const OSSL_PARAM *aes_settable_ctx_params(ossl_unused void *cctx,
                                          ossl_unused void *provctx)
{
    return aes_cbc_hmac_sha_etm_set_ctx_params_list;
}

static void base_ctx_init(void *provctx, PROV_AES_HMAC_SHA_ETM_CTX *ctx,
                          const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *meths,
                          size_t kbits, size_t blkbits, size_t ivbits,
                          uint64_t flags)
{
    ossl_cipher_generic_initkey(&ctx->base, kbits, blkbits, ivbits,
                                EVP_CIPH_CBC_MODE, flags,
                                &meths->base, provctx);
    ctx->hw = (PROV_CIPHER_HW_AES_HMAC_SHA_ETM *)ctx->base.hw;
}

static void *aes_cbc_hmac_sha1_etm_newctx(void *provctx, size_t kbits,
                                          size_t blkbits, size_t ivbits,
                                          uint64_t flags)
{
    PROV_AES_HMAC_SHA1_ETM_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        base_ctx_init(provctx, &ctx->base_ctx,
                      ossl_prov_cipher_hw_aes_cbc_hmac_sha1_etm(), kbits, blkbits,
                      ivbits, flags);
    return ctx;
}

static void aes_cbc_hmac_sha1_etm_freectx(void *vctx)
{
    PROV_AES_HMAC_SHA1_ETM_CTX *ctx = (PROV_AES_HMAC_SHA1_ETM_CTX *)vctx;

    if (ctx != NULL) {
        ossl_cipher_generic_reset_ctx((PROV_CIPHER_CTX *)vctx);
        OPENSSL_clear_free(ctx, sizeof(*ctx));
    }
}

static void *aes_cbc_hmac_sha1_etm_dupctx(void *provctx)
{
    PROV_AES_HMAC_SHA1_ETM_CTX *ctx = provctx;

    if (ctx == NULL)
        return NULL;

    return OPENSSL_memdup(ctx, sizeof(*ctx));
}

static void *aes_cbc_hmac_sha256_etm_newctx(void *provctx, size_t kbits,
                                            size_t blkbits, size_t ivbits,
                                            uint64_t flags)
{
    PROV_AES_HMAC_SHA256_ETM_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        base_ctx_init(provctx, &ctx->base_ctx,
                      ossl_prov_cipher_hw_aes_cbc_hmac_sha256_etm(), kbits, blkbits,
                      ivbits, flags);
    return ctx;
}

static void aes_cbc_hmac_sha256_etm_freectx(void *vctx)
{
    PROV_AES_HMAC_SHA256_ETM_CTX *ctx = (PROV_AES_HMAC_SHA256_ETM_CTX *)vctx;

    if (ctx != NULL) {
        ossl_cipher_generic_reset_ctx((PROV_CIPHER_CTX *)vctx);
        OPENSSL_clear_free(ctx, sizeof(*ctx));
    }
}

static void *aes_cbc_hmac_sha256_etm_dupctx(void *provctx)
{
    PROV_AES_HMAC_SHA256_ETM_CTX *ctx = provctx;

    if (ctx == NULL)
        return NULL;

    return OPENSSL_memdup(ctx, sizeof(*ctx));
}

static void *aes_cbc_hmac_sha512_etm_newctx(void *provctx, size_t kbits,
                                            size_t blkbits, size_t ivbits,
                                            uint64_t flags)
{
    PROV_AES_HMAC_SHA512_ETM_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        base_ctx_init(provctx, &ctx->base_ctx,
                      ossl_prov_cipher_hw_aes_cbc_hmac_sha512_etm(), kbits, blkbits,
                      ivbits, flags);
    return ctx;
}

static void aes_cbc_hmac_sha512_etm_freectx(void *vctx)
{
    PROV_AES_HMAC_SHA512_ETM_CTX *ctx = (PROV_AES_HMAC_SHA512_ETM_CTX *)vctx;

    if (ctx != NULL) {
        ossl_cipher_generic_reset_ctx((PROV_CIPHER_CTX *)vctx);
        OPENSSL_clear_free(ctx, sizeof(*ctx));
    }
}

static void *aes_cbc_hmac_sha512_etm_dupctx(void *provctx)
{
    PROV_AES_HMAC_SHA512_ETM_CTX *ctx = provctx;

    if (ctx == NULL)
        return NULL;

    return OPENSSL_memdup(ctx, sizeof(*ctx));
}

# define IMPLEMENT_CIPHER(nm, sub, kbits, blkbits, ivbits, flags)              \
static OSSL_FUNC_cipher_newctx_fn nm##_##kbits##_##sub##_newctx;               \
static void *nm##_##kbits##_##sub##_newctx(void *provctx)                      \
{                                                                              \
    return nm##_##sub##_newctx(provctx, kbits, blkbits, ivbits, flags);        \
}                                                                              \
static OSSL_FUNC_cipher_get_params_fn nm##_##kbits##_##sub##_get_params;       \
static int nm##_##kbits##_##sub##_get_params(OSSL_PARAM params[])              \
{                                                                              \
    return ossl_cipher_generic_get_params(params, EVP_CIPH_CBC_MODE,           \
                                          flags, kbits, blkbits, ivbits);      \
}                                                                              \
const OSSL_DISPATCH ossl_##nm##kbits##sub##_functions[] = {                    \
    { OSSL_FUNC_CIPHER_NEWCTX, (void (*)(void))nm##_##kbits##_##sub##_newctx },\
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void))nm##_##sub##_freectx },        \
    { OSSL_FUNC_CIPHER_DUPCTX,  (void (*)(void))nm##_##sub##_dupctx},          \
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))nm##_einit },             \
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))nm##_dinit },             \
    { OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))nm##_update },                  \
    { OSSL_FUNC_CIPHER_FINAL, (void (*)(void))nm##_final },                    \
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))nm##_cipher },                  \
    { OSSL_FUNC_CIPHER_GET_PARAMS,                                             \
        (void (*)(void))nm##_##kbits##_##sub##_get_params },                   \
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,                                        \
        (void (*)(void))nm##_gettable_params },                                \
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS,                                         \
         (void (*)(void))nm##_get_ctx_params },                                \
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,                                    \
        (void (*)(void))nm##_gettable_ctx_params },                            \
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS,                                         \
        (void (*)(void))nm##_set_ctx_params },                                 \
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,                                    \
        (void (*)(void))nm##_settable_ctx_params },                            \
    OSSL_DISPATCH_END                                                          \
};
#endif /* AES_CBC_HMAC_SHA_ETM_CAPABLE */

/* ossl_aes128cbc_hmac_sha1_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha1_etm, 128, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes192cbc_hmac_sha1_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha1_etm, 192, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes256cbc_hmac_sha1_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha1_etm, 256, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes128cbc_hmac_sha256_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha256_etm, 128, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes192cbc_hmac_sha256_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha256_etm, 192, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes256cbc_hmac_sha256_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha256_etm, 256, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes128cbc_hmac_sha512_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha512_etm, 128, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes192cbc_hmac_sha512_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha512_etm, 192, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
/* ossl_aes256cbc_hmac_sha512_etm_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha512_etm, 256, 128, 128, EVP_CIPH_FLAG_ENC_THEN_MAC)
