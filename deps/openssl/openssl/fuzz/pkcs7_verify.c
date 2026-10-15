/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.openssl.org/source/license.html
 * or in the file LICENSE in the source distribution.
 */
#include <limits.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include "fuzzer.h"

int FuzzerInitialize(int *argc, char ***argv)
{
    return 1;
}

int FuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    BIO *indata = NULL;
    BIO *out = NULL;
    PKCS7 *p7 = NULL;
    const unsigned char *in;
    size_t consumed;
    size_t remaining;

    if (len > LONG_MAX)
        return 0;

    in = buf;
    p7 = d2i_PKCS7(NULL, &in, (long)len);
    if (p7 == NULL)
        goto err;

    consumed = (size_t)(in - buf);
    remaining = len - consumed;
    if (remaining > INT_MAX)
        goto err;

    if (consumed < len) {
        indata = BIO_new_mem_buf(in, (int)remaining);
        if (indata == NULL)
            goto err;
    }

    out = BIO_new(BIO_s_null());
    if (out == NULL)
        goto err;

    PKCS7_verify(p7, NULL, NULL, indata, out, PKCS7_NOVERIFY);

err:
    BIO_free(out);
    PKCS7_free(p7);
    BIO_free(indata);
    ERR_clear_error();
    return 0;
}

void FuzzerCleanup(void)
{
}
