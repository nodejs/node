/**********************************************************************
 *                          md_gost.c                                 *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *       OpenSSL interface to GOST R 34.11-94 hash functions          *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include <string.h>
#include "gost_lcl.h"
#include "gosthash.h"
#include "e_gost_err.h"

/* implementation of GOST 34.11 hash function See gost_md.c*/
static int gost_digest_init(EVP_MD_CTX *ctx);
static int gost_digest_update(EVP_MD_CTX *ctx, const void *data,
                              size_t count);
static int gost_digest_final(EVP_MD_CTX *ctx, unsigned char *md);
static int gost_digest_copy(EVP_MD_CTX *to, const EVP_MD_CTX *from);
static int gost_digest_cleanup(EVP_MD_CTX *ctx);

EVP_MD digest_gost = {
    NID_id_GostR3411_94,
    NID_undef,
    32,
    EVP_MD_FLAG_PKEY_METHOD_SIGNATURE,
    gost_digest_init,
    gost_digest_update,
    gost_digest_final,
    gost_digest_copy,
    gost_digest_cleanup,
    NULL,
    NULL,
    {NID_undef, NID_undef, 0, 0, 0},
    32,
    sizeof(struct ossl_gost_digest_ctx),
    NULL
};

int gost_digest_init(EVP_MD_CTX *ctx)
{
    struct ossl_gost_digest_ctx *c = ctx->md_data;
    memset(&(c->dctx), 0, sizeof(gost_hash_ctx));
    gost_init(&(c->cctx), &GostR3411_94_CryptoProParamSet);
    c->dctx.cipher_ctx = &(c->cctx);
    return 1;
}

int gost_digest_update(EVP_MD_CTX *ctx, const void *data, size_t count)
{
    return hash_block((gost_hash_ctx *) ctx->md_data, data, count);
}

int gost_digest_final(EVP_MD_CTX *ctx, unsigned char *md)
{
    return finish_hash((gost_hash_ctx *) ctx->md_data, md);

}

int gost_digest_copy(EVP_MD_CTX *to, const EVP_MD_CTX *from)
{
    struct ossl_gost_digest_ctx *md_ctx = to->md_data;
    if (to->md_data && from->md_data) {
        memcpy(to->md_data, from->md_data,
               sizeof(struct ossl_gost_digest_ctx));
        md_ctx->dctx.cipher_ctx = &(md_ctx->cctx);
    }
    return 1;
}

int gost_digest_cleanup(EVP_MD_CTX *ctx)
{
    if (ctx->md_data)
        memset(ctx->md_data, 0, sizeof(struct ossl_gost_digest_ctx));
    return 1;
}
