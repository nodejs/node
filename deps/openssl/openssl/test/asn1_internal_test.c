/*
 * Copyright 1999-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal tests for the asn1 module */

/*
 * RSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/posix_time.h>
#include "testutil.h"
#include "internal/nelem.h"

/**********************************************************************
 *
 * Test of a_strnid's tbl_standard
 *
 ***/

#include "../crypto/asn1/tbl_standard.h"

static int test_tbl_standard(void)
{
    const ASN1_STRING_TABLE *tmp;
    int last_nid = -1;
    size_t i;

    for (tmp = tbl_standard, i = 0; i < OSSL_NELEM(tbl_standard); i++, tmp++) {
        if (tmp->nid < last_nid) {
            last_nid = 0;
            break;
        }
        last_nid = tmp->nid;
    }

    if (TEST_int_ne(last_nid, 0)) {
        TEST_info("asn1 tbl_standard: Table order OK");
        return 1;
    }

    TEST_info("asn1 tbl_standard: out of order");
    for (tmp = tbl_standard, i = 0; i < OSSL_NELEM(tbl_standard); i++, tmp++)
        TEST_note("asn1 tbl_standard: Index %zu, NID %d, Name=%s",
                  i, tmp->nid, OBJ_nid2ln(tmp->nid));

    return 0;
}

/**********************************************************************
 *
 * Test of ameth_lib's standard_methods
 *
 ***/

#include "crypto/asn1.h"
#include "../crypto/asn1/standard_methods.h"

static int test_standard_methods(void)
{
    const EVP_PKEY_ASN1_METHOD **tmp;
    int last_pkey_id = -1;
    size_t i;
    int ok = 1;

    for (tmp = standard_methods, i = 0; i < OSSL_NELEM(standard_methods);
         i++, tmp++) {
        if ((*tmp)->pkey_id < last_pkey_id) {
            last_pkey_id = 0;
            break;
        }
        last_pkey_id = (*tmp)->pkey_id;

        /*
         * One of the following must be true:
         *
         * pem_str == NULL AND ASN1_PKEY_ALIAS is set
         * pem_str != NULL AND ASN1_PKEY_ALIAS is clear
         *
         * Anything else is an error and may lead to a corrupt ASN1 method table
         */
        if (!TEST_true(((*tmp)->pem_str == NULL && ((*tmp)->pkey_flags & ASN1_PKEY_ALIAS) != 0)
                       || ((*tmp)->pem_str != NULL && ((*tmp)->pkey_flags & ASN1_PKEY_ALIAS) == 0))) {
            TEST_note("asn1 standard methods: Index %zu, pkey ID %d, Name=%s",
                      i, (*tmp)->pkey_id, OBJ_nid2sn((*tmp)->pkey_id));
            ok = 0;
        }
    }

    if (TEST_int_ne(last_pkey_id, 0)) {
        TEST_info("asn1 standard methods: Table order OK");
        return ok;
    }

    TEST_note("asn1 standard methods: out of order");
    for (tmp = standard_methods, i = 0; i < OSSL_NELEM(standard_methods);
         i++, tmp++)
        TEST_note("asn1 standard methods: Index %zu, pkey ID %d, Name=%s",
                  i, (*tmp)->pkey_id, OBJ_nid2sn((*tmp)->pkey_id));

    return 0;
}

/**********************************************************************
 *
 * Test of that i2d fail on non-existing non-optional items
 *
 ***/

#include <openssl/rsa.h>

static int test_empty_nonoptional_content(void)
{
    RSA *rsa = NULL;
    BIGNUM *n = NULL;
    BIGNUM *e = NULL;
    int ok = 0;

    if (!TEST_ptr(rsa = RSA_new())
        || !TEST_ptr(n = BN_new())
        || !TEST_ptr(e = BN_new())
        || !TEST_true(RSA_set0_key(rsa, n, e, NULL)))
        goto end;

    n = e = NULL;                /* They are now "owned" by |rsa| */

    /*
     * This SHOULD fail, as we're trying to encode a public key as a private
     * key.  The private key bits MUST be present for a proper RSAPrivateKey.
     */
    if (TEST_int_le(i2d_RSAPrivateKey(rsa, NULL), 0))
        ok = 1;

 end:
    RSA_free(rsa);
    BN_free(n);
    BN_free(e);
    return ok;
}

/**********************************************************************
 *
 * Tests of the Unicode code point range
 *
 ***/

