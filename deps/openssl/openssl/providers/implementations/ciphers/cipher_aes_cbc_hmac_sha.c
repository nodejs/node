/*
 * Copyright 2019-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * AES low level APIs are deprecated for public use, but still ok for internal
 * use where we're using them to implement the higher level EVP interface, as is
 * the case here.
 */
#include "internal/deprecated.h"

/* Dispatch functions for AES_CBC_HMAC_SHA ciphers */

/* For SSL3_VERSION and TLS1_VERSION */
#include <openssl/prov_ssl.h>
#include <openssl/proverr.h>
#include "cipher_aes_cbc_hmac_sha.h"
#include "prov/implementations.h"
#include "prov/providercommon.h"

#ifndef AES_CBC_HMAC_SHA_CAPABLE
# define IMPLEMENT_CIPHER(nm, sub, kbits, blkbits, ivbits, flags)              \
const OSSL_DISPATCH ossl_##nm##kbits##sub##_functions[] = {                    \
    OSSL_DISPATCH_END                                                              \
};
#else

# include "providers/implementations/ciphers/cipher_aes_cbc_hmac_sha.inc"

# define AES_CBC_HMAC_SHA_FLAGS (PROV_CIPHER_FLAG_AEAD                         \
                                 | PROV_CIPHER_FLAG_TLS1_MULTIBLOCK)

static OSSL_FUNC_cipher_encrypt_init_fn aes_einit;
static OSSL_FUNC_cipher_decrypt_init_fn aes_dinit;
static OSSL_FUNC_cipher_freectx_fn aes_cbc_hmac_sha1_freectx;
static OSSL_FUNC_cipher_freectx_fn aes_cbc_hmac_sha256_freectx;
static OSSL_FUNC_cipher_get_ctx_params_fn aes_get_ctx_params;
static OSSL_FUNC_cipher_gettable_ctx_params_fn aes_gettable_ctx_params;
static OSSL_FUNC_cipher_set_ctx_params_fn aes_set_ctx_params;
static OSSL_FUNC_cipher_settable_ctx_params_fn aes_settable_ctx_params;
# define aes_gettable_params ossl_cipher_generic_gettable_params
# define aes_update ossl_cipher_generic_stream_update
# define aes_final ossl_cipher_generic_stream_final
# define aes_cipher ossl_cipher_generic_cipher

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

const OSSL_PARAM *aes_settable_ctx_params(ossl_unused void *cctx,
                                          ossl_unused void *provctx)
{
    return aes_cbc_hmac_sha_set_ctx_params_list;
}

static int aes_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_AES_HMAC_SHA_CTX *ctx = (PROV_AES_HMAC_SHA_CTX *)vctx;
    PROV_CIPHER_HW_AES_HMAC_SHA *hw;
    struct aes_cbc_hmac_sha_set_ctx_params_st p;
    int ret = 1;
# if !defined(OPENSSL_NO_MULTIBLOCK)
    EVP_CTRL_TLS1_1_MULTIBLOCK_PARAM mb_param;
# endif

    if (ctx == NULL || !aes_cbc_hmac_sha_set_ctx_params_decoder(params, &p))
        return 0;

    hw = (PROV_CIPHER_HW_AES_HMAC_SHA *)ctx->hw;

    if (p.key != NULL) {
        if (p.key->data_type != OSSL_PARAM_OCTET_STRING) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        hw->init_mac_key(ctx, p.key->data, p.key->data_size);
    }

