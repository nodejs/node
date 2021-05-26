/*
 * Copyright 2018-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <stdio.h>
#include <string.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>

#include "testutil.h"

static int test_bio_memleak(void)
{
    int ok = 0;
    BIO *bio;
    BUF_MEM bufmem;
    static const char str[] = "BIO test\n";
    char buf[100];

    bio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio))
        goto finish;
    bufmem.length = sizeof(str);
    bufmem.data = (char *) str;
    bufmem.max = bufmem.length;
    BIO_set_mem_buf(bio, &bufmem, BIO_NOCLOSE);
    BIO_set_flags(bio, BIO_FLAGS_MEM_RDONLY);
    if (!TEST_int_eq(BIO_read(bio, buf, sizeof(buf)), sizeof(str)))
        goto finish;
    if (!TEST_mem_eq(buf, sizeof(str), str, sizeof(str)))
        goto finish;
    ok = 1;

finish:
    BIO_free(bio);
    return ok;
}

static int test_bio_get_mem(void)
{
    int ok = 0;
    BIO *bio = NULL;
    BUF_MEM *bufmem = NULL;

    bio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio))
        goto finish;
    if (!TEST_int_eq(BIO_puts(bio, "Hello World\n"), 12))
        goto finish;
    BIO_get_mem_ptr(bio, &bufmem);
    if (!TEST_ptr(bufmem))
        goto finish;
    if (!TEST_int_gt(BIO_set_close(bio, BIO_NOCLOSE), 0))
        goto finish;
    BIO_free(bio);
    bio = NULL;
    if (!TEST_mem_eq(bufmem->data, bufmem->length, "Hello World\n", 12))
        goto finish;
    ok = 1;

finish:
    BIO_free(bio);
    BUF_MEM_free(bufmem);
    return ok;
}

static int test_bio_new_mem_buf(void)
{
    int ok = 0;
    BIO *bio;
    BUF_MEM *bufmem;
    char data[16];

    bio = BIO_new_mem_buf("Hello World\n", 12);
    if (!TEST_ptr(bio))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio, data, 5), 5))
        goto finish;
    if (!TEST_mem_eq(data, 5, "Hello", 5))
        goto finish;
    if (!TEST_int_gt(BIO_get_mem_ptr(bio, &bufmem), 0))
        goto finish;
    if (!TEST_int_lt(BIO_write(bio, "test", 4), 0))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio, data, 16), 7))
        goto finish;
    if (!TEST_mem_eq(data, 7, " World\n", 7))
        goto finish;
    if (!TEST_int_gt(BIO_reset(bio), 0))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio, data, 16), 12))
        goto finish;
    if (!TEST_mem_eq(data, 12, "Hello World\n", 12))
        goto finish;
    ok = 1;

finish:
    BIO_free(bio);
    return ok;
}

static int test_bio_rdonly_mem_buf(void)
{
    int ok = 0;
    BIO *bio, *bio2 = NULL;
    BUF_MEM *bufmem;
    char data[16];

    bio = BIO_new_mem_buf("Hello World\n", 12);
    if (!TEST_ptr(bio))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio, data, 5), 5))
        goto finish;
    if (!TEST_mem_eq(data, 5, "Hello", 5))
        goto finish;
    if (!TEST_int_gt(BIO_get_mem_ptr(bio, &bufmem), 0))
        goto finish;
    (void)BIO_set_close(bio, BIO_NOCLOSE);

    bio2 = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio2))
        goto finish;
    BIO_set_mem_buf(bio2, bufmem, BIO_CLOSE);
    BIO_set_flags(bio2, BIO_FLAGS_MEM_RDONLY);

    if (!TEST_int_eq(BIO_read(bio2, data, 16), 7))
        goto finish;
    if (!TEST_mem_eq(data, 7, " World\n", 7))
        goto finish;
    if (!TEST_int_gt(BIO_reset(bio2), 0))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio2, data, 16), 7))
        goto finish;
    if (!TEST_mem_eq(data, 7, " World\n", 7))
        goto finish;
    ok = 1;

finish:
    BIO_free(bio);
    BIO_free(bio2);
    return ok;
}

static int test_bio_rdwr_rdonly(void)
{
    int ok = 0;
    BIO *bio = NULL;
    char data[16];

    bio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio))
        goto finish;
    if (!TEST_int_eq(BIO_puts(bio, "Hello World\n"), 12))
        goto finish;

    BIO_set_flags(bio, BIO_FLAGS_MEM_RDONLY);
    if (!TEST_int_eq(BIO_read(bio, data, 16), 12))
        goto finish;
    if (!TEST_mem_eq(data, 12, "Hello World\n", 12))
        goto finish;
    if (!TEST_int_gt(BIO_reset(bio), 0))
        goto finish;

    BIO_clear_flags(bio, BIO_FLAGS_MEM_RDONLY);
    if (!TEST_int_eq(BIO_puts(bio, "Hi!\n"), 4))
        goto finish;
    if (!TEST_int_eq(BIO_read(bio, data, 16), 16))
        goto finish;

    if (!TEST_mem_eq(data, 16, "Hello World\nHi!\n", 16))
        goto finish;

    ok = 1;

finish:
    BIO_free(bio);
    return ok;
}

static int test_bio_nonclear_rst(void)
{
    int ok = 0;
    BIO *bio = NULL;
    char data[16];

    bio = BIO_new(BIO_s_mem());
    if (!TEST_ptr(bio))
        goto finish;
    if (!TEST_int_eq(BIO_puts(bio, "Hello World\n"), 12))
        goto finish;

    BIO_set_flags(bio, BIO_FLAGS_NONCLEAR_RST);

    if (!TEST_int_eq(BIO_read(bio, data, 16), 12))
        goto finish;
    if (!TEST_mem_eq(data, 12, "Hello World\n", 12))
        goto finish;
    if (!TEST_int_gt(BIO_reset(bio), 0))
        goto finish;

    if (!TEST_int_eq(BIO_read(bio, data, 16), 12))
        goto finish;
    if (!TEST_mem_eq(data, 12, "Hello World\n", 12))
        goto finish;

    BIO_clear_flags(bio, BIO_FLAGS_NONCLEAR_RST);
    if (!TEST_int_gt(BIO_reset(bio), 0))
        goto finish;

    if (!TEST_int_lt(BIO_read(bio, data, 16), 1))
        goto finish;

    ok = 1;

finish:
    BIO_free(bio);
    return ok;
}

int setup_tests(void)
{
    ADD_TEST(test_bio_memleak);
    ADD_TEST(test_bio_get_mem);
    ADD_TEST(test_bio_new_mem_buf);
    ADD_TEST(test_bio_rdonly_mem_buf);
    ADD_TEST(test_bio_rdwr_rdonly);
    ADD_TEST(test_bio_nonclear_rst);
    return 1;
}
