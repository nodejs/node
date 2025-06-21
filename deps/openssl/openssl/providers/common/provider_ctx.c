/*
 * Copyright 2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdlib.h>
#include "prov/provider_ctx.h"
#include "prov/bio.h"

PROV_CTX *ossl_prov_ctx_new(void)
{
    return OPENSSL_zalloc(sizeof(PROV_CTX));
}

void ossl_prov_ctx_free(PROV_CTX *ctx)
{
    OPENSSL_free(ctx);
}

void ossl_prov_ctx_set0_libctx(PROV_CTX *ctx, OSSL_LIB_CTX *libctx)
{
    if (ctx != NULL)
        ctx->libctx = libctx;
}

void ossl_prov_ctx_set0_handle(PROV_CTX *ctx, const OSSL_CORE_HANDLE *handle)
{
    if (ctx != NULL)
        ctx->handle = handle;
}

void ossl_prov_ctx_set0_core_bio_method(PROV_CTX *ctx, BIO_METHOD *corebiometh)
{
    if (ctx != NULL)
        ctx->corebiometh = corebiometh;
}

OSSL_LIB_CTX *ossl_prov_ctx_get0_libctx(PROV_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->libctx;
}

const OSSL_CORE_HANDLE *ossl_prov_ctx_get0_handle(PROV_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->handle;
}

BIO_METHOD *ossl_prov_ctx_get0_core_bio_method(PROV_CTX *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->corebiometh;
}
