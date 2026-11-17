/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include <openssl/params.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "prov/providercommon.h"
#include "prov/provider_ctx.h"
#include "prov/implementations.h"
#include "crypto/lms_sig.h"
#include "internal/fips.h"
#include "providers/implementations/signature/lms_signature.inc"

static OSSL_FUNC_signature_newctx_fn lms_newctx;
static OSSL_FUNC_signature_freectx_fn lms_freectx;
static OSSL_FUNC_signature_verify_message_init_fn lms_verify_msg_init;
static OSSL_FUNC_signature_verify_fn lms_verify;
static OSSL_FUNC_signature_digest_verify_init_fn lms_digest_verify_init;
static OSSL_FUNC_signature_digest_verify_fn lms_digest_verify;
#ifdef FIPS_MODULE
static OSSL_FUNC_signature_get_ctx_params_fn lms_get_ctx_params;
static OSSL_FUNC_signature_gettable_ctx_params_fn lms_gettable_ctx_params;
#endif

typedef struct {
    OSSL_LIB_CTX *libctx;
    char *propq;
    LMS_KEY *key;
    EVP_MD *md;
} PROV_LMS_CTX;

static void *lms_newctx(void *provctx, const char *propq)
{
    PROV_LMS_CTX *ctx;

    if (!ossl_prov_is_running())
        return NULL;

#ifdef FIPS_MODULE
    if (!ossl_deferred_self_test(PROV_LIBCTX_OF(provctx),
            ST_ID_SIG_LMS))
        return NULL;
#endif

    ctx = OPENSSL_zalloc(sizeof(PROV_LMS_CTX));
    if (ctx == NULL)
        return NULL;

    if (propq != NULL && (ctx->propq = OPENSSL_strdup(propq)) == NULL)
        goto err;
    ctx->libctx = PROV_LIBCTX_OF(provctx);
    return ctx;
err:
    OPENSSL_free(ctx);
    return NULL;
}

static void lms_freectx(void *vctx)
{
    PROV_LMS_CTX *ctx = (PROV_LMS_CTX *)vctx;

    if (ctx == NULL)
        return;
    OPENSSL_free(ctx->propq);
    EVP_MD_free(ctx->md);
    OPENSSL_free(ctx);
}

static int setdigest(PROV_LMS_CTX *ctx, const char *digestname)
{
    /*
     * Assume that only one digest can be used by LMS.
     * Set the digest to the one contained in the public key.
     * If the optional digestname passed in by the user is different
     * then return an error.
     */
    LMS_KEY *key = ctx->key;
    const char *pub_digestname = key->ots_params->digestname;

    if (ctx->md != NULL) {
        if (EVP_MD_is_a(ctx->md, pub_digestname))
            goto end;
        EVP_MD_free(ctx->md);
    }
    ctx->md = EVP_MD_fetch(ctx->libctx, pub_digestname, ctx->propq);
    if (ctx->md == NULL)
        return 0;
end:
    return digestname == NULL || EVP_MD_is_a(ctx->md, digestname);
}

static int lms_verify_msg_init(void *vctx, void *vkey, const OSSL_PARAM params[])
{
    PROV_LMS_CTX *ctx = (PROV_LMS_CTX *)vctx;
    LMS_KEY *key = (LMS_KEY *)vkey;

    if (!ossl_prov_is_running() || ctx == NULL)
        return 0;

    if (key == NULL && ctx->key == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_NO_KEY_SET);
        return 0;
    }
    if (key != NULL)
        ctx->key = key;
    return setdigest(ctx, NULL);
}

static int lms_verify(void *vctx, const unsigned char *sigbuf, size_t sigbuf_len,
    const unsigned char *msg, size_t msglen)
{
    int ret = 0;
    PROV_LMS_CTX *ctx = (PROV_LMS_CTX *)vctx;
    LMS_KEY *pub = ctx->key;
    LMS_SIG *sig = NULL;

    /* A root public key is required to perform a verify operation */
    if (pub == NULL)
        return 0;

    /* Decode the LMS signature data into a LMS_SIG object */
    if (!ossl_lms_sig_decode(&sig, pub, sigbuf, sigbuf_len))
        return 0;

    ret = ossl_lms_sig_verify(sig, pub, ctx->md, msg, msglen);
    ossl_lms_sig_free(sig);
    return ret;
}

static int lms_digest_verify_init(void *vctx, const char *mdname, void *vkey,
    const OSSL_PARAM params[])
{
    PROV_LMS_CTX *ctx = (PROV_LMS_CTX *)vctx;

    if (mdname != NULL && mdname[0] != '\0') {
        ERR_raise_data(ERR_LIB_PROV, PROV_R_INVALID_DIGEST,
            "Explicit digest not supported for LMS operations");
        return 0;
    }
    if (vkey == NULL && ctx->key != NULL)
        return 1; /* lms_set_ctx_params(ctx, params); */

    return lms_verify_msg_init(vctx, vkey, params);
}

static int lms_digest_verify(void *vctx, const uint8_t *sig, size_t siglen,
    const uint8_t *tbs, size_t tbslen)
{
    return lms_verify(vctx, sig, siglen, tbs, tbslen);
}

#ifdef FIPS_MODULE
static const OSSL_PARAM *lms_gettable_ctx_params(ossl_unused void *vctx,
    ossl_unused void *provctx)
{
    return lms_get_ctx_params_list;
}

static int lms_get_ctx_params(ossl_unused void *vctx, OSSL_PARAM params[])
{
    struct lms_get_ctx_params_st p;

    return lms_get_ctx_params_decoder(params, &p);
}
#endif

const OSSL_DISPATCH ossl_lms_signature_functions[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))lms_newctx },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))lms_freectx },
    { OSSL_FUNC_SIGNATURE_VERIFY_MESSAGE_INIT,
        (void (*)(void))lms_verify_msg_init },
    { OSSL_FUNC_SIGNATURE_VERIFY, (void (*)(void))lms_verify },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY_INIT,
        (void (*)(void))lms_digest_verify_init },
    { OSSL_FUNC_SIGNATURE_DIGEST_VERIFY,
        (void (*)(void))lms_digest_verify },
#ifdef FIPS_MODULE
    { OSSL_FUNC_SIGNATURE_GET_CTX_PARAMS, (void (*)(void))lms_get_ctx_params },
    { OSSL_FUNC_SIGNATURE_GETTABLE_CTX_PARAMS,
        (void (*)(void))lms_gettable_ctx_params },
#endif
    OSSL_DISPATCH_END
};
