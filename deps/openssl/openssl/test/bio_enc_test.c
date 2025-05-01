/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

#include "testutil.h"

#define ENCRYPT  1
#define DECRYPT  0

#define DATA_SIZE    1024
#define MAX_IV       32
#define BUF_SIZE     (DATA_SIZE + MAX_IV)

static const unsigned char KEY[] = {
    0x51, 0x50, 0xd1, 0x77, 0x2f, 0x50, 0x83, 0x4a,
    0x50, 0x3e, 0x06, 0x9a, 0x97, 0x3f, 0xbd, 0x7c,
    0xe6, 0x1c, 0x43, 0x2b, 0x72, 0x0b, 0x19, 0xd1,
    0x8e, 0xc8, 0xd8, 0x4b, 0xdc, 0x63, 0x15, 0x1b
};

static const unsigned char IV[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

static int do_bio_cipher(const EVP_CIPHER* cipher, const unsigned char* key,
    const unsigned char* iv)
{
    BIO *b, *mem;
    static unsigned char inp[BUF_SIZE] = { 0 };
    unsigned char out[BUF_SIZE], ref[BUF_SIZE];
    int i, lref, len;

    /* Fill buffer with non-zero data so that over steps can be detected */
    if (!TEST_int_gt(RAND_bytes(inp, DATA_SIZE), 0))
        return 0;

    /* Encrypt tests */

    /* reference output for single-chunk operation */
    b = BIO_new(BIO_f_cipher());
    if (!TEST_ptr(b))
        return 0;
    if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, ENCRYPT)))
        goto err;
    mem = BIO_new_mem_buf(inp, DATA_SIZE);
    if (!TEST_ptr(mem))
        goto err;
    BIO_push(b, mem);
    lref = BIO_read(b, ref, sizeof(ref));
    BIO_free_all(b);

    /* perform split operations and compare to reference */
    for (i = 1; i < lref; i++) {
        b = BIO_new(BIO_f_cipher());
        if (!TEST_ptr(b))
            return 0;
        if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, ENCRYPT))) {
            TEST_info("Split encrypt failed @ operation %d", i);
            goto err;
        }
        mem = BIO_new_mem_buf(inp, DATA_SIZE);
        if (!TEST_ptr(mem))
            goto err;
        BIO_push(b, mem);
        memset(out, 0, sizeof(out));
        out[i] = ~ref[i];
        len = BIO_read(b, out, i);
        /* check for overstep */
        if (!TEST_uchar_eq(out[i], (unsigned char)~ref[i])) {
            TEST_info("Encrypt overstep check failed @ operation %d", i);
            goto err;
        }
        len += BIO_read(b, out + len, sizeof(out) - len);
        BIO_free_all(b);

        if (!TEST_mem_eq(out, len, ref, lref)) {
            TEST_info("Encrypt compare failed @ operation %d", i);
            return 0;
        }
    }

    /* perform small-chunk operations and compare to reference */
    for (i = 1; i < lref / 2; i++) {
        int delta;

        b = BIO_new(BIO_f_cipher());
        if (!TEST_ptr(b))
            return 0;
        if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, ENCRYPT))) {
            TEST_info("Small chunk encrypt failed @ operation %d", i);
            goto err;
        }
        mem = BIO_new_mem_buf(inp, DATA_SIZE);
        if (!TEST_ptr(mem))
            goto err;
        BIO_push(b, mem);
        memset(out, 0, sizeof(out));
        for (len = 0; (delta = BIO_read(b, out + len, i)); ) {
            len += delta;
        }
        BIO_free_all(b);

        if (!TEST_mem_eq(out, len, ref, lref)) {
            TEST_info("Small chunk encrypt compare failed @ operation %d", i);
            return 0;
        }
    }

    /* Decrypt tests */

    /* reference output for single-chunk operation */
    b = BIO_new(BIO_f_cipher());
    if (!TEST_ptr(b))
        return 0;
    if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, DECRYPT)))
        goto err;
    /* Use original reference output as input */
    mem = BIO_new_mem_buf(ref, lref);
    if (!TEST_ptr(mem))
        goto err;
    BIO_push(b, mem);
    (void)BIO_flush(b);
    memset(out, 0, sizeof(out));
    len = BIO_read(b, out, sizeof(out));
    BIO_free_all(b);

    if (!TEST_mem_eq(inp, DATA_SIZE, out, len))
        return 0;

    /* perform split operations and compare to reference */
    for (i = 1; i < lref; i++) {
        b = BIO_new(BIO_f_cipher());
        if (!TEST_ptr(b))
            return 0;
        if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, DECRYPT))) {
            TEST_info("Split decrypt failed @ operation %d", i);
            goto err;
        }
        mem = BIO_new_mem_buf(ref, lref);
        if (!TEST_ptr(mem))
            goto err;
        BIO_push(b, mem);
        memset(out, 0, sizeof(out));
        out[i] = ~ref[i];
        len = BIO_read(b, out, i);
        /* check for overstep */
        if (!TEST_uchar_eq(out[i], (unsigned char)~ref[i])) {
            TEST_info("Decrypt overstep check failed @ operation %d", i);
            goto err;
        }
        len += BIO_read(b, out + len, sizeof(out) - len);
        BIO_free_all(b);

        if (!TEST_mem_eq(inp, DATA_SIZE, out, len)) {
            TEST_info("Decrypt compare failed @ operation %d", i);
            return 0;
        }
    }

    /* perform small-chunk operations and compare to reference */
    for (i = 1; i < lref / 2; i++) {
        int delta;

        b = BIO_new(BIO_f_cipher());
        if (!TEST_ptr(b))
            return 0;
        if (!TEST_true(BIO_set_cipher(b, cipher, key, iv, DECRYPT))) {
            TEST_info("Small chunk decrypt failed @ operation %d", i);
            goto err;
        }
        mem = BIO_new_mem_buf(ref, lref);
        if (!TEST_ptr(mem))
            goto err;
        BIO_push(b, mem);
        memset(out, 0, sizeof(out));
        for (len = 0; (delta = BIO_read(b, out + len, i)); ) {
            len += delta;
        }
        BIO_free_all(b);

        if (!TEST_mem_eq(inp, DATA_SIZE, out, len)) {
            TEST_info("Small chunk decrypt compare failed @ operation %d", i);
            return 0;
        }
    }

    return 1;

