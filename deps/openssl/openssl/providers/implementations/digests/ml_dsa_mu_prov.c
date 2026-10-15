/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * mu is the value:
 *  mu = SHAKE256(tr || M', 64)
 *
 * where tr is the hash of the public key
 * And M' is one of the following:
 *   (1) Pure: M' = 00 || ctx_len || ctx || in (where in = message)
 *   (2) PreHash: M' = 01 || ctx_len || ctx || OID || in (where in = hashed(msg))
 */

#include "internal/deprecated.h" /* including crypto/sha.h requires this */

#include <string.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include "crypto/ml_dsa.h"
#include "prov/provider_ctx.h"
#include "prov/digestcommon.h"
#include "prov/der_pq_dsa.h"
#include "prov/implementations.h"
#include "internal/common.h"
#include "internal/sha3.h"
#include "providers/implementations/digests/ml_dsa_mu_prov.inc"

#define SHAKE256_SIZE 64
#define SHAKE_FLAGS (PROV_DIGEST_FLAG_ALGID_ABSENT)
#define ML_DSA_MAX_CONTEXT_STRING_LEN 255

typedef struct mu_ctx_st {
    OSSL_LIB_CTX *libctx;
    char *propq;
    EVP_MD_CTX *mdctx;
    EVP_MD *md;
    uint8_t context[ML_DSA_MAX_CONTEXT_STRING_LEN];
    size_t context_len;
    uint8_t tr[SHAKE256_SIZE]; /* Pre-cached public key Hash */
    size_t keylen;
    const uint8_t *oid;
    size_t oid_len;
    size_t digest_len;
    size_t remaining;
} MU_CTX;

static OSSL_FUNC_digest_newctx_fn mu_newctx;
static OSSL_FUNC_digest_freectx_fn mu_freectx;
static OSSL_FUNC_digest_get_params_fn mu_get_params;
static OSSL_FUNC_digest_dupctx_fn mu_dupctx;
static OSSL_FUNC_digest_init_fn mu_init;
static OSSL_FUNC_digest_update_fn mu_update;
static OSSL_FUNC_digest_final_fn mu_final;
static OSSL_FUNC_digest_set_ctx_params_fn mu_set_ctx_params;
static OSSL_FUNC_digest_settable_ctx_params_fn mu_settable_ctx_params;
static OSSL_FUNC_digest_get_ctx_params_fn mu_get_ctx_params;
static OSSL_FUNC_digest_gettable_ctx_params_fn mu_gettable_ctx_params;

static void *mu_newctx(void *provctx)
{
    MU_CTX *ctx;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return NULL;
    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL)
        ctx->libctx = PROV_LIBCTX_OF(provctx);
    return ctx;
}

static void mu_freectx(void *vctx)
{
    MU_CTX *ctx = (MU_CTX *)vctx;

    OPENSSL_free(ctx->propq);
    EVP_MD_free(ctx->md);
    EVP_MD_CTX_free(ctx->mdctx);
    OPENSSL_clear_free(ctx, sizeof(*ctx));
}

static void *mu_dupctx(void *ctx)
{
    MU_CTX *src = (MU_CTX *)ctx;
    MU_CTX *dst = ossl_prov_is_running() ? OPENSSL_malloc(sizeof(*dst)) : NULL;

    if (dst == NULL)
        return NULL;
    *dst = *src;
    dst->mdctx = NULL;
    dst->propq = NULL;
    dst->md = NULL;
    if (src->md != NULL) {
        if (!EVP_MD_up_ref(src->md))
            goto err;
        dst->md = src->md;
    }
    if (src->mdctx != NULL) {
        dst->mdctx = EVP_MD_CTX_new();
        if (dst->mdctx == NULL
            || !EVP_MD_CTX_copy_ex(dst->mdctx, src->mdctx))
            goto err;
    }
    if (src->propq != NULL) {
        dst->propq = OPENSSL_strdup(src->propq);
        if (dst->propq == NULL)
            goto err;
    }
    return dst;
err:
    mu_freectx(dst);
    return NULL;
}

static int mu_init(void *vctx, const OSSL_PARAM params[])
{
    MU_CTX *ctx = (MU_CTX *)vctx;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;

    if (ctx->mdctx != NULL && !EVP_MD_CTX_reset(ctx->mdctx))
        return 0;
    ctx->remaining = ctx->digest_len;
    return mu_set_ctx_params(vctx, params);
}

static int mu_get_params(OSSL_PARAM params[])
{
    return ossl_digest_default_get_params(params, SHA3_BLOCKSIZE(256),
        SHAKE256_SIZE, SHAKE_FLAGS);
}

static const OSSL_PARAM *mu_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return ml_dsa_mu_set_ctx_params_list;
}

static int set_property_query(MU_CTX *ctx, const char *propq)
{
    OPENSSL_free(ctx->propq);
    ctx->propq = NULL;
    if (propq != NULL) {
        ctx->propq = OPENSSL_strdup(propq);
        if (ctx->propq == NULL)
            return 0;
    }
    return 1;
}

static EVP_MD *shake_digest(MU_CTX *ctx)
{
    if (ctx->md == NULL)
        ctx->md = EVP_MD_fetch(ctx->libctx, "SHAKE256", ctx->propq);
    return ctx->md;
}

