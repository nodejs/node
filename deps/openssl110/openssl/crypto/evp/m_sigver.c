/*
 * Copyright 2006-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include "internal/evp_int.h"
#include "evp_locl.h"

static int do_sigver_init(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
                          const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey,
                          int ver)
{
    if (ctx->pctx == NULL)
        ctx->pctx = EVP_PKEY_CTX_new(pkey, e);
    if (ctx->pctx == NULL)
        return 0;

    if (!(ctx->pctx->pmeth->flags & EVP_PKEY_FLAG_SIGCTX_CUSTOM)) {

        if (type == NULL) {
            int def_nid;
            if (EVP_PKEY_get_default_digest_nid(pkey, &def_nid) > 0)
                type = EVP_get_digestbynid(def_nid);
        }

        if (type == NULL) {
            EVPerr(EVP_F_DO_SIGVER_INIT, EVP_R_NO_DEFAULT_DIGEST);
            return 0;
        }
    }

    if (ver) {
        if (ctx->pctx->pmeth->verifyctx_init) {
            if (ctx->pctx->pmeth->verifyctx_init(ctx->pctx, ctx) <= 0)
                return 0;
            ctx->pctx->operation = EVP_PKEY_OP_VERIFYCTX;
        } else if (EVP_PKEY_verify_init(ctx->pctx) <= 0)
            return 0;
    } else {
        if (ctx->pctx->pmeth->signctx_init) {
            if (ctx->pctx->pmeth->signctx_init(ctx->pctx, ctx) <= 0)
                return 0;
            ctx->pctx->operation = EVP_PKEY_OP_SIGNCTX;
        } else if (EVP_PKEY_sign_init(ctx->pctx) <= 0)
            return 0;
    }
    if (EVP_PKEY_CTX_set_signature_md(ctx->pctx, type) <= 0)
        return 0;
    if (pctx)
        *pctx = ctx->pctx;
    if (ctx->pctx->pmeth->flags & EVP_PKEY_FLAG_SIGCTX_CUSTOM)
        return 1;
    if (!EVP_DigestInit_ex(ctx, type, e))
        return 0;
    return 1;
}

int EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
                       const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
    return do_sigver_init(ctx, pctx, type, e, pkey, 0);
}

int EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
                         const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
    return do_sigver_init(ctx, pctx, type, e, pkey, 1);
}

int EVP_DigestSignFinal(EVP_MD_CTX *ctx, unsigned char *sigret,
                        size_t *siglen)
{
    int sctx = 0, r = 0;
    EVP_PKEY_CTX *pctx = ctx->pctx;
    if (pctx->pmeth->flags & EVP_PKEY_FLAG_SIGCTX_CUSTOM) {
        if (!sigret)
            return pctx->pmeth->signctx(pctx, sigret, siglen, ctx);
        if (ctx->flags & EVP_MD_CTX_FLAG_FINALISE)
            r = pctx->pmeth->signctx(pctx, sigret, siglen, ctx);
        else {
            EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_dup(ctx->pctx);
            if (!dctx)
                return 0;
            r = dctx->pmeth->signctx(dctx, sigret, siglen, ctx);
            EVP_PKEY_CTX_free(dctx);
        }
        return r;
    }
    if (pctx->pmeth->signctx)
        sctx = 1;
    else
        sctx = 0;
    if (sigret) {
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int mdlen = 0;
        if (ctx->flags & EVP_MD_CTX_FLAG_FINALISE) {
            if (sctx)
                r = ctx->pctx->pmeth->signctx(ctx->pctx, sigret, siglen, ctx);
            else
                r = EVP_DigestFinal_ex(ctx, md, &mdlen);
        } else {
            EVP_MD_CTX *tmp_ctx = EVP_MD_CTX_new();
            if (tmp_ctx == NULL || !EVP_MD_CTX_copy_ex(tmp_ctx, ctx))
                return 0;
            if (sctx)
                r = tmp_ctx->pctx->pmeth->signctx(tmp_ctx->pctx,
                                                  sigret, siglen, tmp_ctx);
            else
                r = EVP_DigestFinal_ex(tmp_ctx, md, &mdlen);
            EVP_MD_CTX_free(tmp_ctx);
        }
        if (sctx || !r)
            return r;
        if (EVP_PKEY_sign(ctx->pctx, sigret, siglen, md, mdlen) <= 0)
            return 0;
    } else {
        if (sctx) {
            if (pctx->pmeth->signctx(pctx, sigret, siglen, ctx) <= 0)
                return 0;
        } else {
            int s = EVP_MD_size(ctx->digest);
            if (s < 0 || EVP_PKEY_sign(pctx, sigret, siglen, NULL, s) <= 0)
                return 0;
        }
    }
    return 1;
}

int EVP_DigestVerifyFinal(EVP_MD_CTX *ctx, const unsigned char *sig,
                          size_t siglen)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    int r = 0;
    unsigned int mdlen = 0;
    int vctx = 0;

    if (ctx->pctx->pmeth->verifyctx)
        vctx = 1;
    else
        vctx = 0;
    if (ctx->flags & EVP_MD_CTX_FLAG_FINALISE) {
        if (vctx) {
            r = ctx->pctx->pmeth->verifyctx(ctx->pctx, sig, siglen, ctx);
        } else
            r = EVP_DigestFinal_ex(ctx, md, &mdlen);
    } else {
        EVP_MD_CTX *tmp_ctx = EVP_MD_CTX_new();
        if (tmp_ctx == NULL || !EVP_MD_CTX_copy_ex(tmp_ctx, ctx))
            return -1;
        if (vctx) {
            r = tmp_ctx->pctx->pmeth->verifyctx(tmp_ctx->pctx,
                                                sig, siglen, tmp_ctx);
        } else
            r = EVP_DigestFinal_ex(tmp_ctx, md, &mdlen);
        EVP_MD_CTX_free(tmp_ctx);
    }
    if (vctx || !r)
        return r;
    return EVP_PKEY_verify(ctx->pctx, sig, siglen, md, mdlen);
}
