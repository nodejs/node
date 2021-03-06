/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "internal/nelem.h"

# include <openssl/hmac.h>
# include <openssl/sha.h>
# ifndef OPENSSL_NO_MD5
#  include <openssl/md5.h>
# endif

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

#include "testutil.h"

# ifndef OPENSSL_NO_MD5
static struct test_st {
    unsigned char key[16];
    int key_len;
    unsigned char data[64];
    int data_len;
    unsigned char *digest;
} test[8] = {
    {
        "", 0, "More text test vectors to stuff up EBCDIC machines :-)", 54,
        (unsigned char *)"e9139d1e6ee064ef8cf514fc7dc83e86",
    },
    {
        {
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        }, 16, "Hi There", 8,
        (unsigned char *)"9294727a3638bb1c13f48ef8158bfc9d",
    },
    {
        "Jefe", 4, "what do ya want for nothing?", 28,
        (unsigned char *)"750c783e6ab0b503eaa86e310a5db738",
    },
    {
        {
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        }, 16, {
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd
        }, 50, (unsigned char *)"56be34521d144c88dbb8c733f0e8b3f6",
    },
    {
        "", 0, "My test data", 12,
        (unsigned char *)"61afdecb95429ef494d61fdee15990cabf0826fc"
    },
    {
        "", 0, "My test data", 12,
        (unsigned char *)"2274b195d90ce8e03406f4b526a47e0787a88a65479938f1a5baa3ce0f079776"
    },
    {
        "123456", 6, "My test data", 12,
        (unsigned char *)"bab53058ae861a7f191abe2d0145cbb123776a6369ee3f9d79ce455667e411dd"
    },
    {
        "12345", 5, "My test data again", 18,
        (unsigned char *)"a12396ceddd2a85f4c656bc1e0aa50c78cffde3e"
    }
};
# endif

static char *pt(unsigned char *md, unsigned int len);


# ifndef OPENSSL_NO_MD5
static int test_hmac_md5(int idx)
{
    char *p;
#  ifdef CHARSET_EBCDIC
    ebcdic2ascii(test[0].data, test[0].data, test[0].data_len);
    ebcdic2ascii(test[1].data, test[1].data, test[1].data_len);
    ebcdic2ascii(test[2].key, test[2].key, test[2].key_len);
    ebcdic2ascii(test[2].data, test[2].data, test[2].data_len);
#  endif

    p = pt(HMAC(EVP_md5(),
                test[idx].key, test[idx].key_len,
                test[idx].data, test[idx].data_len, NULL, NULL),
                MD5_DIGEST_LENGTH);

    if (!TEST_str_eq(p, (char *)test[idx].digest))
      return 0;

    return 1;
}
# endif

static int test_hmac_bad(void)
{
    HMAC_CTX *ctx = NULL;
    int ret = 0;

    ctx = HMAC_CTX_new();
    if (!TEST_ptr(ctx)
        || !TEST_ptr_null(HMAC_CTX_get_md(ctx))
        || !TEST_false(HMAC_Init_ex(ctx, NULL, 0, NULL, NULL))
        || !TEST_false(HMAC_Update(ctx, test[4].data, test[4].data_len))
        || !TEST_false(HMAC_Init_ex(ctx, NULL, 0, EVP_sha1(), NULL))
        || !TEST_false(HMAC_Update(ctx, test[4].data, test[4].data_len)))
        goto err;

    ret = 1;
err:
    HMAC_CTX_free(ctx);
    return ret;
}

