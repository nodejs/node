/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/lms.h"

/* Refer to SP800-208 Section 4 LMS Parameter Sets */
static const LMS_PARAMS lms_params[] = {
    { OSSL_LMS_TYPE_SHA256_N32_H5, "SHA256", 32, 5, 128 },
    { OSSL_LMS_TYPE_SHA256_N32_H10, "SHA256", 32, 10, 128 },
    { OSSL_LMS_TYPE_SHA256_N32_H15, "SHA256", 32, 15, 128 },
    { OSSL_LMS_TYPE_SHA256_N32_H20, "SHA256", 32, 20, 128 },
    { OSSL_LMS_TYPE_SHA256_N32_H25, "SHA256", 32, 25, 128 },
    { OSSL_LMS_TYPE_SHA256_N24_H5, "SHA256-192", 24, 5, 96 },
    { OSSL_LMS_TYPE_SHA256_N24_H10, "SHA256-192", 24, 10, 96 },
    { OSSL_LMS_TYPE_SHA256_N24_H15, "SHA256-192", 24, 15, 96 },
    { OSSL_LMS_TYPE_SHA256_N24_H20, "SHA256-192", 24, 20, 96 },
    { OSSL_LMS_TYPE_SHA256_N24_H25, "SHA256-192", 24, 25, 96 },
    { OSSL_LMS_TYPE_SHAKE_N32_H5, "SHAKE-256", 32, 5, 256 },
    { OSSL_LMS_TYPE_SHAKE_N32_H10, "SHAKE-256", 32, 10, 256 },
    { OSSL_LMS_TYPE_SHAKE_N32_H15, "SHAKE-256", 32, 15, 256 },
    { OSSL_LMS_TYPE_SHAKE_N32_H20, "SHAKE-256", 32, 20, 256 },
    { OSSL_LMS_TYPE_SHAKE_N32_H25, "SHAKE-256", 32, 25, 256 },
    /* SHAKE-256/192 */
    { OSSL_LMS_TYPE_SHAKE_N24_H5, "SHAKE-256", 24, 5, 192 },
    { OSSL_LMS_TYPE_SHAKE_N24_H10, "SHAKE-256", 24, 10, 192 },
    { OSSL_LMS_TYPE_SHAKE_N24_H15, "SHAKE-256", 24, 15, 192 },
    { OSSL_LMS_TYPE_SHAKE_N24_H20, "SHAKE-256", 24, 20, 192 },
    { OSSL_LMS_TYPE_SHAKE_N24_H25, "SHAKE-256", 24, 25, 192 },

    { 0, NULL, 0, 0 }
};

/**
 * @brief A getter to convert a |lms_type| into a LMS_PARAMS object.
 *
 * @param lms_type The type such as OSSL_LMS_TYPE_SHA256_N32_H5.
 * @returns The LMS_PARAMS object associated with the |lms_type|, or
 *          NULL if |lms_type| is undefined.
 */
const LMS_PARAMS *ossl_lms_params_get(uint32_t lms_type)
{
    const LMS_PARAMS *p;

    for (p = lms_params; p->digestname != NULL; ++p)
        if (p->lms_type == lms_type)
            return p;
    return NULL;
}
