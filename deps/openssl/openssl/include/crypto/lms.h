/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Internal LMS/LM_OTS functions for other submodules,
 * not for application use
 */

#ifndef OSSL_CRYPTO_LMS_H
# define OSSL_CRYPTO_LMS_H
# pragma once
# ifndef OPENSSL_NO_LMS
#  include "types.h"
#  include <openssl/params.h>

/*
 * Numeric identifiers associated with Leighton-Micali Signatures (LMS)
 * parameter sets are defined in
 * https://www.iana.org/assignments/leighton-micali-signatures/leighton-micali-signatures.xhtml
 * which is referenced from SP800-208.
 */
#  define OSSL_LMS_TYPE_SHA256_N32_H5   0x00000005
#  define OSSL_LMS_TYPE_SHA256_N32_H10  0x00000006
#  define OSSL_LMS_TYPE_SHA256_N32_H15  0x00000007
#  define OSSL_LMS_TYPE_SHA256_N32_H20  0x00000008
#  define OSSL_LMS_TYPE_SHA256_N32_H25  0x00000009
#  define OSSL_LMS_TYPE_SHA256_N24_H5   0x0000000A
#  define OSSL_LMS_TYPE_SHA256_N24_H10  0x0000000B
#  define OSSL_LMS_TYPE_SHA256_N24_H15  0x0000000C
#  define OSSL_LMS_TYPE_SHA256_N24_H20  0x0000000D
#  define OSSL_LMS_TYPE_SHA256_N24_H25  0x0000000E
#  define OSSL_LMS_TYPE_SHAKE_N32_H5    0x0000000F
#  define OSSL_LMS_TYPE_SHAKE_N32_H10   0x00000010
#  define OSSL_LMS_TYPE_SHAKE_N32_H15   0x00000011
#  define OSSL_LMS_TYPE_SHAKE_N32_H20   0x00000012
#  define OSSL_LMS_TYPE_SHAKE_N32_H25   0x00000013
#  define OSSL_LMS_TYPE_SHAKE_N24_H5    0x00000014
#  define OSSL_LMS_TYPE_SHAKE_N24_H10   0x00000015
#  define OSSL_LMS_TYPE_SHAKE_N24_H15   0x00000016
#  define OSSL_LMS_TYPE_SHAKE_N24_H20   0x00000017
#  define OSSL_LMS_TYPE_SHAKE_N24_H25   0x00000018

#  define OSSL_LM_OTS_TYPE_SHA256_N32_W1 0x00000001
#  define OSSL_LM_OTS_TYPE_SHA256_N32_W2 0x00000002
#  define OSSL_LM_OTS_TYPE_SHA256_N32_W4 0x00000003
#  define OSSL_LM_OTS_TYPE_SHA256_N32_W8 0x00000004
#  define OSSL_LM_OTS_TYPE_SHA256_N24_W1 0x00000005
#  define OSSL_LM_OTS_TYPE_SHA256_N24_W2 0x00000006
#  define OSSL_LM_OTS_TYPE_SHA256_N24_W4 0x00000007
#  define OSSL_LM_OTS_TYPE_SHA256_N24_W8 0x00000008
#  define OSSL_LM_OTS_TYPE_SHAKE_N32_W1  0x00000009
#  define OSSL_LM_OTS_TYPE_SHAKE_N32_W2  0x0000000A
#  define OSSL_LM_OTS_TYPE_SHAKE_N32_W4  0x0000000B
#  define OSSL_LM_OTS_TYPE_SHAKE_N32_W8  0x0000000C
#  define OSSL_LM_OTS_TYPE_SHAKE_N24_W1  0x0000000D
#  define OSSL_LM_OTS_TYPE_SHAKE_N24_W2  0x0000000E
#  define OSSL_LM_OTS_TYPE_SHAKE_N24_W4  0x0000000F
#  define OSSL_LM_OTS_TYPE_SHAKE_N24_W8  0x00000010

/* Constants used for verifying */
#  define LMS_SIZE_q 4

