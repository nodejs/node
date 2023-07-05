/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Internal tests for the OpenSSL library context */

#include "internal/cryptlib.h"
#include "testutil.h"

/*
 * Everything between BEGIN EXAMPLE and END EXAMPLE is copied from
 * doc/internal/man3/ossl_lib_ctx_get_data.pod
 */

/*
 * ======================================================================
 * BEGIN EXAMPLE
 */

typedef struct foo_st {
    int i;
    void *data;
} FOO;

static void *foo_new(OSSL_LIB_CTX *ctx)
{
    FOO *ptr = OPENSSL_zalloc(sizeof(*ptr));
    if (ptr != NULL)
        ptr->i = 42;
    return ptr;
}
static void foo_free(void *ptr)
{
    OPENSSL_free(ptr);
}
static const OSSL_LIB_CTX_METHOD foo_method = {
    OSSL_LIB_CTX_METHOD_DEFAULT_PRIORITY,
    foo_new,
    foo_free
};

/*
 * END EXAMPLE
 * ======================================================================
 */

static int test_context(OSSL_LIB_CTX *ctx)
{
    FOO *data = NULL;

    return TEST_ptr(data = ossl_lib_ctx_get_data(ctx, 0, &foo_method))
        /* OPENSSL_zalloc in foo_new() initialized it to zero */
        && TEST_int_eq(data->i, 42);
}

static int test_app_context(void)
{
    OSSL_LIB_CTX *ctx = NULL;
    int result =
        TEST_ptr(ctx = OSSL_LIB_CTX_new())
        && test_context(ctx);

    OSSL_LIB_CTX_free(ctx);
    return result;
}

static int test_def_context(void)
{
    return test_context(NULL);
}

static int test_set0_default(void)
{
    OSSL_LIB_CTX *global = OSSL_LIB_CTX_get0_global_default();
    OSSL_LIB_CTX *local = OSSL_LIB_CTX_new();
    OSSL_LIB_CTX *prev;
    int testresult = 0;
    FOO *data = NULL;

    if (!TEST_ptr(global)
            || !TEST_ptr(local)
            || !TEST_ptr_eq(global, OSSL_LIB_CTX_set0_default(NULL))
            || !TEST_ptr(data = ossl_lib_ctx_get_data(local, 0, &foo_method)))
        goto err;

    /* Set local "i" value to 43. Global "i" should be 42 */
    data->i++;
    if (!TEST_int_eq(data->i, 43))
        goto err;

    /* The default context should still be the "global" default */
    if (!TEST_ptr(data = ossl_lib_ctx_get_data(NULL, 0, &foo_method))
            || !TEST_int_eq(data->i, 42))
        goto err;

    /* Check we can change the local default context */
    if (!TEST_ptr(prev = OSSL_LIB_CTX_set0_default(local))
            || !TEST_ptr_eq(global, prev)
            || !TEST_ptr(data = ossl_lib_ctx_get_data(NULL, 0, &foo_method))
            || !TEST_int_eq(data->i, 43))
        goto err;

    /* Calling OSSL_LIB_CTX_set0_default() with a NULL should be a no-op */
    if (!TEST_ptr_eq(local, OSSL_LIB_CTX_set0_default(NULL))
            || !TEST_ptr(data = ossl_lib_ctx_get_data(NULL, 0, &foo_method))
            || !TEST_int_eq(data->i, 43))
        goto err;

    /* Global default should be unchanged */
    if (!TEST_ptr_eq(global, OSSL_LIB_CTX_get0_global_default()))
        goto err;

    /* Check we can swap back to the global default */
   if (!TEST_ptr(prev = OSSL_LIB_CTX_set0_default(global))
            || !TEST_ptr_eq(local, prev)
            || !TEST_ptr(data = ossl_lib_ctx_get_data(NULL, 0, &foo_method))
            || !TEST_int_eq(data->i, 42))
        goto err;

    testresult = 1;
 err:
    OSSL_LIB_CTX_free(local);
    return testresult;
}

int setup_tests(void)
{
    ADD_TEST(test_app_context);
    ADD_TEST(test_def_context);
    ADD_TEST(test_set0_default);
    return 1;
}
