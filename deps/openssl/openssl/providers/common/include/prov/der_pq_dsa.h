/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_COMMON_INCLUDE_PROV_DER_PQ_DSA_H)
#define OSSL_PROVIDERS_COMMON_INCLUDE_PROV_DER_PQ_DSA_H

#include <stddef.h>
#include <stdint.h>

int ossl_der_oid_pq_dsa_prehash_digest(const char *oid_digest_name,
    const uint8_t **oid, size_t *oidlen, size_t *sz);

#endif /* !defined(OSSL_PROVIDERS_COMMON_INCLUDE_PROV_DER_PQ_DSA_H) */