static int test_unicode(const unsigned char *univ, size_t len, int expected)
{
    const unsigned char *end = univ + len;
    int ok = 1;

    for (; univ < end; univ += 4) {
        if (!TEST_int_eq(ASN1_mbstring_copy(NULL, univ, 4, MBSTRING_UNIV,
                                            B_ASN1_UTF8STRING),
                         expected))
            ok = 0;
    }
    return ok;
}

static int test_unicode_range(void)
{
    const unsigned char univ_ok[] = "\0\0\0\0"
                                    "\0\0\xd7\xff"
                                    "\0\0\xe0\x00"
                                    "\0\x10\xff\xff";
    const unsigned char univ_bad[] = "\0\0\xd8\x00"
                                     "\0\0\xdf\xff"
                                     "\0\x11\x00\x00"
                                     "\x80\x00\x00\x00"
                                     "\xff\xff\xff\xff";
    int ok = 1;

    if (!test_unicode(univ_ok, sizeof univ_ok - 1, V_ASN1_UTF8STRING))
        ok = 0;
    if (!test_unicode(univ_bad, sizeof univ_bad - 1, -1))
        ok = 0;
    return ok;
}

static int test_invalid_utf8(void)
{
    const unsigned char inv_utf8[] = "\xF4\x90\x80\x80";
    unsigned long val;

    if (!TEST_int_lt(UTF8_getc(inv_utf8, sizeof(inv_utf8), &val), 0))
        return 0;
    return 1;
}

/**********************************************************************
 *
 * Tests of object creation
 *
 ***/

static int test_obj_create_once(const char *oid, const char *sn, const char *ln)
{
    int nid;

    ERR_set_mark();

    nid = OBJ_create(oid, sn, ln);

    if (nid == NID_undef) {
        unsigned long err = ERR_peek_last_error();
        int l = ERR_GET_LIB(err);
        int r = ERR_GET_REASON(err);

        /* If it exists, that's fine, otherwise not */
        if (l != ERR_LIB_OBJ || r != OBJ_R_OID_EXISTS) {
            ERR_clear_last_mark();
            return 0;
        }
    }
    ERR_pop_to_mark();
    return 1;
}

static int test_asn1_time_conversion(char *time_string, const char *file,
                                     int line)
{
    int ret = 0;
    int is_utc = 0;
    struct tm tm;
    ASN1_TIME *asn1_time = NULL;
    ASN1_TIME *result = NULL;

    if (!TEST_ptr(asn1_time = ASN1_TIME_new()))
        goto err;
    if (!TEST_true(ASN1_TIME_set_string(asn1_time, time_string)))
        goto err;
    if (!TEST_true(ASN1_TIME_to_tm(asn1_time, &tm)))
        goto err;
    is_utc = (tm.tm_year >= 50 && tm.tm_year <= 1949);
    if (is_utc) {
        /* our original string could have been provided as gen, or utc */
        if (strlen(time_string) == 13) {
            if (!TEST_ptr(result = ossl_asn1_time_from_tm(result, &tm,
                                                          V_ASN1_UNDEF)))
                goto err;
        } else {
            if (!TEST_ptr(result
                          = ossl_asn1_time_from_tm(result, &tm,
                                                   V_ASN1_GENERALIZEDTIME)))
                goto err;
        }
    } else {
        if (!TEST_ptr(result
                      = ossl_asn1_time_from_tm(result, &tm,
                                               V_ASN1_GENERALIZEDTIME)))
            goto err;
    }
    if (!TEST_true((strcmp(time_string,
                           (const char *)ASN1_STRING_get0_data(result))
                    == 0))) {
        TEST_info("Expected time: %s, Got time: %s\n", time_string,
                  ASN1_STRING_get0_data(result));
        goto err;
    }

    ret = 1;

 err:
    if (!ret)
        TEST_info("Failed to convert time %s at %s:%d\n", time_string, file, line);
    ASN1_STRING_free(asn1_time);
    ASN1_STRING_free(result);
    return ret;
}

/*
 * These should not go through time_t so they should work on any
 * platform.
 */
