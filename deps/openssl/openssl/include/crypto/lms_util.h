/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* @brief Internal LMS helper functions */

#include "internal/packet.h"
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * This LMS implementation assumes that the hash algorithm must be the same for
 * LMS params and OTS params. Since OpenSSL does not have a "SHAKE256-192"
 * algorithm, we have to check the digest size as well as the name.
 * This macro can be used to compare 2 LMS_PARAMS, LMS_PARAMS and LM_OTS_PARAMS.
 */
#define HASH_NOT_MATCHED(a, b) \
    (a)->n != (b)->n || (strcmp((a)->digestname, (b)->digestname) != 0)

/*
 * See RFC 8554 Section 3.1.3: Strings of w-bit Elements
 * w: Is one of {1,2,4,8}
 */
static ossl_unused ossl_inline
uint8_t lms_ots_coef(const unsigned char *S, uint16_t i, uint8_t w)
{
    uint8_t bitmask = (1 << w) - 1;
    uint8_t shift = 8 - (w * (i % (8 / w)) + w);
    int id = (i * w) / 8;

    return (S[id] >> shift) & bitmask;
}

static ossl_unused ossl_inline
int lms_evp_md_ctx_init(EVP_MD_CTX *ctx, const EVP_MD *md,
                        const LMS_PARAMS *lms_params)
{
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };
    OSSL_PARAM *p = NULL;

    /* The OpenSSL SHAKE implementation requires the xoflen to be set */
    if (strncmp(lms_params->digestname, "SHAKE", 5) == 0) {
        params[0] = OSSL_PARAM_construct_uint32(OSSL_DIGEST_PARAM_XOFLEN,
                                                (uint32_t *)&lms_params->n);
        p = params;
    }
    return EVP_DigestInit_ex2(ctx, md, p);
}
