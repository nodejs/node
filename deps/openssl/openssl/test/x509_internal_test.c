/*
 * Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal tests for the x509 and x509v3 modules */

#include <stdio.h>
#include <string.h>

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include "testutil.h"
#include "internal/nelem.h"

/**********************************************************************
 *
 * Test of x509v3
 *
 ***/

#include "../crypto/x509/ext_dat.h"
#include "../crypto/x509/standard_exts.h"

static int test_standard_exts(void)
{
    size_t i;
    int prev = -1, good = 1;
    const X509V3_EXT_METHOD **tmp;

    tmp = standard_exts;
    for (i = 0; i < OSSL_NELEM(standard_exts); i++, tmp++) {
        if ((*tmp)->ext_nid < prev)
            good = 0;
        prev = (*tmp)->ext_nid;

    }
    if (!good) {
        tmp = standard_exts;
        TEST_error("Extensions out of order!");
        for (i = 0; i < STANDARD_EXTENSION_COUNT; i++, tmp++)
            TEST_note("%d : %s", (*tmp)->ext_nid, OBJ_nid2sn((*tmp)->ext_nid));
    }
    return good;
}

typedef struct {
    const char *ipasc;
    const char *data;
    int length;
} IP_TESTDATA;

static IP_TESTDATA a2i_ipaddress_tests[] = {
    {"127.0.0.1", "\x7f\x00\x00\x01", 4},
    {"1.2.3.4", "\x01\x02\x03\x04", 4},
    {"1.2.3.255", "\x01\x02\x03\xff", 4},
    {"1.2.3", NULL, 0},
    {"1.2.3 .4", NULL, 0},

    {"::1", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16},
    {"1:1:1:1:1:1:1:1", "\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01", 16},
    {"2001:db8::ff00:42:8329", "\x20\x01\x0d\xb8\x00\x00\x00\x00\x00\x00\xff\x00\x00\x42\x83\x29", 16},
    {"1:1:1:1:1:1:1:1.test", NULL, 0},
    {":::1", NULL, 0},
    {"2001::123g", NULL, 0},

    {"example.test", NULL, 0},
    {"", NULL, 0},

    {"1.2.3.4 ", "\x01\x02\x03\x04", 4},
    {" 1.2.3.4", "\x01\x02\x03\x04", 4},
    {" 1.2.3.4 ", "\x01\x02\x03\x04", 4},
    {"1.2.3.4.example.test", NULL, 0},
};


static int test_a2i_ipaddress(int idx)
{
    int good = 1;
    ASN1_OCTET_STRING *ip;
    int len = a2i_ipaddress_tests[idx].length;

    ip = a2i_IPADDRESS(a2i_ipaddress_tests[idx].ipasc);
    if (len == 0) {
        if (!TEST_ptr_null(ip)) {
            good = 0;
            TEST_note("'%s' should not be parsed as IP address", a2i_ipaddress_tests[idx].ipasc);
        }
    } else {
        if (!TEST_ptr(ip)
            || !TEST_int_eq(ASN1_STRING_length(ip), len)
            || !TEST_mem_eq(ASN1_STRING_get0_data(ip), len,
                            a2i_ipaddress_tests[idx].data, len)) {
            good = 0;
        }
    }
    ASN1_OCTET_STRING_free(ip);
    return good;
}

int setup_tests(void)
{
    ADD_TEST(test_standard_exts);
    ADD_ALL_TESTS(test_a2i_ipaddress, OSSL_NELEM(a2i_ipaddress_tests));
    return 1;
}
