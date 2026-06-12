/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/lms.h"
#include "crypto/lms_sig.h"
#include "crypto/lms_util.h"

/* Refer to SP800-208 Section 4 LM-OTS parameter sets */
static const LM_OTS_PARAMS lm_ots_params[] = {
    { OSSL_LM_OTS_TYPE_SHA256_N32_W1, 32, 1, 265, 7, "SHA256"},
    { OSSL_LM_OTS_TYPE_SHA256_N32_W2, 32, 2, 133, 6, "SHA256"},
    { OSSL_LM_OTS_TYPE_SHA256_N32_W4, 32, 4,  67, 4, "SHA256"},
    { OSSL_LM_OTS_TYPE_SHA256_N32_W8, 32, 8,  34, 0, "SHA256"},
    { OSSL_LM_OTS_TYPE_SHA256_N24_W1, 24, 1, 200, 8, "SHA256-192"},
    { OSSL_LM_OTS_TYPE_SHA256_N24_W2, 24, 2, 101, 6, "SHA256-192"},
    { OSSL_LM_OTS_TYPE_SHA256_N24_W4, 24, 4,  51, 4, "SHA256-192"},
    { OSSL_LM_OTS_TYPE_SHA256_N24_W8, 24, 8,  26, 0, "SHA256-192"},
    { OSSL_LM_OTS_TYPE_SHAKE_N32_W1,  32, 1, 265, 7, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N32_W2,  32, 2, 133, 6, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N32_W4,  32, 4,  67, 4, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N32_W8,  32, 8,  34, 0, "SHAKE-256"},
    /* SHAKE-256/192 - OpenSSL does not support this as a name */
    { OSSL_LM_OTS_TYPE_SHAKE_N24_W1,  24, 1, 200, 8, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N24_W2,  24, 2, 101, 6, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N24_W4,  24, 4,  51, 4, "SHAKE-256"},
    { OSSL_LM_OTS_TYPE_SHAKE_N24_W8,  24, 8,  26, 0, "SHAKE-256"},
    { 0, 0, 0, 0, 0, NULL },
};

/**
 * @brief A getter to convert an |ots_type| into a LM_OTS_PARAMS object.
 *
 * @param ots_type The type such as OSSL_LM_OTS_TYPE_SHA256_N32_W1
 * @returns The LM_OTS_PARAMS object associated with the |ots_type|, or
 *          NULL if |ots_type| is undefined.
 */
const LM_OTS_PARAMS *ossl_lm_ots_params_get(uint32_t ots_type)
{
    const LM_OTS_PARAMS *p;

    for (p = lm_ots_params; p->digestname != NULL; ++p)
        if (p->lm_ots_type == ots_type)
            return p;
    return NULL;
}

/* See RFC 8554 Section 4.4 Checksum */
uint16_t ossl_lm_ots_params_checksum(const LM_OTS_PARAMS *params,
                                     const unsigned char *S)
{
    uint16_t sum = 0;
    uint16_t i;
    /* Largest size is 8 * 32 / 1 = 256 (which doesn't quite fit into 8 bits) */
    uint16_t bytes = (8 * params->n / params->w);
    uint16_t end = (1 << params->w) - 1;

    for (i = 0; i < bytes; ++i)
        sum += end - lms_ots_coef(S, i, params->w);
    return (sum << params->ls);
}
