/*
 * Copyright 2016-2025 The OpenSSL Project Authors. All Rights Reserved.
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
#include <openssl/x509_vfy.h>
#include "testutil.h"
#include "internal/nelem.h"
#include "crypto/x509.h"

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
    {"255.255.255.255", "\xff\xff\xff\xff", 4},

    {"::", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16},
    {"::1", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16},
    {"::01", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16},
    {"::0001", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16},
    {"ffff::", "\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16},
    {"ffff::1", "\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", 16},
    {"1::2", "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02", 16},
    {"1:1:1:1:1:1:1:1", "\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01", 16},
    {"2001:db8::ff00:42:8329", "\x20\x01\x0d\xb8\x00\x00\x00\x00\x00\x00\xff\x00\x00\x42\x83\x29", 16},
    {"::1.2.3.4", "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04", 16},
    {"ffff:ffff:ffff:ffff:ffff:ffff:1.2.3.4", "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01\x02\x03\x04", 16},

    {"1:1:1:1:1:1:1:1.test", NULL, 0},
    {":::1", NULL, 0},
    {"2001::123g", NULL, 0},

    /* Too few IPv4 components. */
    {"1", NULL, 0 },
    {"1.", NULL, 0 },
    {"1.2", NULL, 0 },
    {"1.2.", NULL, 0 },
    {"1.2.3", NULL, 0 },
    {"1.2.3.", NULL, 0 },

    /* Invalid embedded IPv4 address. */
    {"::1.2.3", NULL, 0 },

    /* IPv4 literals take the place of two IPv6 components. */
    {"1:2:3:4:5:6:7:1.2.3.4", NULL, 0 },

    /* '::' should have fewer than 16 components or it is redundant. */
    {"1:2:3:4:5:6:7::8", NULL, 0 },

    /* Embedded IPv4 addresses must be at the end. */
    {"::1.2.3.4:1", NULL, 0 },

    /* Too many components. */
    {"1.2.3.4.5", NULL, 0 },
    {"1:2:3:4:5:6:7:8:9", NULL, 0 },
    {"1:2:3:4:5::6:7:8:9", NULL, 0 },

    /* Stray whitespace or other invalid characters. */
    {"1.2.3.4 ", NULL, 0 },
    {"1.2.3 .4", NULL, 0 },
    {"1.2.3. 4", NULL, 0 },
    {" 1.2.3.4", NULL, 0 },
    {"1.2.3.4.", NULL, 0 },
    {"1.2.3.+4", NULL, 0 },
    {"1.2.3.-4", NULL, 0 },
    {"1.2.3.4.example.test", NULL, 0 },
    {"::1 ", NULL, 0 },
    {" ::1", NULL, 0 },
    {":: 1", NULL, 0 },
    {": :1", NULL, 0 },
    {"1.2.3.nope", NULL, 0 },
    {"::nope", NULL, 0 },

    /* Components too large. */
    {"1.2.3.256", NULL, 0},  /* Overflows when adding */
    {"1.2.3.260", NULL, 0},  /* Overflows when multiplying by 10 */
    {"1.2.3.999999999999999999999999999999999999999999", NULL, 0 },
    {"::fffff", NULL, 0 },

    /* Although not an overflow, more than four hex digits is an error. */
    {"::00000", NULL, 0 },

    /* Too many colons. */
    {":::", NULL, 0 },
    {"1:::", NULL, 0 },
    {":::2", NULL, 0 },
    {"1:::2", NULL, 0 },

    /* Only one group of zeros may be elided. */
    {"1::2::3", NULL, 0 },

    /* We only support decimal. */
    {"1.2.3.01", NULL, 0 },
    {"1.2.3.0x1", NULL, 0 },

    /* Random garbage. */
    {"example.test", NULL, 0 },
    {"", NULL, 0},
    {" 1.2.3.4", NULL, 0},
    {" 1.2.3.4 ", NULL, 0},
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

static int ck_purp(ossl_unused const X509_PURPOSE *purpose,
                   ossl_unused const X509 *x, int ca)
{
    return 1;
}

