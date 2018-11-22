/*
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>

#include "testutil.h"
#include "internal/nelem.h"

#ifndef OPENSSL_NO_DSA
static int dsa_cb(int p, int n, BN_GENCB *arg);

/*
 * seed, out_p, out_q, out_g are taken from the updated Appendix 5 to FIPS
 * PUB 186 and also appear in Appendix 5 to FIPS PIB 186-1
 */
static unsigned char seed[20] = {
    0xd5, 0x01, 0x4e, 0x4b, 0x60, 0xef, 0x2b, 0xa8, 0xb6, 0x21, 0x1b, 0x40,
    0x62, 0xba, 0x32, 0x24, 0xe0, 0x42, 0x7d, 0xd3,
};

static unsigned char out_p[] = {
    0x8d, 0xf2, 0xa4, 0x94, 0x49, 0x22, 0x76, 0xaa,
    0x3d, 0x25, 0x75, 0x9b, 0xb0, 0x68, 0x69, 0xcb,
    0xea, 0xc0, 0xd8, 0x3a, 0xfb, 0x8d, 0x0c, 0xf7,
    0xcb, 0xb8, 0x32, 0x4f, 0x0d, 0x78, 0x82, 0xe5,
    0xd0, 0x76, 0x2f, 0xc5, 0xb7, 0x21, 0x0e, 0xaf,
    0xc2, 0xe9, 0xad, 0xac, 0x32, 0xab, 0x7a, 0xac,
    0x49, 0x69, 0x3d, 0xfb, 0xf8, 0x37, 0x24, 0xc2,
    0xec, 0x07, 0x36, 0xee, 0x31, 0xc8, 0x02, 0x91,
};

static unsigned char out_q[] = {
    0xc7, 0x73, 0x21, 0x8c, 0x73, 0x7e, 0xc8, 0xee,
    0x99, 0x3b, 0x4f, 0x2d, 0xed, 0x30, 0xf4, 0x8e,
    0xda, 0xce, 0x91, 0x5f,
};

static unsigned char out_g[] = {
    0x62, 0x6d, 0x02, 0x78, 0x39, 0xea, 0x0a, 0x13,
    0x41, 0x31, 0x63, 0xa5, 0x5b, 0x4c, 0xb5, 0x00,
    0x29, 0x9d, 0x55, 0x22, 0x95, 0x6c, 0xef, 0xcb,
    0x3b, 0xff, 0x10, 0xf3, 0x99, 0xce, 0x2c, 0x2e,
    0x71, 0xcb, 0x9d, 0xe5, 0xfa, 0x24, 0xba, 0xbf,
    0x58, 0xe5, 0xb7, 0x95, 0x21, 0x92, 0x5c, 0x9c,
    0xc4, 0x2e, 0x9f, 0x6f, 0x46, 0x4b, 0x08, 0x8c,
    0xc5, 0x72, 0xaf, 0x53, 0xe6, 0xd7, 0x88, 0x02,
};

static const unsigned char str1[] = "12345678901234567890";

static int dsa_test(void)
{
    BN_GENCB *cb;
    DSA *dsa = NULL;
    int counter, ret = 0, i, j;
    unsigned char buf[256];
    unsigned long h;
    unsigned char sig[256];
    unsigned int siglen;
    const BIGNUM *p = NULL, *q = NULL, *g = NULL;

    if (!TEST_ptr(cb = BN_GENCB_new()))
        goto end;

    BN_GENCB_set(cb, dsa_cb, NULL);
    if (!TEST_ptr(dsa = DSA_new())
        || !TEST_true(DSA_generate_parameters_ex(dsa, 512, seed, 20,
                                                &counter, &h, cb)))
        goto end;

    if (!TEST_int_eq(counter, 105))
        goto end;
    if (!TEST_int_eq(h, 2))
        goto end;

    DSA_get0_pqg(dsa, &p, &q, &g);
    i = BN_bn2bin(q, buf);
    j = sizeof(out_q);
    if (!TEST_int_eq(i, j) || !TEST_mem_eq(buf, i, out_q, i))
        goto end;

    i = BN_bn2bin(p, buf);
    j = sizeof(out_p);
    if (!TEST_int_eq(i, j) || !TEST_mem_eq(buf, i, out_p, i))
        goto end;

    i = BN_bn2bin(g, buf);
    j = sizeof(out_g);
    if (!TEST_int_eq(i, j) || !TEST_mem_eq(buf, i, out_g, i))
        goto end;

    DSA_generate_key(dsa);
    DSA_sign(0, str1, 20, sig, &siglen, dsa);
    if (TEST_true(DSA_verify(0, str1, 20, sig, siglen, dsa)))
        ret = 1;

 end:
    DSA_free(dsa);
    BN_GENCB_free(cb);
    return ret;
}

static int dsa_cb(int p, int n, BN_GENCB *arg)
{
    static int ok = 0, num = 0;

    if (p == 0)
        num++;
    if (p == 2)
        ok++;

    if (!ok && (p == 0) && (num > 1)) {
        TEST_error("dsa_cb error");
        return 0;
    }
    return 1;
}
#endif /* OPENSSL_NO_DSA */

int setup_tests(void)
{
#ifndef OPENSSL_NO_DSA
    ADD_TEST(dsa_test);
#endif
    return 1;
}
