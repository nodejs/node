/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <internal/numbers.h>


#define TEST(e) \
    do { \
        if (!(e)) { \
            fprintf(stderr, "Failed " #e "\n"); \
            failures++; \
        } \
    } while (0)


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

int main()
{
    char *p;
    char bytes[sizeof(p)];
    int failures = 0;

    /* Is NULL equivalent to all-bytes-zero? */
    p = NULL;
    memset(bytes, 0, sizeof(bytes));
    TEST(memcmp(&p, bytes, sizeof(bytes)) == 0);

    /* Enum size */
    TEST(sizeof(enum smallchoices) == sizeof(int));
    TEST(sizeof(enum medchoices) == sizeof(int));
    TEST(sizeof(enum largechoices) == sizeof(int));
    /* Basic two's complement checks. */
    TEST(~(-1) == 0);
    TEST(~(-1L) == 0L);

    /* Check that values with sign bit 1 and value bits 0 are valid */
    TEST(-(INT_MIN + 1) == INT_MAX);
    TEST(-(LONG_MIN + 1) == LONG_MAX);

    /* Check that unsigned-to-signed conversions preserve bit patterns */
    TEST((int)((unsigned int)INT_MAX + 1) == INT_MIN);
    TEST((long)((unsigned long)LONG_MAX + 1) == LONG_MIN);

    return failures;
}