static int digest_public_key(MU_CTX *ctx, const uint8_t *pub, size_t publen)
{
    int ret;
    EVP_MD *md;
    EVP_MD_CTX *mdctx;

    if (publen != ML_DSA_44_PUB_LEN
        && publen != ML_DSA_65_PUB_LEN
        && publen != ML_DSA_87_PUB_LEN) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
        return 0;
    }

    md = shake_digest(ctx);
    if (md == NULL)
        return 0;
    mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL)
        return 0;
    ret = EVP_DigestInit_ex(mdctx, md, NULL)
        && EVP_DigestUpdate(mdctx, pub, publen)
        && EVP_DigestFinalXOF(mdctx, ctx->tr, sizeof(ctx->tr));
    EVP_MD_CTX_free(mdctx);

    return ret;
}

static int mu_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    MU_CTX *ctx = (MU_CTX *)vctx;
    struct ml_dsa_mu_set_ctx_params_st p;

    if (ctx == NULL || !ml_dsa_mu_set_ctx_params_decoder(params, &p))
        return 0;

    if (p.ctx != NULL) {
        void *vp = ctx->context;

        if (!OSSL_PARAM_get_octet_string(p.ctx, &vp, sizeof(ctx->context),
                &(ctx->context_len))) {
            ctx->context_len = 0;
            return 0;
        }
    }
    if (p.propq != NULL) {
        if (p.propq->data_type != OSSL_PARAM_UTF8_STRING
            || !set_property_query(ctx, p.propq->data))
            return 0;
    }
    if (p.pubkey != NULL) {
        if (p.pubkey->data_type != OSSL_PARAM_OCTET_STRING)
            return 0;
        if (!digest_public_key(ctx, p.pubkey->data, p.pubkey->data_size))
            return 0;
        ctx->keylen = p.pubkey->data_size;
    }
    if (p.digestname != NULL) {
        int ret;

        if (p.digestname->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        ret = ossl_der_oid_pq_dsa_prehash_digest(p.digestname->data,
            &ctx->oid, &ctx->oid_len, &ctx->digest_len);
        if (ret)
            ctx->remaining = ctx->digest_len;
        else
            ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_DIGEST,
                "%s is not supported", p.digestname->data);
        return ret;
    }
    return 1;
}

static const OSSL_PARAM *mu_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return ml_dsa_mu_get_ctx_params_list;
}

static int mu_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    MU_CTX *ctx = (MU_CTX *)vctx;
    struct ml_dsa_mu_get_ctx_params_st p;

    if (ctx == NULL || !ml_dsa_mu_get_ctx_params_decoder(params, &p))
        return 0;

    /* Size is an alias of xoflen */
    if (p.xoflen != NULL || p.size != NULL) {
        size_t xoflen = SHAKE256_SIZE;

        if (p.size != NULL && !OSSL_PARAM_set_size_t(p.size, xoflen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (p.xoflen != NULL && !OSSL_PARAM_set_size_t(p.xoflen, xoflen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
    }
    return 1;
}

static int check_init(MU_CTX *ctx)
{
    if (ctx->mdctx == NULL) {
        EVP_MD *md = shake_digest(ctx);

        if (md == NULL)
            return 0;
        if (ctx->keylen == 0) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_KEY);
            return 0;
        }
        ctx->mdctx = ossl_ml_dsa_mu_init_int(md, ctx->tr, sizeof(ctx->tr), 1,
            ctx->oid_len != 0, ctx->context, ctx->context_len);
        if (ctx->mdctx == NULL)
            return 0;
        if (!ossl_ml_dsa_mu_update(ctx->mdctx, ctx->oid, ctx->oid_len))
            return 0;
    }
    return 1;
}

static int mu_update(void *vctx, const unsigned char *in, size_t inlen)
{
    MU_CTX *ctx = (MU_CTX *)vctx;
    int ret;

    if (ctx->oid_len > 0) {
        /* For the HASH-ML-DSA case we expect the input to be the size of the digest */
        if (inlen > ctx->remaining) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_DATA);
            return 0;
        }
        ctx->remaining -= inlen;
    }
    ret = check_init(ctx)
        && ossl_ml_dsa_mu_update(ctx->mdctx, in, inlen);
    return ret;
}

static int mu_final(void *vctx, uint8_t *out, size_t *outl, size_t outsz)
{
    MU_CTX *ctx = (MU_CTX *)vctx;
    size_t len = SHAKE256_SIZE;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;
    if (out == NULL) {
        if (outl == NULL)
            return 0;
    } else if (outsz > 0) {
        if (outsz < len)
            return 0;

        if (ctx->remaining != 0)
            return 0;
        if (!ossl_ml_dsa_mu_finalize(ctx->mdctx, out, len))
            return 0;
    }
    *outl = len;
    return 1;
}

const OSSL_DISPATCH ossl_ml_dsa_mu_functions[] = {
    { OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))mu_newctx },
    { OSSL_FUNC_DIGEST_INIT, (void (*)(void))mu_init },
    { OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))mu_update },
    { OSSL_FUNC_DIGEST_FINAL, (void (*)(void))mu_final },
    { OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))mu_freectx },
    { OSSL_FUNC_DIGEST_DUPCTX, (void (*)(void))mu_dupctx },
    { OSSL_FUNC_DIGEST_SET_CTX_PARAMS, (void (*)(void))mu_set_ctx_params },
    { OSSL_FUNC_DIGEST_SETTABLE_CTX_PARAMS,
        (void (*)(void))mu_settable_ctx_params },
    { OSSL_FUNC_DIGEST_GET_CTX_PARAMS, (void (*)(void))mu_get_ctx_params },
    { OSSL_FUNC_DIGEST_GETTABLE_CTX_PARAMS,
        (void (*)(void))mu_gettable_ctx_params },
    PROV_DISPATCH_FUNC_DIGEST_GET_PARAMS(mu),
    PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END
