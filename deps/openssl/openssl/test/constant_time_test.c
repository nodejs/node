/*
 * Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/constant_time_locl.h"
#include "e_os.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned int CONSTTIME_TRUE = (unsigned)(~0);
static const unsigned int CONSTTIME_FALSE = 0;
static const unsigned char CONSTTIME_TRUE_8 = 0xff;
static const unsigned char CONSTTIME_FALSE_8 = 0;

static int test_binary_op(unsigned int (*op) (unsigned int a, unsigned int b),
                          const char *op_name, unsigned int a, unsigned int b,
                          int is_true)
{
    unsigned c = op(a, b);
    if (is_true && c != CONSTTIME_TRUE) {
        fprintf(stderr, "Test failed for %s(%du, %du): expected %du "
                "(TRUE), got %du\n", op_name, a, b, CONSTTIME_TRUE, c);
        return 1;
    } else if (!is_true && c != CONSTTIME_FALSE) {
        fprintf(stderr, "Test failed for  %s(%du, %du): expected %du "
                "(FALSE), got %du\n", op_name, a, b, CONSTTIME_FALSE, c);
        return 1;
    }
    return 0;
}

static int test_binary_op_8(unsigned
                            char (*op) (unsigned int a, unsigned int b),
                            const char *op_name, unsigned int a,
                            unsigned int b, int is_true)
{
    unsigned char c = op(a, b);
    if (is_true && c != CONSTTIME_TRUE_8) {
        fprintf(stderr, "Test failed for %s(%du, %du): expected %u "
                "(TRUE), got %u\n", op_name, a, b, CONSTTIME_TRUE_8, c);
        return 1;
    } else if (!is_true && c != CONSTTIME_FALSE_8) {
        fprintf(stderr, "Test failed for  %s(%du, %du): expected %u "
                "(FALSE), got %u\n", op_name, a, b, CONSTTIME_FALSE_8, c);
        return 1;
    }
    return 0;
}

static int test_is_zero(unsigned int a)
{
    unsigned int c = constant_time_is_zero(a);
    if (a == 0 && c != CONSTTIME_TRUE) {
        fprintf(stderr, "Test failed for constant_time_is_zero(%du): "
                "expected %du (TRUE), got %du\n", a, CONSTTIME_TRUE, c);
        return 1;
    } else if (a != 0 && c != CONSTTIME_FALSE) {
        fprintf(stderr, "Test failed for constant_time_is_zero(%du): "
                "expected %du (FALSE), got %du\n", a, CONSTTIME_FALSE, c);
        return 1;
    }
    return 0;
}

static int test_is_zero_8(unsigned int a)
{
    unsigned char c = constant_time_is_zero_8(a);
    if (a == 0 && c != CONSTTIME_TRUE_8) {
        fprintf(stderr, "Test failed for constant_time_is_zero(%du): "
                "expected %u (TRUE), got %u\n", a, CONSTTIME_TRUE_8, c);
        return 1;
    } else if (a != 0 && c != CONSTTIME_FALSE) {
        fprintf(stderr, "Test failed for constant_time_is_zero(%du): "
                "expected %u (FALSE), got %u\n", a, CONSTTIME_FALSE_8, c);
        return 1;
    }
    return 0;
}

static int test_select(unsigned int a, unsigned int b)
{
    unsigned int selected = constant_time_select(CONSTTIME_TRUE, a, b);
    if (selected != a) {
        fprintf(stderr, "Test failed for constant_time_select(%du, %du,"
                "%du): expected %du(first value), got %du\n",
                CONSTTIME_TRUE, a, b, a, selected);
        return 1;
    }
    selected = constant_time_select(CONSTTIME_FALSE, a, b);
    if (selected != b) {
        fprintf(stderr, "Test failed for constant_time_select(%du, %du,"
                "%du): expected %du(second value), got %du\n",
                CONSTTIME_FALSE, a, b, b, selected);
        return 1;
    }
    return 0;
}

static int test_select_8(unsigned char a, unsigned char b)
{
    unsigned char selected = constant_time_select_8(CONSTTIME_TRUE_8, a, b);
    if (selected != a) {
        fprintf(stderr, "Test failed for constant_time_select(%u, %u,"
                "%u): expected %u(first value), got %u\n",
                CONSTTIME_TRUE, a, b, a, selected);
        return 1;
    }
    selected = constant_time_select_8(CONSTTIME_FALSE_8, a, b);
    if (selected != b) {
        fprintf(stderr, "Test failed for constant_time_select(%u, %u,"
                "%u): expected %u(second value), got %u\n",
                CONSTTIME_FALSE, a, b, b, selected);
        return 1;
    }
    return 0;
}

static int test_select_int(int a, int b)
{
    int selected = constant_time_select_int(CONSTTIME_TRUE, a, b);
    if (selected != a) {
        fprintf(stderr, "Test failed for constant_time_select(%du, %d,"
                "%d): expected %d(first value), got %d\n",
                CONSTTIME_TRUE, a, b, a, selected);
        return 1;
    }
    selected = constant_time_select_int(CONSTTIME_FALSE, a, b);
    if (selected != b) {
        fprintf(stderr, "Test failed for constant_time_select(%du, %d,"
                "%d): expected %d(second value), got %d\n",
                CONSTTIME_FALSE, a, b, b, selected);
        return 1;
    }
    return 0;
}

static int test_eq_int(int a, int b)
{
    unsigned int equal = constant_time_eq_int(a, b);
    if (a == b && equal != CONSTTIME_TRUE) {
        fprintf(stderr, "Test failed for constant_time_eq_int(%d, %d): "
                "expected %du(TRUE), got %du\n", a, b, CONSTTIME_TRUE, equal);
        return 1;
    } else if (a != b && equal != CONSTTIME_FALSE) {
        fprintf(stderr, "Test failed for constant_time_eq_int(%d, %d): "
                "expected %du(FALSE), got %du\n",
                a, b, CONSTTIME_FALSE, equal);
        return 1;
    }
    return 0;
}

static int test_eq_int_8(int a, int b)
{
    unsigned char equal = constant_time_eq_int_8(a, b);
    if (a == b && equal != CONSTTIME_TRUE_8) {
        fprintf(stderr, "Test failed for constant_time_eq_int_8(%d, %d): "
                "expected %u(TRUE), got %u\n", a, b, CONSTTIME_TRUE_8, equal);
        return 1;
    } else if (a != b && equal != CONSTTIME_FALSE_8) {
        fprintf(stderr, "Test failed for constant_time_eq_int_8(%d, %d): "
                "expected %u(FALSE), got %u\n",
                a, b, CONSTTIME_FALSE_8, equal);
        return 1;
    }
    return 0;
}

static unsigned int test_values[] =
    { 0, 1, 1024, 12345, 32000, UINT_MAX / 2 - 1,
    UINT_MAX / 2, UINT_MAX / 2 + 1, UINT_MAX - 1,
    UINT_MAX
};

static unsigned char test_values_8[] =
    { 0, 1, 2, 20, 32, 127, 128, 129, 255 };

static int signed_test_values[] = { 0, 1, -1, 1024, -1024, 12345, -12345,
    32000, -32000, INT_MAX, INT_MIN, INT_MAX - 1,
    INT_MIN + 1
};

int main(int argc, char *argv[])
{
    unsigned int a, b, i, j;
    int c, d;
    unsigned char e, f;
    int num_failed = 0, num_all = 0;
    fprintf(stdout, "Testing constant time operations...\n");

    for (i = 0; i < OSSL_NELEM(test_values); ++i) {
        a = test_values[i];
        num_failed += test_is_zero(a);
        num_failed += test_is_zero_8(a);
        num_all += 2;
        for (j = 0; j < OSSL_NELEM(test_values); ++j) {
            b = test_values[j];
            num_failed += test_binary_op(&constant_time_lt,
                                         "constant_time_lt", a, b, a < b);
            num_failed += test_binary_op_8(&constant_time_lt_8,
                                           "constant_time_lt_8", a, b, a < b);
            num_failed += test_binary_op(&constant_time_lt,
                                         "constant_time_lt_8", b, a, b < a);
            num_failed += test_binary_op_8(&constant_time_lt_8,
                                           "constant_time_lt_8", b, a, b < a);
            num_failed += test_binary_op(&constant_time_ge,
                                         "constant_time_ge", a, b, a >= b);
            num_failed += test_binary_op_8(&constant_time_ge_8,
                                           "constant_time_ge_8", a, b,
                                           a >= b);
            num_failed +=
                test_binary_op(&constant_time_ge, "constant_time_ge", b, a,
                               b >= a);
            num_failed +=
                test_binary_op_8(&constant_time_ge_8, "constant_time_ge_8", b,
                                 a, b >= a);
            num_failed +=
                test_binary_op(&constant_time_eq, "constant_time_eq", a, b,
                               a == b);
            num_failed +=
                test_binary_op_8(&constant_time_eq_8, "constant_time_eq_8", a,
                                 b, a == b);
            num_failed +=
                test_binary_op(&constant_time_eq, "constant_time_eq", b, a,
                               b == a);
            num_failed +=
                test_binary_op_8(&constant_time_eq_8, "constant_time_eq_8", b,
                                 a, b == a);
            num_failed += test_select(a, b);
            num_all += 13;
        }
    }

    for (i = 0; i < OSSL_NELEM(signed_test_values); ++i) {
        c = signed_test_values[i];
        for (j = 0; j < OSSL_NELEM(signed_test_values); ++j) {
            d = signed_test_values[j];
            num_failed += test_select_int(c, d);
            num_failed += test_eq_int(c, d);
            num_failed += test_eq_int_8(c, d);
            num_all += 3;
        }
    }

    for (i = 0; i < sizeof(test_values_8); ++i) {
        e = test_values_8[i];
        for (j = 0; j < sizeof(test_values_8); ++j) {
            f = test_values_8[j];
            num_failed += test_select_8(e, f);
            num_all += 1;
        }
    }

    if (!num_failed) {
        fprintf(stdout, "success (ran %d tests)\n", num_all);
        return EXIT_SUCCESS;
    } else {
        fprintf(stdout, "%d of %d tests failed!\n", num_failed, num_all);
        return EXIT_FAILURE;
    }
}
