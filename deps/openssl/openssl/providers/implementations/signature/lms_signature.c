/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
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

static OSSL_FUNC_signature_newctx_fn lms_newctx;
static OSSL_FUNC_signature_freectx_fn lms_freectx;
static OSSL_FUNC_signature_verify_message_init_fn lms_verify_msg_init;
static OSSL_FUNC_signature_verify_fn lms_verify;

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

const OSSL_DISPATCH ossl_lms_signature_functions[] = {
    { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))lms_newctx },
    { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))lms_freectx },
    { OSSL_FUNC_SIGNATURE_VERIFY_MESSAGE_INIT,
      (void (*)(void))lms_verify_msg_init },
    { OSSL_FUNC_SIGNATURE_VERIFY, (void (*)(void))lms_verify },
    OSSL_DISPATCH_END
};
