/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core.h>
#include "provider_local.h"

OSSL_provider_init_fn ossl_default_provider_init;
OSSL_provider_init_fn ossl_base_provider_init;
OSSL_provider_init_fn ossl_null_provider_init;
OSSL_provider_init_fn ossl_fips_intern_provider_init;
#ifdef STATIC_LEGACY
OSSL_provider_init_fn ossl_legacy_provider_init;
#endif
const struct predefined_providers_st ossl_predefined_providers[] = {
#ifdef FIPS_MODULE
    { "fips", ossl_fips_intern_provider_init, 1 },
#else
    { "default", ossl_default_provider_init, 1 },
# ifdef STATIC_LEGACY
    { "legacy", ossl_legacy_provider_init, 0 },
# endif
    { "base", ossl_base_provider_init, 0 },
    { "null", ossl_null_provider_init, 0 },
#endif
    { NULL, NULL, 0 }
};
