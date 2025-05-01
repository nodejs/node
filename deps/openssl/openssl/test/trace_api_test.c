/*
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/trace.h>

#include "testutil.h"

static int test_trace_categories(void)
{
    int cat_num;

    for (cat_num = -1; cat_num <= OSSL_TRACE_CATEGORY_NUM + 1; ++cat_num) {
        const char *cat_name = OSSL_trace_get_category_name(cat_num);
        int is_cat_name_eq = 0;
        int ret_cat_num;
        int expected_ret;

        switch (cat_num) {
#define CASE(name) \
        case OSSL_TRACE_CATEGORY_##name: \
            is_cat_name_eq = TEST_str_eq(cat_name, #name); \
            break

        CASE(ALL);
        CASE(TRACE);
        CASE(INIT);
        CASE(TLS);
        CASE(TLS_CIPHER);
        CASE(CONF);
        CASE(ENGINE_TABLE);
        CASE(ENGINE_REF_COUNT);
        CASE(PKCS5V2);
        CASE(PKCS12_KEYGEN);
        CASE(PKCS12_DECRYPT);
        CASE(X509V3_POLICY);
        CASE(BN_CTX);
        CASE(CMP);
        CASE(STORE);
        CASE(DECODER);
        CASE(ENCODER);
        CASE(REF_COUNT);
#undef CASE
        default:
            is_cat_name_eq = TEST_ptr_null(cat_name);
            break;
        }

        if (!TEST_true(is_cat_name_eq))
            return 0;
        ret_cat_num =
            OSSL_trace_get_category_num(cat_name);
        expected_ret = cat_name != NULL ? cat_num : -1;
        if (!TEST_int_eq(expected_ret, ret_cat_num))
            return 0;
    }

    return 1;
}

#ifndef OPENSSL_NO_TRACE
static void put_trace_output(void)
{
    OSSL_TRACE_BEGIN(REF_COUNT) {
        BIO_printf(trc_out, "Hello World\n");
        BIO_printf(trc_out, "Good Bye Universe\n");
    } OSSL_TRACE_END(REF_COUNT);
}

static int test_trace_channel(void)
{
    static const char expected[] = "xyz-\nHello World\nGood Bye Universe\n-abc\n";
    static const char expected_len = sizeof(expected) - 1;
    BIO *bio = NULL;
    char *p_buf = NULL;
    long len = 0;
    int ret = 0;

    bio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio))
        goto end;

    if (!TEST_int_eq(OSSL_trace_set_channel(OSSL_TRACE_CATEGORY_REF_COUNT, bio), 1))
        goto end;

    if (!TEST_true(OSSL_trace_enabled(OSSL_TRACE_CATEGORY_REF_COUNT)))
        goto end;

    if (!TEST_int_eq(OSSL_trace_set_prefix(OSSL_TRACE_CATEGORY_REF_COUNT, "xyz-"), 1))
        goto end;
    if (!TEST_int_eq(OSSL_trace_set_suffix(OSSL_TRACE_CATEGORY_REF_COUNT, "-abc"), 1))
        goto end;

    put_trace_output();
    len = BIO_get_mem_data(bio, &p_buf);
    if (!TEST_strn2_eq(p_buf, len, expected, expected_len))
        goto end;
    if (!TEST_int_eq(OSSL_trace_set_channel(OSSL_TRACE_CATEGORY_REF_COUNT, NULL), 1))
        goto end;
    bio = NULL;

    ret = 1;
 end:
    BIO_free(bio);
    return ret;
}

static int trace_cb_failure;
static int trace_cb_called;

static size_t trace_cb(const char *buffer, size_t count,
                       int category, int cmd, void *data)
{
    trace_cb_called = 1;
    if (!TEST_true(category == OSSL_TRACE_CATEGORY_TRACE))
        trace_cb_failure = 1;
    return count;
}

static int test_trace_callback(void)
{
    int ret = 0;

    if (!TEST_true(OSSL_trace_set_callback(OSSL_TRACE_CATEGORY_TRACE, trace_cb,
                                           NULL)))
        goto end;

    put_trace_output();

    if (!TEST_false(trace_cb_failure) || !TEST_true(trace_cb_called))
        goto end;

    ret = 1;
 end:
    return ret;
}
#endif

OPT_TEST_DECLARE_USAGE("\n")

int setup_tests(void)
{
    if (!test_skip_common_options()) {
        TEST_error("Error parsing test options\n");
        return 0;
    }

    ADD_TEST(test_trace_categories);
#ifndef OPENSSL_NO_TRACE
    ADD_TEST(test_trace_channel);
    ADD_TEST(test_trace_callback);
#endif
    return 1;
}

void cleanup_tests(void)
{
}
