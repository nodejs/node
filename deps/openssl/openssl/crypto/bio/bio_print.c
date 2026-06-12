/*
 * Copyright 1995-2025 The OpenSSL Project Authors. All Rights Reserved.
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

#if defined(_WIN32)
/*
 * _MSC_VER described here:
 * https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-170
 *
 * Beginning with the UCRT in Visual Studio 2015 and Windows 10, snprintf is no
 * longer identical to _snprintf. The snprintf behavior is now C99 standard
 * conformant. The difference is that if you run out of buffer, snprintf
 * null-terminates the end of the buffer and returns the number of characters
 * that would have been required whereas _snprintf doesn't null-terminate the
 * buffer and returns -1. Also, snprintf() includes one more character in the
 * output because it doesn't null-terminate the buffer.
 * [ https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/snprintf-snprintf-snprintf-l-snwprintf-snwprintf-l?view=msvc-170#remarks
 *
 * for older MSVC (older than 2015) we can use _vscprintf() and _vsnprintf()
 * as suggested here:
 * https://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010
 *
 */
static int msvc_bio_vprintf(BIO *bio, const char *format, va_list args)
{
    char buf[512];
    char *abuf;
    int ret, sz;

    sz = _vsnprintf_s(buf, sizeof (buf), _TRUNCATE, format, args);
    if (sz == -1) {
        sz = _vscprintf(format, args) + 1;
        abuf = (char *) OPENSSL_malloc(sz);
        if (abuf == NULL) {
            ret = -1;
        } else {
            sz = _vsnprintf(abuf, sz, format, args);
            ret = BIO_write(bio, abuf, sz);
            OPENSSL_free(abuf);
        }
    } else {
        ret = BIO_write(bio, buf, sz);
    }

    return ret;
}

/*
 * This function is for unit test on windows only when built with Visual Studio
 */
int ossl_BIO_snprintf_msvc(char *buf, size_t n, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = _vsnprintf_s(buf, n, _TRUNCATE, format, args);
    va_end(args);

    return ret;
}

#endif

int BIO_vprintf(BIO *bio, const char *format, va_list args)
{
    va_list cp_args;
#if !defined(_MSC_VER) || _MSC_VER > 1900
    int sz;
#endif
    int ret = -1;

    va_copy(cp_args, args);
#if defined(_MSC_VER) && _MSC_VER < 1900
    ret = msvc_bio_vprintf(bio, format, cp_args);
#else
    char buf[512];
    char *abuf;
    /*
     * some compilers modify va_list, hence each call to v*printf()
     * should operate with its own instance of va_list. The first
     * call to vsnprintf() here uses args we got in function argument.
     * The second call is going to use cp_args we made earlier.
     */
    sz = vsnprintf(buf, sizeof (buf), format, args);
    if (sz >= 0) {
        if ((size_t)sz > sizeof (buf)) {
            sz += 1;
            abuf = (char *) OPENSSL_malloc(sz);
            if (abuf == NULL) {
                ret = -1;
            } else {
                sz = vsnprintf(abuf, sz, format, cp_args);
                ret = BIO_write(bio, abuf, sz);
                OPENSSL_free(abuf);
            }
        } else {
             /* vsnprintf returns length not including nul-terminator */
             ret = BIO_write(bio, buf, sz);
        }
    }
#endif
    va_end(cp_args);
    return ret;
}

/*
 * For historical reasons BIO_snprintf and friends return a failure for string
 * truncation (-1) instead of the POSIX requirement of a success with the
 * number of characters that would have been written. Upon seeing -1 on
 * return, the caller must treat output buf as unsafe (as a buf with missing
 * nul terminator).
 */
int BIO_snprintf(char *buf, size_t n, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);

#if defined(_MSC_VER) && _MSC_VER < 1900
    ret = _vsnprintf_s(buf, n, _TRUNCATE, format, args);
#else
    ret = vsnprintf(buf, n, format, args);
    if ((size_t)ret >= n)
        ret = -1;
#endif
    va_end(args);

    return ret;
}

int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
    int ret;

#if defined(_MSC_VER) && _MSC_VER < 1900
    ret = _vsnprintf_s(buf, n, _TRUNCATE, format, args);
#else
    ret = vsnprintf(buf, n, format, args);
    if ((size_t)ret >= n)
        ret = -1;
#endif
    return ret;
}
