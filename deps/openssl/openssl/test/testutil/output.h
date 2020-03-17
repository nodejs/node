/*
 * Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_TESTUTIL_OUTPUT_H
# define OSSL_TESTUTIL_OUTPUT_H

#include <stdarg.h>

/*
 * The basic I/O functions used internally by the test framework.  These
 * can be overridden when needed. Note that if one is, then all must be.
 */
void test_open_streams(void);
void test_close_streams(void);
/* The following ALL return the number of characters written */
int test_vprintf_stdout(const char *fmt, va_list ap);
int test_vprintf_stderr(const char *fmt, va_list ap);
/* These return failure or success */
int test_flush_stdout(void);
int test_flush_stderr(void);

/* Commodity functions.  There's no need to override these */
int test_printf_stdout(const char *fmt, ...);
int test_printf_stderr(const char *fmt, ...);

#endif                          /* OSSL_TESTUTIL_OUTPUT_H */
