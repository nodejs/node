/*
 * Copyright 2022-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/kdf.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include "internal/common.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/hmac_drbg.h"
#include "prov/provider_ctx.h"
#include "providers/implementations/kdfs/hmacdrbg_kdf.inc"

static OSSL_FUNC_kdf_newctx_fn hmac_drbg_kdf_new;
static OSSL_FUNC_kdf_dupctx_fn hmac_drbg_kdf_dup;
static OSSL_FUNC_kdf_freectx_fn hmac_drbg_kdf_free;
static OSSL_FUNC_kdf_reset_fn hmac_drbg_kdf_reset;
static OSSL_FUNC_kdf_derive_fn hmac_drbg_kdf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn hmac_drbg_kdf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn hmac_drbg_kdf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn hmac_drbg_kdf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn hmac_drbg_kdf_get_ctx_params;

typedef struct {
    PROV_DRBG_HMAC base;
    void *provctx;
    unsigned char *entropy, *nonce;
    size_t entropylen, noncelen;
    int init;
} KDF_HMAC_DRBG;

static void *hmac_drbg_kdf_new(void *provctx)
{
    KDF_HMAC_DRBG *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    ctx->provctx = provctx;
    return ctx;
}

static void hmac_drbg_kdf_reset(void *vctx)
{
    KDF_HMAC_DRBG *ctx = (KDF_HMAC_DRBG *)vctx;
    PROV_DRBG_HMAC *drbg = &ctx->base;
    void *provctx = ctx->provctx;

    EVP_MAC_CTX_free(drbg->ctx);
    ossl_prov_digest_reset(&drbg->digest);
    OPENSSL_clear_free(ctx->entropy, ctx->entropylen);
    OPENSSL_clear_free(ctx->nonce, ctx->noncelen);
    OPENSSL_cleanse(ctx, sizeof(*ctx));
    ctx->provctx = provctx;
}

static void hmac_drbg_kdf_free(void *vctx)
{
    KDF_HMAC_DRBG *ctx = (KDF_HMAC_DRBG *)vctx;

    if (ctx != NULL) {
        hmac_drbg_kdf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static int ossl_drbg_hmac_dup(PROV_DRBG_HMAC *dst, const PROV_DRBG_HMAC *src) {
    if (src->ctx != NULL) {
        dst->ctx = EVP_MAC_CTX_dup(src->ctx);
        if (dst->ctx == NULL)
            return 0;
    }
    if (!ossl_prov_digest_copy(&dst->digest, &src->digest))
        return 0;
    memcpy(dst->K, src->K, sizeof(dst->K));
    memcpy(dst->V, src->V, sizeof(dst->V));
    dst->blocklen = src->blocklen;
    return 1;
}

static void *hmac_drbg_kdf_dup(void *vctx)
{
    const KDF_HMAC_DRBG *src = (const KDF_HMAC_DRBG *)vctx;
    KDF_HMAC_DRBG *dst;

    dst = hmac_drbg_kdf_new(src->provctx);
    if (dst != NULL) {
        if (!ossl_drbg_hmac_dup(&dst->base, &src->base)
                || !ossl_prov_memdup(src->entropy, src->entropylen,
                                     &dst->entropy , &dst->entropylen)
                || !ossl_prov_memdup(src->nonce, src->noncelen,
                                     &dst->nonce, &dst->noncelen))
            goto err;
        dst->init = src->init;
    }
    return dst;

 err:
    hmac_drbg_kdf_free(dst);
    return NULL;
}

static int hmac_drbg_kdf_derive(void *vctx, unsigned char *out, size_t outlen,
                                const OSSL_PARAM params[])
{
    KDF_HMAC_DRBG *ctx = (KDF_HMAC_DRBG *)vctx;
    PROV_DRBG_HMAC *drbg = &ctx->base;

    if (!ossl_prov_is_running()
            || !hmac_drbg_kdf_set_ctx_params(vctx, params))
        return 0;
    if (!ctx->init) {
        if (ctx->entropy == NULL
                || ctx->entropylen == 0
                || ctx->nonce == NULL
                || ctx->noncelen == 0
                || !ossl_drbg_hmac_init(drbg, ctx->entropy, ctx->entropylen,
                                        ctx->nonce, ctx->noncelen, NULL, 0))
            return 0;
        ctx->init = 1;
    }

    return ossl_drbg_hmac_generate(drbg, out, outlen, NULL, 0);
}

static int hmac_drbg_kdf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    KDF_HMAC_DRBG *hmac = (KDF_HMAC_DRBG *)vctx;
    PROV_DRBG_HMAC *drbg = &hmac->base;
    const char *name;
    const EVP_MD *md;
    struct hmac_drbg_kdf_get_ctx_params_st p;

    if (hmac == NULL || !hmac_drbg_kdf_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.mac != NULL) {
        if (drbg->ctx == NULL)
            return 0;
        name = EVP_MAC_get0_name(EVP_MAC_CTX_get0_mac(drbg->ctx));
        if (!OSSL_PARAM_set_utf8_string(p.mac, name))
            return 0;
    }

    if (p.digest != NULL) {
        md = ossl_prov_digest_md(&drbg->digest);
        if (md == NULL
                || !OSSL_PARAM_set_utf8_string(p.digest, EVP_MD_get0_name(md)))
            return 0;
    }
    return 1;
}

static const OSSL_PARAM *hmac_drbg_kdf_gettable_ctx_params(
    ossl_unused void *vctx, ossl_unused void *p_ctx)
{
    return hmac_drbg_kdf_get_ctx_params_list;
}

static int hmac_drbg_kdf_set_ctx_params(void *vctx,
                                        const OSSL_PARAM params[])
{
    KDF_HMAC_DRBG *hmac = (KDF_HMAC_DRBG *)vctx;
    PROV_DRBG_HMAC *drbg;
    OSSL_LIB_CTX *libctx;
    const EVP_MD *md;
    struct hmac_drbg_kdf_set_ctx_params_st p;
    void *ptr = NULL;
    size_t size = 0;
    int md_size;

    if (hmac == NULL || !hmac_drbg_kdf_set_ctx_params_decoder(params, &p))
        return 0;

    drbg = &hmac->base;
    libctx = PROV_LIBCTX_OF(hmac->provctx);

    if (p.ent != NULL) {
        if (!OSSL_PARAM_get_octet_string(p.ent, &ptr, 0, &size))
            return 0;
        OPENSSL_free(hmac->entropy);
        hmac->entropy = ptr;
        hmac->entropylen = size;
        hmac->init = 0;
        ptr = NULL;
    }

    if (p.nonce != NULL) {
        if (!OSSL_PARAM_get_octet_string(p.nonce, &ptr, 0, &size))
            return 0;
        OPENSSL_free(hmac->nonce);
        hmac->nonce = ptr;
        hmac->noncelen = size;
        hmac->init = 0;
    }

    if (p.digest != NULL) {
        if (!ossl_prov_digest_load(&drbg->digest, p.digest,
                                   p.propq, p.engine, libctx))
            return 0;

        /* Confirm digest is allowed. Allow all digests that are not XOF */
        md = ossl_prov_digest_md(&drbg->digest);
        if (md != NULL) {
            if (EVP_MD_xof(md)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_XOF_DIGESTS_NOT_ALLOWED);
                return 0;
            }
            md_size = EVP_MD_get_size(md);
            if (md_size <= 0)
                return 0;
            drbg->blocklen = (size_t)md_size;
        }
        if (!ossl_prov_macctx_load(&drbg->ctx, NULL, NULL, p.digest, p.propq,
                                   p.engine, "HMAC", NULL, NULL, libctx))
            return 0;
    }
    return 1;
}

static const OSSL_PARAM *hmac_drbg_kdf_settable_ctx_params(
    ossl_unused void *vctx, ossl_unused void *p_ctx)
{
    return hmac_drbg_kdf_set_ctx_params_list;
}

const OSSL_DISPATCH ossl_kdf_hmac_drbg_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void(*)(void))hmac_drbg_kdf_new },
    { OSSL_FUNC_KDF_FREECTX, (void(*)(void))hmac_drbg_kdf_free },
    { OSSL_FUNC_KDF_DUPCTX, (void(*)(void))hmac_drbg_kdf_dup },
    { OSSL_FUNC_KDF_RESET, (void(*)(void))hmac_drbg_kdf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void(*)(void))hmac_drbg_kdf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
      (void(*)(void))hmac_drbg_kdf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS,
      (void(*)(void))hmac_drbg_kdf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
      (void(*)(void))hmac_drbg_kdf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS,
      (void(*)(void))hmac_drbg_kdf_get_ctx_params },
    OSSL_DISPATCH_END
};
