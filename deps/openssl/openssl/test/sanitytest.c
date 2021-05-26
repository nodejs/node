/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include "testutil.h"
#include "internal/numbers.h"

static int test_sanity_null_zero(void)
{
    char *p;
    char bytes[sizeof(p)];

    /* Is NULL equivalent to all-bytes-zero? */
    p = NULL;
    memset(bytes, 0, sizeof(bytes));
    return TEST_mem_eq(&p, sizeof(p), bytes, sizeof(bytes));
}

static int test_sanity_enum_size(void)
{
    enum smallchoices { sa, sb, sc };
    enum medchoices { ma, mb, mc, md, me, mf, mg, mh, mi, mj, mk, ml };
    enum largechoices {
        a01, b01, c01, d01, e01, f01, g01, h01, i01, j01,
        a02, b02, c02, d02, e02, f02, g02, h02, i02, j02,
        a03, b03, c03, d03, e03, f03, g03, h03, i03, j03,
        a04, b04, c04, d04, e04, f04, g04, h04, i04, j04,
        a05, b05, c05, d05, e05, f05, g05, h05, i05, j05,
        a06, b06, c06, d06, e06, f06, g06, h06, i06, j06,
        a07, b07, c07, d07, e07, f07, g07, h07, i07, j07,
        a08, b08, c08, d08, e08, f08, g08, h08, i08, j08,
        a09, b09, c09, d09, e09, f09, g09, h09, i09, j09,
        a10, b10, c10, d10, e10, f10, g10, h10, i10, j10,
        xxx };

    /* Enum size */
    if (!TEST_size_t_eq(sizeof(enum smallchoices), sizeof(int))
        || !TEST_size_t_eq(sizeof(enum medchoices), sizeof(int))
        || !TEST_size_t_eq(sizeof(enum largechoices), sizeof(int)))
        return 0;
    return 1;
}

static int test_sanity_twos_complement(void)
{
    /* Basic two's complement checks. */
    if (!TEST_int_eq(~(-1), 0)
        || !TEST_long_eq(~(-1L), 0L))
        return 0;
    return 1;
}

static int test_sanity_sign(void)
{
    /* Check that values with sign bit 1 and value bits 0 are valid */
    if (!TEST_int_eq(-(INT_MIN + 1), INT_MAX)
        || !TEST_long_eq(-(LONG_MIN + 1), LONG_MAX))
        return 0;
    return 1;
}

static int test_sanity_unsigned_conversion(void)
{
    /* Check that unsigned-to-signed conversions preserve bit patterns */
    if (!TEST_int_eq((int)((unsigned int)INT_MAX + 1), INT_MIN)
        || !TEST_long_eq((long)((unsigned long)LONG_MAX + 1), LONG_MIN))
        return 0;
    return 1;
}

static int test_sanity_range(void)
{
    /* This isn't possible to check using the framework functions */
    if (SIZE_MAX < INT_MAX) {
        TEST_error("int must not be wider than size_t");
        return 0;
    }
    return 1;
}

static int test_sanity_memcmp(void)
{
    return CRYPTO_memcmp("ab","cd",2);
}

int setup_tests(void)
{
    ADD_TEST(test_sanity_null_zero);
    ADD_TEST(test_sanity_enum_size);
    ADD_TEST(test_sanity_twos_complement);
    ADD_TEST(test_sanity_sign);
    ADD_TEST(test_sanity_unsigned_conversion);
    ADD_TEST(test_sanity_range);
    ADD_TEST(test_sanity_memcmp);
    return 1;
}

