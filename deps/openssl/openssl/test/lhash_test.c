/*
 * Copyright 2017-2020 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/lhash.h>
#include <openssl/err.h>
#include <openssl/crypto.h>

#include "internal/nelem.h"
#include "testutil.h"

/*
 * The macros below generate unused functions which error out one of the clang
 * builds.  We disable this check here.
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

DEFINE_LHASH_OF(int);

static int int_tests[] = { 65537, 13, 1, 3, -5, 6, 7, 4, -10, -12, -14, 22, 9,
                           -17, 16, 17, -23, 35, 37, 173, 11 };
static const unsigned int n_int_tests = OSSL_NELEM(int_tests);
static short int_found[OSSL_NELEM(int_tests)];
static short int_not_found;

static unsigned long int int_hash(const int *p)
{
    return 3 & *p;      /* To force collisions */
}

static int int_cmp(const int *p, const int *q)
{
    return *p != *q;
}

static int int_find(int n)
{
    unsigned int i;

    for (i = 0; i < n_int_tests; i++)
        if (int_tests[i] == n)
            return i;
    return -1;
}

static void int_doall(int *v)
{
    const int n = int_find(*v);

    if (n < 0)
        int_not_found++;
    else
        int_found[n]++;
}

static void int_doall_arg(int *p, short *f)
{
    const int n = int_find(*p);

    if (n < 0)
        int_not_found++;
    else
        f[n]++;
}

IMPLEMENT_LHASH_DOALL_ARG(int, short);

static int test_int_lhash(void)
{
    static struct {
        int data;
        int null;
    } dels[] = {
        { 65537,    0 },
        { 173,      0 },
        { 999,      1 },
        { 37,       0 },
        { 1,        0 },
        { 34,       1 }
    };
    const unsigned int n_dels = OSSL_NELEM(dels);
    LHASH_OF(int) *h = lh_int_new(&int_hash, &int_cmp);
    unsigned int i;
    int testresult = 0, j, *p;

    if (!TEST_ptr(h))
        goto end;

    /* insert */
    for (i = 0; i < n_int_tests; i++)
        if (!TEST_ptr_null(lh_int_insert(h, int_tests + i))) {
            TEST_info("int insert %d", i);
            goto end;
        }

    /* num_items */
    if (!TEST_int_eq(lh_int_num_items(h), n_int_tests))
        goto end;

    /* retrieve */
    for (i = 0; i < n_int_tests; i++)
        if (!TEST_int_eq(*lh_int_retrieve(h, int_tests + i), int_tests[i])) {
            TEST_info("lhash int retrieve value %d", i);
            goto end;
        }
    for (i = 0; i < n_int_tests; i++)
        if (!TEST_ptr_eq(lh_int_retrieve(h, int_tests + i), int_tests + i)) {
            TEST_info("lhash int retrieve address %d", i);
            goto end;
        }
    j = 1;
    if (!TEST_ptr_eq(lh_int_retrieve(h, &j), int_tests + 2))
        goto end;

    /* replace */
    j = 13;
    if (!TEST_ptr(p = lh_int_insert(h, &j)))
        goto end;
    if (!TEST_ptr_eq(p, int_tests + 1))
        goto end;
    if (!TEST_ptr_eq(lh_int_retrieve(h, int_tests + 1), &j))
        goto end;

    /* do_all */
    memset(int_found, 0, sizeof(int_found));
    int_not_found = 0;
    lh_int_doall(h, &int_doall);
    if (!TEST_int_eq(int_not_found, 0)) {
        TEST_info("lhash int doall encountered a not found condition");
        goto end;
    }
    for (i = 0; i < n_int_tests; i++)
        if (!TEST_int_eq(int_found[i], 1)) {
            TEST_info("lhash int doall %d", i);
            goto end;
        }

    /* do_all_arg */
    memset(int_found, 0, sizeof(int_found));
    int_not_found = 0;
    lh_int_doall_short(h, int_doall_arg, int_found);
    if (!TEST_int_eq(int_not_found, 0)) {
        TEST_info("lhash int doall arg encountered a not found condition");
        goto end;
    }
    for (i = 0; i < n_int_tests; i++)
        if (!TEST_int_eq(int_found[i], 1)) {
            TEST_info("lhash int doall arg %d", i);
            goto end;
        }

    /* delete */
    for (i = 0; i < n_dels; i++) {
        const int b = lh_int_delete(h, &dels[i].data) == NULL;
        if (!TEST_int_eq(b ^ dels[i].null,  0)) {
            TEST_info("lhash int delete %d", i);
            goto end;
        }
    }

    /* error */
    if (!TEST_int_eq(lh_int_error(h), 0))
        goto end;

    testresult = 1;
end:
    lh_int_free(h);
    return testresult;
}

static unsigned long int stress_hash(const int *p)
{
    return *p;
}

static int test_stress(void)
{
    LHASH_OF(int) *h = lh_int_new(&stress_hash, &int_cmp);
    const unsigned int n = 2500000;
    unsigned int i;
    int testresult = 0, *p;

    if (!TEST_ptr(h))
        goto end;

    /* insert */
    for (i = 0; i < n; i++) {
        p = OPENSSL_malloc(sizeof(i));
        if (!TEST_ptr(p)) {
            TEST_info("lhash stress out of memory %d", i);
            goto end;
        }
        *p = 3 * i + 1;
        lh_int_insert(h, p);
    }

    /* num_items */
    if (!TEST_int_eq(lh_int_num_items(h), n))
            goto end;

    TEST_info("hash full statistics:");
    OPENSSL_LH_stats_bio((OPENSSL_LHASH *)h, bio_err);
    TEST_note("hash full node usage:");
    OPENSSL_LH_node_usage_stats_bio((OPENSSL_LHASH *)h, bio_err);

    /* delete in a different order */
    for (i = 0; i < n; i++) {
        const int j = (7 * i + 4) % n * 3 + 1;

        if (!TEST_ptr(p = lh_int_delete(h, &j))) {
            TEST_info("lhash stress delete %d\n", i);
            goto end;
        }
        if (!TEST_int_eq(*p, j)) {
            TEST_info("lhash stress bad value %d", i);
            goto end;
        }
        OPENSSL_free(p);
    }

    TEST_info("hash empty statistics:");
    OPENSSL_LH_stats_bio((OPENSSL_LHASH *)h, bio_err);
    TEST_note("hash empty node usage:");
    OPENSSL_LH_node_usage_stats_bio((OPENSSL_LHASH *)h, bio_err);

    testresult = 1;
end:
    lh_int_free(h);
    return testresult;
}

int setup_tests(void)
{
    ADD_TEST(test_int_lhash);
    ADD_TEST(test_stress);
    return 1;
}