static int test_asn1_time_tm_conversions(void)
{
    int fails = 0;

    fails += !test_asn1_time_conversion("00000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("10000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("16001231235959Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("16010101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("700101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("19691231235959Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("691231235959Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("19700101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("700101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("20000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("20380119031407Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("20380119031408Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("20380119031409Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("21060207062814Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("21060207062815Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("21060207062816Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("30000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("40000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("50000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("60000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("70000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("80000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("90000101000000Z", __FILE__, __LINE__);
    fails += !test_asn1_time_conversion("99991231235959Z", __FILE__, __LINE__);

    return fails == 0;
}

static int test_obj_create(void)
{
/* Stolen from evp_extra_test.c */
#define arc "1.3.6.1.4.1.16604.998866."
#define broken_arc "25."
#define sn_prefix "custom"
#define ln_prefix "custom"

    /* Try different combinations of correct object creation */
    if (!TEST_true(test_obj_create_once(NULL, sn_prefix "1", NULL))
        || !TEST_int_ne(OBJ_sn2nid(sn_prefix "1"), NID_undef)
        || !TEST_true(test_obj_create_once(NULL, NULL, ln_prefix "2"))
        || !TEST_int_ne(OBJ_ln2nid(ln_prefix "2"), NID_undef)
        || !TEST_true(test_obj_create_once(NULL, sn_prefix "3", ln_prefix "3"))
        || !TEST_int_ne(OBJ_sn2nid(sn_prefix "3"), NID_undef)
        || !TEST_int_ne(OBJ_ln2nid(ln_prefix "3"), NID_undef)
        || !TEST_true(test_obj_create_once(arc "4", NULL, NULL))
        || !TEST_true(test_obj_create_once(arc "5", sn_prefix "5", NULL))
        || !TEST_int_ne(OBJ_sn2nid(sn_prefix "5"), NID_undef)
        || !TEST_true(test_obj_create_once(arc "6", NULL, ln_prefix "6"))
        || !TEST_int_ne(OBJ_ln2nid(ln_prefix "6"), NID_undef)
        || !TEST_true(test_obj_create_once(arc "7",
                                           sn_prefix "7", ln_prefix "7"))
        || !TEST_int_ne(OBJ_sn2nid(sn_prefix "7"), NID_undef)
        || !TEST_int_ne(OBJ_ln2nid(ln_prefix "7"), NID_undef))
        return 0;

    if (!TEST_false(test_obj_create_once(NULL, NULL, NULL))
        || !TEST_false(test_obj_create_once(broken_arc "8",
                                            sn_prefix "8", ln_prefix "8")))
        return 0;

    return 1;
}

static int test_obj_nid_undef(void)
{
    if (!TEST_ptr(OBJ_nid2obj(NID_undef))
        || !TEST_ptr(OBJ_nid2sn(NID_undef))
        || !TEST_ptr(OBJ_nid2ln(NID_undef)))
        return 0;

    return 1;
}

/* 0000-01-01 00:00:00 UTC */
#define MIN_POSIX_TIME INT64_C(-62167219200)
/* 9999-12-31 23:59:59 UTC */
#define MAX_POSIX_TIME INT64_C(253402300799)

extern ASN1_TIME *ossl_asn1_time_from_tm(ASN1_TIME *s, struct tm *ts, int type);

/* A time_t immune way to get an ASN1_TIME via the same internal conversion */
static int ossl_asn1_time_from_posix(int64_t posix_time, ASN1_TIME *out_time)
{
    struct tm tm;

    if (!OPENSSL_posix_to_tm(posix_time, &tm))
        return 0;
    if (out_time == NULL)
        return 0;
    return ossl_asn1_time_from_tm(out_time, &tm, V_ASN1_UNDEF) != NULL;
}

static int test_a_posix_time(int64_t time, struct tm *tm, ASN1_TIME *asn1_time)
{
    /*
     * We test a posix time by converting it to a tm, converting
     * that to an ASN1_TIME, converting the ASN1_TIME back to a tm,
     * and then converting the tm back to a posix time.
     *
     * We expect all of those steps to fail correctly for values
     * outside of the valid range, and to work correctly for
     * values in the range
     */
    int expected_to_work = time >= MIN_POSIX_TIME && time <= MAX_POSIX_TIME;
    time_t time_as_time_t;
    int ret = 1;

    if (!TEST_int_eq(OPENSSL_posix_to_tm(time, tm), expected_to_work)) {
        TEST_info("OPENSSL_posix_to_tm %s unexpectedly converting %lld\n",
                  expected_to_work ? "failed" : "succeeded", (long long) time);
        ret = 0;

    }
    if (ret && expected_to_work && OPENSSL_timegm(tm, &time_as_time_t)) {
        /*
         * When we got a tm from the previous step, and if
         * OPENSSL_timegm succeeds, whatever value we are testing fits
         * in a time_t on this platform, so we can call ASN1_TIME_adj
         * with it to try to convert to ASN1_TIME. It remains an enduring
         * tragedy that adj takes time_t and not int64_t.
         */
        if ((ASN1_TIME_adj(asn1_time, time_as_time_t, 0, 0) != NULL)
            == !expected_to_work) {
            TEST_info("ASN1_TIME_adj %s unexpectedly converting %lld\n",
                      expected_to_work ? "failed" : "succeeded",
                      (long long) time);
            ret = 0;
        }
    } else {
        /*
         * otherwise we still call the same underlying conversion, but without
         * passing through time_t, to test the underlying conversion to ASN1_TIME.
         */
        if (!TEST_int_eq(ossl_asn1_time_from_posix(time, asn1_time),
                         expected_to_work)) {
            TEST_info("ossl_asn1_time_from_posix %s unexpectedly converting %lld\n",
                      expected_to_work ? "failed" : "succeeded",
                      (long long) time);
            ret = 0;
        }
    }
    if (expected_to_work) {
        int64_t should_be_same = time - 1;

        /*
         * We should have an ASN1_TIME from the previous steps, it should
         * convert to a tm (no matter what time_t is)
         */
        if (!ASN1_TIME_to_tm(asn1_time, tm)) {
            TEST_info("ASN1_TIME_to_tm failed unexpectedly converting %lld\n",
                      (long long) time);
            ret = 0;
        }
        /* That tm should convert back to a posix time */
        if (!TEST_int_eq(OPENSSL_tm_to_posix(tm, &should_be_same),
                         expected_to_work)) {
            TEST_info("OPENSSL_tm_to_posix failed unexpectedly converting %lld\n",
                      (long long) time);
            ret = 0;
        }
        /* The resulting posix time should be the same one we started with. */
        if (!TEST_int64_t_eq(time, should_be_same)) {
            TEST_info("Got converted time of time of %lld, expected %lld\n",
                      (long long) should_be_same, (long long) time);
            ret = 0;
        }
    }

    return ret;
}

static int posix_time_test(void)
{
    int ret = 0;
    int64_t test_time;
    struct tm tm;
    ASN1_TIME *conversion_time = NULL;

    conversion_time = ASN1_TIME_new();
    if (!TEST_ptr(conversion_time)) {
        TEST_info("malloc failed\n");
        goto err;
    }

    /*
     * Frequently platform conversions can not deal with one second before the
     * the Unix epoch, due to inheriting terrible API design and knocking this
     * time value out as an error return.
     *
     * We should do better.
     */
    if (!test_a_posix_time(-1, &tm, conversion_time))
        goto err;

    /* The epoch should also not be an error */
    if (!test_a_posix_time(0, &tm, conversion_time))
        goto err;
    /*
     * A time oddly near the epoch, but not quite at the epoch.,
     * In memory of Vernor Vinge.
     */
    if (!test_a_posix_time(-16751025, &tm, conversion_time))
        goto err;

    /* Test the minimum boundary */
    if (!test_a_posix_time(MIN_POSIX_TIME - 1, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(MIN_POSIX_TIME, &tm, conversion_time))
        goto err;

    /* Test the maximum boundary */
    if (!test_a_posix_time(MAX_POSIX_TIME, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(MAX_POSIX_TIME + 1, &tm, conversion_time))
        goto err;

    /*
     * Only the bad idea bears who visited the platform authors know
     * for certain what decisions were made about time_t. So let's
     * make sure we test the realistic limiting values of the
     * possible outcomes of that visit.
     */
    if (!test_a_posix_time(INT_MAX, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(INT_MIN, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(UINT_MAX, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(INT32_MAX, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(INT32_MIN, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(UINT32_MAX, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(INT64_MAX, &tm, conversion_time))
        goto err;

    if (!test_a_posix_time(INT64_MIN, &tm, conversion_time))
        goto err;

    /* Test from outside through and past the full range of validity */
#define INCREMENT 100000 /* because doing them all is a bit slow */

    for (test_time = MIN_POSIX_TIME - INCREMENT;
         test_time <= MAX_POSIX_TIME + INCREMENT;
         test_time += INCREMENT) {
        if (!test_a_posix_time(test_time, &tm, conversion_time))
            goto err;
    }

    ret = 1;

 err:
    ASN1_TIME_free(conversion_time);
    return ret;
}

int setup_tests(void)
{
    ADD_TEST(test_tbl_standard);
    ADD_TEST(test_standard_methods);
    ADD_TEST(test_empty_nonoptional_content);
    ADD_TEST(test_unicode_range);
    ADD_TEST(test_invalid_utf8);
    ADD_TEST(test_obj_create);
    ADD_TEST(test_obj_nid_undef);
    ADD_TEST(posix_time_test);
    ADD_TEST(test_asn1_time_tm_conversions);
    return 1;
}
