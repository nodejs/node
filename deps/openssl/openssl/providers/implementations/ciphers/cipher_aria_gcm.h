/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_ARIA_GCM_H)
#define OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_ARIA_GCM_H

#include "crypto/aria.h"
#include "prov/ciphercommon.h"
#include "prov/ciphercommon_gcm.h"

typedef struct prov_aria_gcm_ctx_st {
    PROV_GCM_CTX base; /* must be first entry in struct */
    union {
        OSSL_UNION_ALIGN;
        ARIA_KEY ks;
    } ks;
} PROV_ARIA_GCM_CTX;

const PROV_GCM_HW *ossl_prov_aria_hw_gcm(size_t keybits);

#endif /* !defined(OSSL_PROVIDERS_IMPLEMENTATIONS_CIPHERS_CIPHER_ARIA_GCM_H) */
