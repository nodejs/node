/*
 * Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Known answer tests (KAT) for NIST SP800-90A DRBGs.
 */

#include <stddef.h>

#ifndef OSSL_TEST_DRBG_CAVS_DATA_H
# define OSSL_TEST_DRBG_CAVS_DATA_H

enum drbg_kat_type {
    NO_RESEED,
    PR_FALSE,
    PR_TRUE
};

enum drbg_df {
    USE_DF,
    NO_DF,
    NA
};

struct drbg_kat_no_reseed {
    size_t count;
    const unsigned char *entropyin;
    const unsigned char *nonce;
    const unsigned char *persstr;
    const unsigned char *addin1;
    const unsigned char *addin2;
    const unsigned char *retbytes;
};

struct drbg_kat_pr_false {
    size_t count;
    const unsigned char *entropyin;
    const unsigned char *nonce;
    const unsigned char *persstr;
    const unsigned char *entropyinreseed;
    const unsigned char *addinreseed;
    const unsigned char *addin1;
    const unsigned char *addin2;
    const unsigned char *retbytes;
};

struct drbg_kat_pr_true {
    size_t count;
    const unsigned char *entropyin;
    const unsigned char *nonce;
    const unsigned char *persstr;
    const unsigned char *entropyinpr1;
    const unsigned char *addin1;
    const unsigned char *entropyinpr2;
    const unsigned char *addin2;
    const unsigned char *retbytes;
};

struct drbg_kat {
    enum drbg_kat_type type;
    enum drbg_df df;
    int nid;

    size_t entropyinlen;
    size_t noncelen;
    size_t persstrlen;
    size_t addinlen;
    size_t retbyteslen;

    const void *t;
};

extern const struct drbg_kat *drbg_test[];
extern const size_t drbg_test_nelem;

#endif