static int tests_X509_PURPOSE(void)
{
    OSSL_LIB_CTX *libctx = NULL;
    int id, idx, *p;
    X509_PURPOSE *xp;

#undef LN
#define LN "LN_test"
#undef SN
#define SN "SN_test"
#undef ARGS
#define ARGS(id, sn) id, X509_TRUST_MAX, 0, ck_purp, LN, sn, NULL
    return TEST_int_gt((id = X509_PURPOSE_get_unused_id(libctx)), X509_PURPOSE_MAX)
        && TEST_int_eq(X509_PURPOSE_get_count() + 1, id)
        && TEST_int_eq(X509_PURPOSE_get_by_id(id), -1)
        && TEST_int_eq(X509_PURPOSE_get_by_sname(SN), -1)

        /* add new entry with fresh id and fresh sname: */
        && TEST_int_eq(X509_PURPOSE_add(ARGS(id, SN)), 1)
        && TEST_int_ne((idx = X509_PURPOSE_get_by_sname(SN)), -1)
        && TEST_int_eq(X509_PURPOSE_get_by_id(id), idx)

        /* overwrite same entry, should be idempotent: */
        && TEST_int_eq(X509_PURPOSE_add(ARGS(id, SN)), 1)
        && TEST_int_eq(X509_PURPOSE_get_by_sname(SN), idx)
        && TEST_int_eq(X509_PURPOSE_get_by_id(id), idx)

        /* fail adding entry with same sname but existing conflicting id: */
        && TEST_int_eq(X509_PURPOSE_add(ARGS(X509_PURPOSE_MAX, SN)), 0)
        /* fail adding entry with same existing id but conflicting sname: */
        && TEST_int_eq(X509_PURPOSE_add(ARGS(id, SN"_different")), 0)

        && TEST_ptr((xp = X509_PURPOSE_get0(idx)))
        && TEST_int_eq(X509_PURPOSE_get_id(xp), id)
        && TEST_str_eq(X509_PURPOSE_get0_name(xp), LN)
        && TEST_str_eq(X509_PURPOSE_get0_sname(xp), SN)
        && TEST_int_eq(X509_PURPOSE_get_trust(xp), X509_TRUST_MAX)

        && TEST_int_eq(*(p = &xp->purpose), id)
        && TEST_int_eq(X509_PURPOSE_set(p, X509_PURPOSE_DEFAULT_ANY), 1)
        && TEST_int_eq(X509_PURPOSE_get_id(xp), X509_PURPOSE_DEFAULT_ANY);
}

/* 0000-01-01 00:00:00 UTC */
#define MIN_CERT_TIME INT64_C(-62167219200)
/* 9999-12-31 23:59:59 UTC */
#define MAX_CERT_TIME INT64_C(253402300799)
/* 1950-01-01 00:00:00 UTC */
#define MIN_UTC_TIME INT64_C(-631152000)
/* 2049-12-31 23:59:59 UTC */
#define MAX_UTC_TIME INT64_C(2524607999)

typedef struct {
    int64_t NotBefore;
    int64_t NotAfter;
} CERT_TEST_DATA;

/* clang-format off */
static CERT_TEST_DATA cert_test_data[] = {
    { 0, 0 },
    { 0, 1 },
    { 1, 1 },
    { -1, 0 },
    { -1, 1 },
    { 1442939232, 1443004020 },
    { 0, INT32_MAX },
    { INT32_MIN, 0 },
    { INT32_MIN, INT32_MAX },
    { 0, UINT32_MAX },
    { MIN_UTC_TIME, 0 },
    { MIN_UTC_TIME - 1, 0 },
    { 0, MAX_UTC_TIME },
    { 0, MAX_UTC_TIME + 1 },
    { MIN_UTC_TIME, MAX_UTC_TIME},
    { MIN_UTC_TIME - 1, MAX_UTC_TIME + 1 },
    { MIN_CERT_TIME,  MAX_CERT_TIME },
    { MIN_CERT_TIME,  MAX_CERT_TIME - 1 },
    { MIN_CERT_TIME + 1,  MAX_CERT_TIME },
    { MIN_CERT_TIME + 1,  MAX_CERT_TIME - 1 },
    { 0,  MAX_CERT_TIME },
    { 0,  MAX_CERT_TIME - 1 }
};
/* clang-format on */

/* Returns 0 for success, 1 if failed */
static int test_a_time(X509_STORE_CTX *ctx, X509 *x509,
                       X509_CRL *crl,
                       const int64_t test_time,
                       int64_t notBefore, int64_t notAfter,
                       const char *file, const int line)
{
    int expected_value, expected_crl_value, error, expected_error;
    X509_VERIFY_PARAM *vpm;

    expected_value = notBefore <= test_time;
    if (expected_value)
        expected_value = notAfter == MAX_CERT_TIME || notAfter >= test_time;

    expected_crl_value = notBefore <= test_time;
    if (expected_crl_value)
        expected_crl_value = notAfter >= test_time;

    if (notBefore > test_time)
        expected_error = X509_V_ERR_CERT_NOT_YET_VALID;
    else if (notAfter < test_time && notAfter != MAX_CERT_TIME)
        expected_error = X509_V_ERR_CERT_HAS_EXPIRED;
    else
        expected_error = 0;

    vpm = X509_STORE_CTX_get0_param(ctx);
    ossl_x509_verify_param_set_time_posix(vpm, test_time);
    if (ossl_x509_check_cert_time(ctx, x509, 0) != expected_value) {
        TEST_info("%s:%d - ossl_X509_check_cert_time %s unexpectedly when "
                  "verifying notBefore %lld, notAfter %lld at time %lld\n",
                  file, line,
                  expected_value ? "failed" : "succeeded",
                  (long long)notBefore, (long long)notAfter,
                  (long long)test_time);
        return 1;
    }
    if (ossl_x509_check_crl_time(ctx, crl, 0) != expected_crl_value) {
        TEST_info("%s:%d - ossl_X509_check_crl_time %s unexpectedly when "
                  "verifying lastUpdate %lld, nextUpdate %lld at time %lld\n",
                  file, line,
                  expected_value ? "failed" : "succeeded",
                  (long long)notBefore, (long long)notAfter,
                  (long long)test_time);
        return 1;
    }
    error = 0;
    if (ossl_x509_check_certificate_times(vpm, x509, &error) != expected_value) {
        TEST_info("%s:%d - ossl_X509_check_certificate_times %s unexpectedly "
                  "when verifying notBefore %lld, notAfter %lld at time %lld\n",
                  file, line,
                  expected_value ? "failed" : "succeeded",
                  (long long)notBefore, (long long)notAfter,
                  (long long)test_time);
        return 1;
    }
    if (error != expected_error) {
        TEST_info("%s:%d - ossl_X509_check_certificate_times error return was "
                  "%d, expected %d when verifying notBefore %lld, notAfter "
                  "%lld at time %lld\n",
                  file, line,
                  error, expected_error,
                  (long long)notBefore, (long long)notAfter,
                  (long long)test_time);
        return 1;
    }
    return 0;
}

