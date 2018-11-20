/*
 * Copyright 2017-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Tests for X509 time functions */

#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/x509.h>
#include "testutil.h"
#include "e_os.h"

typedef struct {
    const char *data;
    int type;
    time_t cmp_time;
    /* -1 if asn1_time <= cmp_time, 1 if asn1_time > cmp_time, 0 if error. */
    int expected;
} TESTDATA;

static TESTDATA x509_cmp_tests[] = {
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "20170217180154Z", V_ASN1_GENERALIZEDTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* Same as UTC time. */
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        1487354514, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        1487354515, -1,
    },
    {
        "170217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        1487354513, 1,
    },
    /* UTCTime from the 20th century. */
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* The same in seconds since epoch. */
        919274514, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second more. */
        919274515, -1,
    },
    {
        "990217180154Z", V_ASN1_UTCTIME,
        /* One second less. */
        919274513, 1,
    },
    /* Various invalid formats. */
    {
        /* No trailing Z. */
        "20170217180154", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No trailing Z, UTCTime. */
        "170217180154", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* No seconds. */
        "201702171801Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* No seconds, UTCTime. */
        "1702171801Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Fractional seconds. */
        "20170217180154.001Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Fractional seconds, UTCTime. */
        "170217180154.001Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Timezone offset. */
        "20170217180154+0100", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Timezone offset, UTCTime. */
        "170217180154+0100", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Extra digits. */
        "2017021718015400Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Extra digits, UTCTime. */
        "17021718015400Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Non-digits. */
        "2017021718015aZ", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Non-digits, UTCTime. */
        "17021718015aZ", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Trailing garbage. */
        "20170217180154Zlongtrailinggarbage", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Trailing garbage, UTCTime. */
        "170217180154Zlongtrailinggarbage", V_ASN1_UTCTIME, 0, 0,
    },
    {
         /* Swapped type. */
        "20170217180154Z", V_ASN1_UTCTIME, 0, 0,
    },
    {
        /* Swapped type. */
        "170217180154Z", V_ASN1_GENERALIZEDTIME, 0, 0,
    },
    {
        /* Bad type. */
        "20170217180154Z", V_ASN1_OCTET_STRING, 0, 0,
    },
};

static int test_x509_cmp_time(int idx)
{
    ASN1_TIME t;
    int result;

    memset(&t, 0, sizeof(t));
    t.type = x509_cmp_tests[idx].type;
    t.data = (unsigned char*)(x509_cmp_tests[idx].data);
    t.length = strlen(x509_cmp_tests[idx].data);

    result = X509_cmp_time(&t, &x509_cmp_tests[idx].cmp_time);
    if (result != x509_cmp_tests[idx].expected) {
        fprintf(stderr, "test_x509_cmp_time(%d) failed: expected %d, got %d\n",
                idx, x509_cmp_tests[idx].expected, result);
        return 0;
    }
    return 1;
}

static int test_x509_cmp_time_current()
{
    time_t now = time(NULL);
    /* Pick a day earlier and later, relative to any system clock. */
    ASN1_TIME *asn1_before = NULL, *asn1_after = NULL;
    int cmp_result, failed = 0;

    asn1_before = ASN1_TIME_adj(NULL, now, -1, 0);
    asn1_after = ASN1_TIME_adj(NULL, now, 1, 0);

    cmp_result  = X509_cmp_time(asn1_before, NULL);
    if (cmp_result != -1) {
        fprintf(stderr, "test_x509_cmp_time_current failed: expected -1, got %d\n",
                cmp_result);
        failed = 1;
    }

    cmp_result = X509_cmp_time(asn1_after, NULL);
    if (cmp_result != 1) {
        fprintf(stderr, "test_x509_cmp_time_current failed: expected 1, got %d\n",
                cmp_result);
        failed = 1;
    }

    ASN1_TIME_free(asn1_before);
    ASN1_TIME_free(asn1_after);

    return failed == 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    unsigned int idx;

    if (!test_x509_cmp_time_current())
        ret = 1;

    for (idx=0 ; idx < OSSL_NELEM(x509_cmp_tests) ; ++idx) {
        if (!test_x509_cmp_time(idx))
            ret = 1;
    }

    if (ret == 0)
        printf("PASS\n");
    return ret;
}
