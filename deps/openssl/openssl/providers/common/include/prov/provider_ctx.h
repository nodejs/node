/*
 * Copyright 2019-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_PROV_PROVIDER_CTX_H
# define OSSL_PROV_PROVIDER_CTX_H

# include <openssl/types.h>
# include <openssl/crypto.h>
# include <openssl/bio.h>
# include <openssl/core.h>

typedef struct prov_ctx_st {
    const OSSL_CORE_HANDLE *handle;
    OSSL_LIB_CTX *libctx;         /* For all provider modules */
    BIO_METHOD *corebiometh;
} PROV_CTX;

/*
 * To be used anywhere the library context needs to be passed, such as to
 * fetching functions.
 */
# define PROV_LIBCTX_OF(provctx)        \
    ossl_prov_ctx_get0_libctx((provctx))

PROV_CTX *ossl_prov_ctx_new(void);
void ossl_prov_ctx_free(PROV_CTX *ctx);
void ossl_prov_ctx_set0_libctx(PROV_CTX *ctx, OSSL_LIB_CTX *libctx);
void ossl_prov_ctx_set0_handle(PROV_CTX *ctx, const OSSL_CORE_HANDLE *handle);
void ossl_prov_ctx_set0_core_bio_method(PROV_CTX *ctx, BIO_METHOD *corebiometh);
OSSL_LIB_CTX *ossl_prov_ctx_get0_libctx(PROV_CTX *ctx);
const OSSL_CORE_HANDLE *ossl_prov_ctx_get0_handle(PROV_CTX *ctx);
BIO_METHOD *ossl_prov_ctx_get0_core_bio_method(PROV_CTX *ctx);

#endif
