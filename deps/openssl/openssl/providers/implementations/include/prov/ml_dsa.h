/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_INCLUDE_PROV_ML_DSA_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_INCLUDE_PROV_ML_DSA_H

#include "crypto/ml_dsa.h"
#include "prov/provider_ctx.h"

ML_DSA_KEY *
ossl_prov_ml_dsa_new(PROV_CTX *provctx, const char *propq, int evp_type);

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_INCLUDE_PROV_ML_DSA_H) */
