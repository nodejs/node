/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include "internal/cryptlib.h"
#include "crypto/ctype.h"
#include "internal/numbers.h"
#include <openssl/bio.h>
#include <openssl/configuration.h>

int BIO_printf(BIO *bio, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);

    ret = BIO_vprintf(bio, format, args);

    va_end(args);
    return ret;
}

int BIO_vprintf(BIO *bio, const char *format, va_list args)
{
    va_list cp_args;
    int sz;
    int ret = -1;
    char buf[512];
    char *abuf;
#if defined(_MSC_VER) && _MSC_VER < 1900
    char *msvc_fmt_alloc = NULL;
#endif
    const char *fmt;

#if defined(_MSC_VER) && _MSC_VER < 1900
    /* Fix MSVC 2013 format to accept C99 format strings */
    if (!msvc_translate_printf_format(format, &fmt, &msvc_fmt_alloc))
        goto done;
#else
    fmt = format;
#endif

    va_copy(cp_args, args);

    /*
     * some compilers modify va_list, hence each call to v*printf()
     * should operate with its own instance of va_list. The first
     * call to vsnprintf() here uses args we got in function argument.
     * The second call is going to use cp_args we made earlier.
     */
    sz = vsnprintf(buf, sizeof(buf), fmt, args);
    if (sz >= 0) {
        if ((size_t)sz >= sizeof(buf)) {
            sz += 1;
            abuf = (char *)OPENSSL_malloc(sz);
            if (abuf == NULL) {
                ret = -1;
            } else {
                sz = vsnprintf(abuf, sz, fmt, cp_args);
                ret = BIO_write(bio, abuf, sz);
                OPENSSL_free(abuf);
            }
        } else {
            /* vsnprintf returns length not including nul-terminator */
            ret = BIO_write(bio, buf, sz);
        }
    }
    va_end(cp_args);
#if defined(_MSC_VER) && _MSC_VER < 1900
done:
    OPENSSL_free(msvc_fmt_alloc);
#endif
    return ret;
}

#ifndef OPENSSL_NO_DEPRECATED_4_1
/*
 * For historical reasons BIO_snprintf and friends return -1 on truncation
 * instead of the C99 snprintf semantic of returning the number of characters
 * that would have been written.  Deprecated in 4.1; new code should call
 * snprintf() / vsnprintf() directly.
 */
int BIO_snprintf(char *buf, size_t n, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);

    ret = vsnprintf(buf, n, format, args);
    if ((size_t)ret >= n)
        ret = -1;
    va_end(args);

    return ret;
}

int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
    int ret;

    ret = vsnprintf(buf, n, format, args);
    if ((size_t)ret >= n)
        ret = -1;

    return ret;
}
#endif /* OPENSSL_NO_DEPRECATED_4_1 */
