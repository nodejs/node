/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/core.h>

struct predefined_providers_st {
    const char *name;
    OSSL_provider_init_fn *init;
    unsigned int is_fallback:1;
};

extern const struct predefined_providers_st ossl_predefined_providers[];
