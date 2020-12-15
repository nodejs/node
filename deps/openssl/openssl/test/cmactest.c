/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "internal/nelem.h"

#include <openssl/cmac.h>
#include <openssl/aes.h>
#include <openssl/evp.h>

#include "testutil.h"

static const char xtskey[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

static struct test_st {
    const char key[32];
    int key_len;
    const unsigned char data[64];
    int data_len;
    const char *mac;
} test[3] = {
    {
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
            0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        },
        16,
        "My test data",
        12,
        "29cec977c48f63c200bd5c4a6881b224"
    },
    {
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
            0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
            0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
        },
        32,
        "My test data",
        12,
        "db6493aa04e4761f473b2b453c031c9a"
    },
    {
        {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
            0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
            0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
        },
        32,
        "My test data again",
        18,
        "65c11c75ecf590badd0a5e56cbb8af60"
    },
};

static char *pt(unsigned char *md, unsigned int len);

static int test_cmac_bad(void)
{
    CMAC_CTX *ctx = NULL;
    int ret = 0;

    ctx = CMAC_CTX_new();
    if (!TEST_ptr(ctx)
        || !TEST_false(CMAC_Init(ctx, NULL, 0, NULL, NULL))
        || !TEST_false(CMAC_Update(ctx, test[0].data, test[0].data_len))
           /* Should be able to pass cipher first, and then key */
        || !TEST_true(CMAC_Init(ctx, NULL, 0, EVP_aes_128_cbc(), NULL))
           /* Must have a key */
        || !TEST_false(CMAC_Update(ctx, test[0].data, test[0].data_len))
           /* Now supply the key */
        || !TEST_true(CMAC_Init(ctx, test[0].key, test[0].key_len, NULL, NULL))
           /* Update should now work */
        || !TEST_true(CMAC_Update(ctx, test[0].data, test[0].data_len))
           /* XTS is not a suitable cipher to use */
        || !TEST_false(CMAC_Init(ctx, xtskey, sizeof(xtskey), EVP_aes_128_xts(),
                                 NULL))
        || !TEST_false(CMAC_Update(ctx, test[0].data, test[0].data_len)))
        goto err;

    ret = 1;
err:
    CMAC_CTX_free(ctx);
    return ret;
}

static int test_cmac_run(void)
{
    char *p;
    CMAC_CTX *ctx = NULL;
    unsigned char buf[AES_BLOCK_SIZE];
    size_t len;
    int ret = 0;

    ctx = CMAC_CTX_new();

    if (!TEST_true(CMAC_Init(ctx, test[0].key, test[0].key_len,
                             EVP_aes_128_cbc(), NULL))
        || !TEST_true(CMAC_Update(ctx, test[0].data, test[0].data_len))
        || !TEST_true(CMAC_Final(ctx, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, test[0].mac))
        goto err;

    if (!TEST_true(CMAC_Init(ctx, test[1].key, test[1].key_len,
                             EVP_aes_256_cbc(), NULL))
        || !TEST_true(CMAC_Update(ctx, test[1].data, test[1].data_len))
        || !TEST_true(CMAC_Final(ctx, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, test[1].mac))
        goto err;

    if (!TEST_true(CMAC_Init(ctx, test[2].key, test[2].key_len, NULL, NULL))
        || !TEST_true(CMAC_Update(ctx, test[2].data, test[2].data_len))
        || !TEST_true(CMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, test[2].mac))
        goto err;
    /* Test reusing a key */
    if (!TEST_true(CMAC_Init(ctx, NULL, 0, NULL, NULL))
        || !TEST_true(CMAC_Update(ctx, test[2].data, test[2].data_len))
        || !TEST_true(CMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, test[2].mac))
        goto err;

    /* Test setting the cipher and key separately */
    if (!TEST_true(CMAC_Init(ctx, NULL, 0, EVP_aes_256_cbc(), NULL))
        || !TEST_true(CMAC_Init(ctx, test[2].key, test[2].key_len, NULL, NULL))
        || !TEST_true(CMAC_Update(ctx, test[2].data, test[2].data_len))
        || !TEST_true(CMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, test[2].mac))
        goto err;

    ret = 1;
err:
    CMAC_CTX_free(ctx);
    return ret;
}

static int test_cmac_copy(void)
{
    char *p;
    CMAC_CTX *ctx = NULL, *ctx2 = NULL;
    unsigned char buf[AES_BLOCK_SIZE];
    size_t len;
    int ret = 0;

    ctx = CMAC_CTX_new();
    ctx2 = CMAC_CTX_new();
    if (!TEST_ptr(ctx) || !TEST_ptr(ctx2))
        goto err;

    if (!TEST_true(CMAC_Init(ctx, test[0].key, test[0].key_len,
                             EVP_aes_128_cbc(), NULL))
        || !TEST_true(CMAC_Update(ctx, test[0].data, test[0].data_len))
        || !TEST_true(CMAC_CTX_copy(ctx2, ctx))
        || !TEST_true(CMAC_Final(ctx2, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, test[0].mac))
        goto err;

    ret = 1;
err:
    CMAC_CTX_free(ctx2);
    CMAC_CTX_free(ctx);
    return ret;
}

static char *pt(unsigned char *md, unsigned int len)
{
    unsigned int i;
    static char buf[80];

    for (i = 0; i < len; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return buf;
}

int setup_tests(void)
{
    ADD_TEST(test_cmac_bad);
    ADD_TEST(test_cmac_run);
    ADD_TEST(test_cmac_copy);
    return 1;
}