static int do_x509_time_tests(CERT_TEST_DATA *tests, size_t ntests)
{
    int ret = 0;
    int failures = 0;
    X509 *x509 = NULL;
    X509_CRL *crl = NULL;
    X509_STORE_CTX *ctx = NULL;
    X509_VERIFY_PARAM *vpm = NULL;
    ASN1_TIME *nb = NULL, *na = NULL;
    size_t i;

    if (!TEST_ptr(x509 = X509_new())) {
        TEST_info("Malloc failed");
        goto err;
    }
    if (!TEST_ptr(crl = X509_CRL_new())) {
        TEST_info("Malloc failed");
        goto err;
    }
    if (!TEST_ptr(ctx = X509_STORE_CTX_new())) {
        TEST_info("Malloc failed");
        goto err;
    }
    X509_STORE_CTX_init(ctx, NULL, NULL, NULL);
    if (!TEST_ptr(vpm = X509_VERIFY_PARAM_new())) {
        TEST_info("Malloc failed");
        goto err;
    }
    X509_STORE_CTX_set0_param(ctx, vpm);
    if (!TEST_ptr(nb = ASN1_TIME_new())) {
        TEST_info("Malloc failed");
        goto err;
    }
    if (!TEST_ptr(na = ASN1_TIME_new())) {
        TEST_info("Malloc failed");
        goto err;
    }

    for (i = 0; i < ntests; i++) {
        int64_t test_time;

        if (!TEST_true(ossl_posix_to_asn1_time(tests[i].NotBefore, &nb))) {
            TEST_info("Could not create NotBefore for time %lld\n", (long long) tests[i].NotBefore);
            goto err;
        }
        if (!TEST_true(ossl_posix_to_asn1_time(tests[i].NotAfter, &na))) {
            TEST_info("Could not create NotAfter for time %lld\n", (long long) tests[i].NotBefore);
            goto err;
        }

        /* Forcibly jam the times into the X509 */
        if (!TEST_true(X509_set1_notBefore(x509, nb)))
            goto err;

        if (!TEST_true(X509_set1_notAfter(x509, na)))
            TEST_info("X509_set1_notAftere failed");

        /* Forcibly jam the times into the CRL */
        if (!TEST_true(X509_CRL_set1_lastUpdate(crl, nb)))
            goto err;

        if (!TEST_true(X509_CRL_set1_nextUpdate(crl, na)))
            goto err;

        /* Test boundaries of NotBefore */
        test_time = tests[i].NotBefore - 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = tests[i].NotBefore;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = tests[i].NotBefore + 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        /* Test boundaries of NotAfter */
        test_time = tests[i].NotAfter - 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = tests[i].NotAfter;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = tests[i].NotAfter + 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = 1442939232;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = 1443004020;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = MIN_UTC_TIME;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = MIN_UTC_TIME - 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = MAX_UTC_TIME;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = MAX_UTC_TIME + 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        /* Test integer value boundaries */
        test_time = INT64_MIN;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = INT32_MIN;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = -1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = 0;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = 1;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = INT32_MAX;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = UINT32_MAX;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
        test_time = INT64_MAX;
        failures += test_a_time(ctx, x509, crl, test_time, tests[i].NotBefore,
                                tests[i].NotAfter,
                                __FILE__, __LINE__);
    }

    ret = (failures == 0);

err:
    X509_STORE_CTX_free(ctx);
    X509_free(x509);
    X509_CRL_free(crl);
    ASN1_STRING_free(nb);
    ASN1_STRING_free(na);
    return ret;
}

static int tests_X509_check_time(void)
{
    return do_x509_time_tests(cert_test_data, sizeof(cert_test_data)
                              / sizeof(CERT_TEST_DATA));
}

int setup_tests(void)
{
    ADD_TEST(test_standard_exts);
    ADD_ALL_TESTS(test_a2i_ipaddress, OSSL_NELEM(a2i_ipaddress_tests));
    ADD_TEST(tests_X509_PURPOSE);
    ADD_TEST(tests_X509_check_time);
    return 1;
}