/* XDR sizes when encoding and decoding */
#  define LMS_SIZE_I 16
#  define LMS_SIZE_LMS_TYPE 4
#  define LMS_SIZE_OTS_TYPE 4
#  define LMS_MAX_DIGEST_SIZE 32
#  define LMS_MAX_PUBKEY \
    (LMS_SIZE_LMS_TYPE + LMS_SIZE_OTS_TYPE + LMS_SIZE_I + LMS_MAX_DIGEST_SIZE)

/*
 * Refer to RFC 8554 Section 4.1.
 * See also lm_ots_params[]
 */
typedef struct lm_ots_params_st {
    /*
     * The OTS type associates an id with a set of OTS parameters
     * e.g. OSSL_LM_OTS_TYPE_SHAKE_N32_W1
     */
    uint32_t lm_ots_type;
    uint32_t n;              /* Hash output size in bytes (32 or 24) */
    /*
     * The width of the Winternitz coefficients in bits. One of (1, 2, 4, 8)
     * Higher values of w are slower (~2^w computations) but have smaller
     * signatures.
     */
    uint32_t w;
    /*
     * The number of n-byte elements used for an LMOTS signature.
     * One of (265, 133, 67, 34) for n = 32, for w=1,2,4,8
     * One of (200, 101, 51, 26) for n = 24, for w=1,2,4,8
     */
    uint32_t p;
    /*
     * The size of the shift needed to move the checksum so
     * that it appears in the checksum digits.
     * See RFC 8554 Appendix B.  LM-OTS Parameter Options
     */
    uint32_t ls;
    const char *digestname; /* Hash Name */
} LM_OTS_PARAMS;

/* See lms_params[] */
typedef struct lms_params_st {
    /*
     * The lms type associates an id with a set of parameters to define the
     * Digest and Height of a LMS tree.
     * e.g, OSSL_LMS_TYPE_SHA256_N24_H25
     */
    uint32_t lms_type;
    const char *digestname; /* One of SHA256, SHA256-192, or SHAKE256 */
    uint32_t n; /* The Digest size (either 24 or 32), Useful for setting up SHAKE */
    uint32_t h; /* The height of a LMS tree which is one of 5, 10, 15, 20, 25) */
} LMS_PARAMS;

typedef struct lms_pub_key_st {
    /*
     * A buffer containing an encoded public key of the form
     * u32str(lmstype) || u32str(otstype) || I[16] || K[n]
     */
    unsigned char *encoded;         /* encoded public key data */
    size_t encodedlen;
    /*
     * K is the LMS tree's root public key (Called T(1))
     * It is n bytes long (the hash size).
     * It is a pointer into the encoded buffer
     */
    unsigned char *K;
} LMS_PUB_KEY;

typedef struct lms_key_st {
    const LMS_PARAMS *lms_params;
    const LM_OTS_PARAMS *ots_params;
    OSSL_LIB_CTX *libctx;
    unsigned char *Id;        /* A pointer to 16 bytes (I[16]) */
    LMS_PUB_KEY pub;
} LMS_KEY;

const LMS_PARAMS *ossl_lms_params_get(uint32_t lms_type);
const LM_OTS_PARAMS *ossl_lm_ots_params_get(uint32_t ots_type);

LMS_KEY *ossl_lms_key_new(OSSL_LIB_CTX *libctx);
void ossl_lms_key_free(LMS_KEY *lmskey);
int ossl_lms_key_equal(const LMS_KEY *key1, const LMS_KEY *key2, int selection);
int ossl_lms_key_valid(const LMS_KEY *key, int selection);
int ossl_lms_key_has(const LMS_KEY *key, int selection);

int ossl_lms_pubkey_from_params(const OSSL_PARAM *pub, LMS_KEY *lmskey);
int ossl_lms_pubkey_decode(const unsigned char *pub, size_t publen,
                           LMS_KEY *lmskey);
size_t ossl_lms_pubkey_length(const unsigned char *data, size_t datalen);

# endif /* OPENSSL_NO_LMS */
#endif /* OSSL_CRYPTO_LMS_H */
