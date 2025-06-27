/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_PROVIDER_H
# define OPENSSL_PROVIDER_H
# pragma once

# include <openssl/core.h>

# ifdef __cplusplus
extern "C" {
# endif

/* Set the default provider search path */
int OSSL_PROVIDER_set_default_search_path(OSSL_LIB_CTX *, const char *path);

/* Load and unload a provider */
OSSL_PROVIDER *OSSL_PROVIDER_load(OSSL_LIB_CTX *, const char *name);
OSSL_PROVIDER *OSSL_PROVIDER_try_load(OSSL_LIB_CTX *, const char *name,
                                      int retain_fallbacks);
int OSSL_PROVIDER_unload(OSSL_PROVIDER *prov);
int OSSL_PROVIDER_available(OSSL_LIB_CTX *, const char *name);
int OSSL_PROVIDER_do_all(OSSL_LIB_CTX *ctx,
                         int (*cb)(OSSL_PROVIDER *provider, void *cbdata),
                         void *cbdata);

const OSSL_PARAM *OSSL_PROVIDER_gettable_params(const OSSL_PROVIDER *prov);
int OSSL_PROVIDER_get_params(const OSSL_PROVIDER *prov, OSSL_PARAM params[]);
int OSSL_PROVIDER_self_test(const OSSL_PROVIDER *prov);
int OSSL_PROVIDER_get_capabilities(const OSSL_PROVIDER *prov,
                                   const char *capability,
                                   OSSL_CALLBACK *cb,
                                   void *arg);

const OSSL_ALGORITHM *OSSL_PROVIDER_query_operation(const OSSL_PROVIDER *prov,
                                                    int operation_id,
                                                    int *no_cache);
void OSSL_PROVIDER_unquery_operation(const OSSL_PROVIDER *prov,
                                     int operation_id, const OSSL_ALGORITHM *algs);
void *OSSL_PROVIDER_get0_provider_ctx(const OSSL_PROVIDER *prov);
const OSSL_DISPATCH *OSSL_PROVIDER_get0_dispatch(const OSSL_PROVIDER *prov);

/* Add a built in providers */
int OSSL_PROVIDER_add_builtin(OSSL_LIB_CTX *, const char *name,
                              OSSL_provider_init_fn *init_fn);

/* Information */
const char *OSSL_PROVIDER_get0_name(const OSSL_PROVIDER *prov);

# ifdef __cplusplus
}
# endif

#endif