err:
    BIO_free_all(b);
    return 0;
}

static int do_test_bio_cipher(const EVP_CIPHER* cipher, int idx)
{
    switch(idx)
    {
        case 0:
            return do_bio_cipher(cipher, KEY, NULL);
        case 1:
            return do_bio_cipher(cipher, KEY, IV);
    }
    return 0;
}

static int test_bio_enc_aes_128_cbc(int idx)
{
    return do_test_bio_cipher(EVP_aes_128_cbc(), idx);
}

static int test_bio_enc_aes_128_ctr(int idx)
{
    return do_test_bio_cipher(EVP_aes_128_ctr(), idx);
}

static int test_bio_enc_aes_256_cfb(int idx)
{
    return do_test_bio_cipher(EVP_aes_256_cfb(), idx);
}

static int test_bio_enc_aes_256_ofb(int idx)
{
    return do_test_bio_cipher(EVP_aes_256_ofb(), idx);
}

# ifndef OPENSSL_NO_CHACHA
static int test_bio_enc_chacha20(int idx)
{
    return do_test_bio_cipher(EVP_chacha20(), idx);
}

#  ifndef OPENSSL_NO_POLY1305
static int test_bio_enc_chacha20_poly1305(int idx)
{
    return do_test_bio_cipher(EVP_chacha20_poly1305(), idx);
}
#  endif
# endif

int setup_tests(void)
{
    ADD_ALL_TESTS(test_bio_enc_aes_128_cbc, 2);
    ADD_ALL_TESTS(test_bio_enc_aes_128_ctr, 2);
    ADD_ALL_TESTS(test_bio_enc_aes_256_cfb, 2);
    ADD_ALL_TESTS(test_bio_enc_aes_256_ofb, 2);
# ifndef OPENSSL_NO_CHACHA
    ADD_ALL_TESTS(test_bio_enc_chacha20, 2);
#  ifndef OPENSSL_NO_POLY1305
    ADD_ALL_TESTS(test_bio_enc_chacha20_poly1305, 2);
#  endif
# endif
    return 1;
}
