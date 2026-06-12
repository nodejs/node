/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "lms_common.inc"

typedef struct LMS_ACVP_TEST_DATA_st {
    const unsigned char *pub;
    size_t publen;
    const unsigned char *priv;
    size_t privlen;
    const unsigned char *msg;
    size_t msglen;
    const unsigned char *sig;
    size_t siglen;
} LMS_ACVP_TEST_DATA;

/*
 * The data for HSS with a single level is almost identical
 * to LMS data, except the public key & signature have extra 4 byte headers.
 */
#define LMS_ACVP_ITEM(name) {                \
    name##_pub + 4, sizeof(name##_pub) - 4,  \
    name##_priv, sizeof(name##_priv),        \
    name##_msg, sizeof(name##_msg),          \
    name##_sig + 4, sizeof(name##_sig) - 4 }

/* We can only use the hss tests that have a single level here */
static LMS_ACVP_TEST_DATA lms_testdata[] = {
    LMS_ACVP_ITEM(sha256_192),
    LMS_ACVP_ITEM(shake256_192),
    LMS_ACVP_ITEM(shake256_256)
};