# if !defined(OPENSSL_NO_MULTIBLOCK)
    if (p.maxfrag != NULL
            && !OSSL_PARAM_get_size_t(p.maxfrag, &ctx->multiblock_max_send_fragment)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }
    /*
     * The inputs to tls1_multiblock_aad are:
     *   mb_param->inp
     *   mb_param->len
     *   mb_param->interleave
     * The outputs of tls1_multiblock_aad are written to:
     *   ctx->multiblock_interleave
     *   ctx->multiblock_aad_packlen
     */
    if (p.mb_aad != NULL) {
        if (p.mb_aad->data_type != OSSL_PARAM_OCTET_STRING
            || p.ileave == NULL
            || !OSSL_PARAM_get_uint(p.ileave, &mb_param.interleave)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        mb_param.inp = p.mb_aad->data;
        mb_param.len = p.mb_aad->data_size;
        if (hw->tls1_multiblock_aad(vctx, &mb_param) <= 0)
            return 0;
    }

    /*
     * The inputs to tls1_multiblock_encrypt are:
     *   mb_param->inp
     *   mb_param->len
     *   mb_param->interleave
     *   mb_param->out
     * The outputs of tls1_multiblock_encrypt are:
     *   ctx->multiblock_encrypt_len
     */
    if (p.enc != NULL) {
        if (p.enc->data_type != OSSL_PARAM_OCTET_STRING
            || p.enc_in == NULL
            || p.enc_in->data_type != OSSL_PARAM_OCTET_STRING
            || p.ileave == NULL
            || !OSSL_PARAM_get_uint(p.ileave, &mb_param.interleave)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        mb_param.out = p.enc->data;
        mb_param.inp = p.enc_in->data;
        mb_param.len = p.enc_in->data_size;
        if (hw->tls1_multiblock_encrypt(vctx, &mb_param) <= 0)
            return 0;
    }
# endif /* !defined(OPENSSL_NO_MULTIBLOCK) */

    if (p.tlsaad != NULL) {
        if (p.tlsaad->data_type != OSSL_PARAM_OCTET_STRING || p.tlsaad->data_size > INT_MAX) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (hw->set_tls1_aad(ctx, p.tlsaad->data, (int)p.tlsaad->data_size) <= 0)
            return 0;
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

    if (p.tlsver != NULL) {
        if (!OSSL_PARAM_get_uint(p.tlsver, &ctx->base.tlsversion)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (ctx->base.tlsversion == SSL3_VERSION
                || ctx->base.tlsversion == TLS1_VERSION) {
            if (!ossl_assert(ctx->base.removetlsfixed >= AES_BLOCK_SIZE)) {
                ERR_raise(ERR_LIB_PROV, ERR_R_INTERNAL_ERROR);
                return 0;
            }
            /*
             * There is no explicit IV with these TLS versions, so don't attempt
             * to remove it.
             */
            ctx->base.removetlsfixed -= AES_BLOCK_SIZE;
        }
    }
    return ret;
}

static int aes_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    PROV_AES_HMAC_SHA_CTX *ctx = (PROV_AES_HMAC_SHA_CTX *)vctx;
    struct aes_cbc_hmac_sha_get_ctx_params_st p;

    if (ctx == NULL || !aes_cbc_hmac_sha_get_ctx_params_decoder(params, &p))
        return 0;

# if !defined(OPENSSL_NO_MULTIBLOCK)
    if (p.max != NULL) {
        PROV_CIPHER_HW_AES_HMAC_SHA *hw =
           (PROV_CIPHER_HW_AES_HMAC_SHA *)ctx->hw;
        size_t len = hw->tls1_multiblock_max_bufsize(ctx);

        if (!OSSL_PARAM_set_size_t(p.max, len)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
            return 0;
        }
    }

    if (p.inter != NULL
            && !OSSL_PARAM_set_uint(p.inter, ctx->multiblock_interleave)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.packlen != NULL
            && !OSSL_PARAM_set_uint(p.packlen, ctx->multiblock_aad_packlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.enclen != NULL
            && !OSSL_PARAM_set_size_t(p.enclen, ctx->multiblock_encrypt_len)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
# endif /* !defined(OPENSSL_NO_MULTIBLOCK) */

    if (p.pad != NULL && !OSSL_PARAM_set_size_t(p.pad, ctx->tls_aad_pad)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.keylen != NULL && !OSSL_PARAM_set_size_t(p.keylen, ctx->base.keylen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.ivlen != NULL && !OSSL_PARAM_set_size_t(p.ivlen, ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.iv != NULL
        && !OSSL_PARAM_set_octet_string_or_ptr(p.iv, ctx->base.oiv,
                                               ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.upd_iv != NULL
        && !OSSL_PARAM_set_octet_string_or_ptr(p.upd_iv, ctx->base.iv,
                                               ctx->base.ivlen)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    return 1;
}

const OSSL_PARAM *aes_gettable_ctx_params(ossl_unused void *cctx,
                                          ossl_unused void *provctx)
{
    return aes_cbc_hmac_sha_get_ctx_params_list;
}

static void base_init(void *provctx, PROV_AES_HMAC_SHA_CTX *ctx,
                      const PROV_CIPHER_HW_AES_HMAC_SHA *meths,
                      size_t kbits, size_t blkbits, size_t ivbits,
                      uint64_t flags)
{
    ossl_cipher_generic_initkey(&ctx->base, kbits, blkbits, ivbits,
                                EVP_CIPH_CBC_MODE, flags,
                                &meths->base, provctx);
    ctx->hw = (PROV_CIPHER_HW_AES_HMAC_SHA *)ctx->base.hw;
}

static void *aes_cbc_hmac_sha1_newctx(void *provctx, size_t kbits,
                                      size_t blkbits, size_t ivbits,
                                      uint64_t flags)
{
    PROV_AES_HMAC_SHA1_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        base_init(provctx, &ctx->base_ctx,
                  ossl_prov_cipher_hw_aes_cbc_hmac_sha1(), kbits, blkbits,
                  ivbits, flags);
    return ctx;
}

static void *aes_cbc_hmac_sha1_dupctx(void *provctx)
{
    PROV_AES_HMAC_SHA1_CTX *ctx = provctx;

    if (!ossl_prov_is_running())
        return NULL;

    if (ctx == NULL)
        return NULL;

    return OPENSSL_memdup(ctx, sizeof(*ctx));
}

static void aes_cbc_hmac_sha1_freectx(void *vctx)
{
    PROV_AES_HMAC_SHA1_CTX *ctx = (PROV_AES_HMAC_SHA1_CTX *)vctx;

    if (ctx != NULL) {
        ossl_cipher_generic_reset_ctx((PROV_CIPHER_CTX *)vctx);
        OPENSSL_clear_free(ctx, sizeof(*ctx));
    }
}

static void *aes_cbc_hmac_sha256_newctx(void *provctx, size_t kbits,
                                        size_t blkbits, size_t ivbits,
                                        uint64_t flags)
{
    PROV_AES_HMAC_SHA256_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        base_init(provctx, &ctx->base_ctx,
                  ossl_prov_cipher_hw_aes_cbc_hmac_sha256(), kbits, blkbits,
                  ivbits, flags);
    return ctx;
}

static void *aes_cbc_hmac_sha256_dupctx(void *provctx)
{
    PROV_AES_HMAC_SHA256_CTX *ctx = provctx;

    if (!ossl_prov_is_running())
        return NULL;

    return OPENSSL_memdup(ctx, sizeof(*ctx));
}

static void aes_cbc_hmac_sha256_freectx(void *vctx)
{
    PROV_AES_HMAC_SHA256_CTX *ctx = (PROV_AES_HMAC_SHA256_CTX *)vctx;

    if (ctx != NULL) {
        ossl_cipher_generic_reset_ctx((PROV_CIPHER_CTX *)vctx);
        OPENSSL_clear_free(ctx, sizeof(*ctx));
    }
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

#endif /* AES_CBC_HMAC_SHA_CAPABLE */

/* ossl_aes128cbc_hmac_sha1_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha1, 128, 128, 128, AES_CBC_HMAC_SHA_FLAGS)
/* ossl_aes256cbc_hmac_sha1_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha1, 256, 128, 128, AES_CBC_HMAC_SHA_FLAGS)
/* ossl_aes128cbc_hmac_sha256_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha256, 128, 128, 128, AES_CBC_HMAC_SHA_FLAGS)
/* ossl_aes256cbc_hmac_sha256_functions */
IMPLEMENT_CIPHER(aes, cbc_hmac_sha256, 256, 128, 128, AES_CBC_HMAC_SHA_FLAGS)
