/*
 * Copyright 2017-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../testutil.h"
#include "output.h"
#include "tu_local.h"

#include <openssl/crypto.h>
#include <openssl/bio.h>

/* These are available for any test program */
BIO *bio_out = NULL;
BIO *bio_err = NULL;

/* These are available for TAP output only (internally) */
static BIO *tap_out = NULL;
static BIO *tap_err = NULL;

void test_open_streams(void)
{
    tap_out = BIO_new_fp(stdout, BIO_NOCLOSE | BIO_FP_TEXT);
    tap_err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);
#ifdef __VMS
    tap_out = BIO_push(BIO_new(BIO_f_linebuffer()), tap_out);
    tap_err = BIO_push(BIO_new(BIO_f_linebuffer()), tap_err);
#endif
    tap_out = BIO_push(BIO_new(BIO_f_prefix()), tap_out);
    tap_err = BIO_push(BIO_new(BIO_f_prefix()), tap_err);

    bio_out = BIO_push(BIO_new(BIO_f_prefix()), tap_out);
    bio_err = BIO_push(BIO_new(BIO_f_prefix()), tap_err);
    BIO_set_prefix(bio_out, "# ");
    BIO_set_prefix(bio_err, "# ");

    OPENSSL_assert(bio_out != NULL);
    OPENSSL_assert(bio_err != NULL);
}

void test_adjust_streams_tap_level(int level)
{
    BIO_set_indent(tap_out, level);
    BIO_set_indent(tap_err, level);
}

void test_close_streams(void)
{
    /*
     * The rest of the chain is freed by the BIO_free_all() calls below, so
     * we only need to free the last one in the bio_out and bio_err chains.
     */
    BIO_free(bio_out);
    BIO_free(bio_err);

    BIO_free_all(tap_out);
    BIO_free_all(tap_err);
}

int test_vprintf_stdout(const char *fmt, va_list ap)
{
    return BIO_vprintf(bio_out, fmt, ap);
}

int test_vprintf_stderr(const char *fmt, va_list ap)
{
    return BIO_vprintf(bio_err, fmt, ap);
}

int test_flush_stdout(void)
{
    return BIO_flush(bio_out);
}

int test_flush_stderr(void)
{
    return BIO_flush(bio_err);
}

int test_vprintf_tapout(const char *fmt, va_list ap)
{
    return BIO_vprintf(tap_out, fmt, ap);
}

int test_vprintf_taperr(const char *fmt, va_list ap)
{
    return BIO_vprintf(tap_err, fmt, ap);
}

int test_flush_tapout(void)
{
    return BIO_flush(tap_out);
}

int test_flush_taperr(void)
{
    return BIO_flush(tap_err);
}
