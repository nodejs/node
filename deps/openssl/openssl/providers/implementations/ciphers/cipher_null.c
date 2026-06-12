/*
 * Copyright 2020-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/core_dispatch.h>
#include <openssl/proverr.h>
#include "prov/implementations.h"
#include "prov/ciphercommon.h"
#include "prov/providercommon.h"
#include "providers/implementations/ciphers/cipher_null.inc"

typedef struct prov_cipher_null_ctx_st {
    int enc;
    size_t tlsmacsize;
    const unsigned char *tlsmac;
} PROV_CIPHER_NULL_CTX;

static OSSL_FUNC_cipher_newctx_fn null_newctx;
static void *null_newctx(void *provctx)
{
    if (!ossl_prov_is_running())
        return NULL;

    return OPENSSL_zalloc(sizeof(PROV_CIPHER_NULL_CTX));
}

static OSSL_FUNC_cipher_freectx_fn null_freectx;
static void null_freectx(void *vctx)
{
    OPENSSL_free(vctx);
}

static OSSL_FUNC_cipher_encrypt_init_fn null_einit;
static int null_einit(void *vctx, const unsigned char *key, size_t keylen,
                      const unsigned char *iv, size_t ivlen,
                      const OSSL_PARAM params[])
{
    PROV_CIPHER_NULL_CTX *ctx = (PROV_CIPHER_NULL_CTX *)vctx;

    if (!ossl_prov_is_running())
        return 0;

    ctx->enc = 1;
    return 1;
}

static OSSL_FUNC_cipher_decrypt_init_fn null_dinit;
static int null_dinit(void *vctx, const unsigned char *key, size_t keylen,
                      const unsigned char *iv, size_t ivlen,
                      const OSSL_PARAM params[])
{
    if (!ossl_prov_is_running())
        return 0;

    return 1;
}

static OSSL_FUNC_cipher_cipher_fn null_cipher;
static int null_cipher(void *vctx, unsigned char *out, size_t *outl,
                       size_t outsize, const unsigned char *in, size_t inl)
{
    PROV_CIPHER_NULL_CTX *ctx = (PROV_CIPHER_NULL_CTX *)vctx;

    if (!ossl_prov_is_running())
        return 0;

    if (!ctx->enc && ctx->tlsmacsize > 0) {
        /*
         * TLS NULL cipher as per:
         * https://tools.ietf.org/html/rfc5246#section-6.2.3.1
         */
        if (inl < ctx->tlsmacsize)
            return 0;
        ctx->tlsmac = in + inl - ctx->tlsmacsize;
        inl -= ctx->tlsmacsize;
    }
    if (outsize < inl)
        return 0;
    if (out != NULL && in != out)
        memcpy(out, in, inl);
    *outl = inl;
    return 1;
}

static OSSL_FUNC_cipher_final_fn null_final;
static int null_final(void *vctx, unsigned char *out, size_t *outl,
                      size_t outsize)
{
    if (!ossl_prov_is_running())
        return 0;

    *outl = 0;
    return 1;
}

static OSSL_FUNC_cipher_get_params_fn null_get_params;
static int null_get_params(OSSL_PARAM params[])
{
    return ossl_cipher_generic_get_params(params, 0, 0, 0, 8, 0);
}

static OSSL_FUNC_cipher_gettable_ctx_params_fn null_gettable_ctx_params;
static const OSSL_PARAM *null_gettable_ctx_params(ossl_unused void *cctx,
                                                  ossl_unused void *provctx)
{
    return null_get_ctx_params_list;
}

static OSSL_FUNC_cipher_get_ctx_params_fn null_get_ctx_params;
static int null_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    PROV_CIPHER_NULL_CTX *ctx = (PROV_CIPHER_NULL_CTX *)vctx;
    struct null_get_ctx_params_st p;

    if (ctx == NULL || !null_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.ivlen != NULL && !OSSL_PARAM_set_size_t(p.ivlen, 0)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.keylen != NULL && !OSSL_PARAM_set_size_t(p.keylen, 0)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }

    if (p.mac != NULL
        && !OSSL_PARAM_set_octet_ptr(p.mac, ctx->tlsmac, ctx->tlsmacsize)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
        return 0;
    }
    return 1;
}

static OSSL_FUNC_cipher_settable_ctx_params_fn null_settable_ctx_params;
static const OSSL_PARAM *null_settable_ctx_params(ossl_unused void *cctx,
                                                  ossl_unused void *provctx)
{
    return null_set_ctx_params_list;
}

static OSSL_FUNC_cipher_set_ctx_params_fn null_set_ctx_params;
static int null_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_CIPHER_NULL_CTX *ctx = (PROV_CIPHER_NULL_CTX *)vctx;
    struct null_set_ctx_params_st p;

    if (ctx == NULL || !null_set_ctx_params_decoder(params, &p))
        return 0;

    if (p.macsize != NULL
            && !OSSL_PARAM_get_size_t(p.macsize, &ctx->tlsmacsize)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }

    return 1;
}

const OSSL_DISPATCH ossl_null_functions[] = {
    { OSSL_FUNC_CIPHER_NEWCTX,
      (void (*)(void)) null_newctx },
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void)) null_freectx },
    { OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void)) null_newctx },
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))null_einit },
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))null_dinit },
    { OSSL_FUNC_CIPHER_UPDATE, (void (*)(void))null_cipher },
    { OSSL_FUNC_CIPHER_FINAL, (void (*)(void))null_final },
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))null_cipher },
    { OSSL_FUNC_CIPHER_GET_PARAMS, (void (*)(void)) null_get_params },
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,
        (void (*)(void))ossl_cipher_generic_gettable_params },
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS, (void (*)(void))null_get_ctx_params },
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,
      (void (*)(void))null_gettable_ctx_params },
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS, (void (*)(void))null_set_ctx_params },
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,
      (void (*)(void))null_settable_ctx_params },
    OSSL_DISPATCH_END
};