static int test_hmac_run(void)
{
    char *p;
    HMAC_CTX *ctx = NULL;
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len;
    int ret = 0;

    ctx = HMAC_CTX_new();
    HMAC_CTX_reset(ctx);

    if (!TEST_ptr(ctx)
        || !TEST_ptr_null(HMAC_CTX_get_md(ctx))
        || !TEST_false(HMAC_Init_ex(ctx, NULL, 0, NULL, NULL))
        || !TEST_false(HMAC_Update(ctx, test[4].data, test[4].data_len))
        || !TEST_false(HMAC_Init_ex(ctx, test[4].key, -1, EVP_sha1(), NULL)))
        goto err;

    if (!TEST_true(HMAC_Init_ex(ctx, test[4].key, test[4].key_len, EVP_sha1(), NULL))
        || !TEST_true(HMAC_Update(ctx, test[4].data, test[4].data_len))
        || !TEST_true(HMAC_Final(ctx, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[4].digest))
        goto err;

    if (!TEST_false(HMAC_Init_ex(ctx, NULL, 0, EVP_sha256(), NULL)))
        goto err;

    if (!TEST_true(HMAC_Init_ex(ctx, test[5].key, test[5].key_len, EVP_sha256(), NULL))
        || !TEST_ptr_eq(HMAC_CTX_get_md(ctx), EVP_sha256())
        || !TEST_true(HMAC_Update(ctx, test[5].data, test[5].data_len))
        || !TEST_true(HMAC_Final(ctx, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[5].digest))
        goto err;

    if (!TEST_true(HMAC_Init_ex(ctx, test[6].key, test[6].key_len, NULL, NULL))
        || !TEST_true(HMAC_Update(ctx, test[6].data, test[6].data_len))
        || !TEST_true(HMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[6].digest))
        goto err;

    /* Test reusing a key */
    if (!TEST_true(HMAC_Init_ex(ctx, NULL, 0, NULL, NULL))
        || !TEST_true(HMAC_Update(ctx, test[6].data, test[6].data_len))
        || !TEST_true(HMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[6].digest))
        goto err;

    /*
     * Test reusing a key where the digest is provided again but is the same as
     * last time
     */
    if (!TEST_true(HMAC_Init_ex(ctx, NULL, 0, EVP_sha256(), NULL))
        || !TEST_true(HMAC_Update(ctx, test[6].data, test[6].data_len))
        || !TEST_true(HMAC_Final(ctx, buf, &len)))
        goto err;
    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[6].digest))
        goto err;

    ret = 1;
err:
    HMAC_CTX_free(ctx);
    return ret;
}


static int test_hmac_single_shot(void)
{
    char *p;

    /* Test single-shot with an empty key. */
    p = pt(HMAC(EVP_sha1(), NULL, 0, test[4].data, test[4].data_len,
                NULL, NULL), SHA_DIGEST_LENGTH);
    if (!TEST_str_eq(p, (char *)test[4].digest))
        return 0;

    return 1;
}


static int test_hmac_copy(void)
{
    char *p;
    HMAC_CTX *ctx = NULL, *ctx2 = NULL;
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len;
    int ret = 0;

    ctx = HMAC_CTX_new();
    ctx2 = HMAC_CTX_new();
    if (!TEST_ptr(ctx) || !TEST_ptr(ctx2))
        goto err;

    if (!TEST_true(HMAC_Init_ex(ctx, test[7].key, test[7].key_len, EVP_sha1(), NULL))
        || !TEST_true(HMAC_Update(ctx, test[7].data, test[7].data_len))
        || !TEST_true(HMAC_CTX_copy(ctx2, ctx))
        || !TEST_true(HMAC_Final(ctx2, buf, &len)))
        goto err;

    p = pt(buf, len);
    if (!TEST_str_eq(p, (char *)test[7].digest))
        goto err;

    ret = 1;
err:
    HMAC_CTX_free(ctx2);
    HMAC_CTX_free(ctx);
    return ret;
}

# ifndef OPENSSL_NO_MD5
static char *pt(unsigned char *md, unsigned int len)
{
    unsigned int i;
    static char buf[80];

    for (i = 0; i < len; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return buf;
}
# endif

int setup_tests(void)
{
    ADD_ALL_TESTS(test_hmac_md5, 4);
    ADD_TEST(test_hmac_single_shot);
    ADD_TEST(test_hmac_bad);
    ADD_TEST(test_hmac_run);
    ADD_TEST(test_hmac_copy);
    return 1;
}

