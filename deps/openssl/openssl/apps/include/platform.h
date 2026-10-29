/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_APPS_PLATFORM_H
#define OSSL_APPS_PLATFORM_H

#include <openssl/e_os2.h>

#if defined(OPENSSL_SYS_VMS) && defined(__DECC)
/*
 * VMS C only for now, implemented in vms_decc_init.c
 * If other C compilers forget to terminate argv with NULL, this function
 * can be reused.
 */
char **copy_argv(int *argc, char *argv[]);
#endif

#ifdef _WIN32
/*
 * Win32-specific argv initialization that splits OS-supplied UNICODE
 * command line string to array of UTF8-encoded strings.
 */
void win32_utf8argv(int *argc, char **argv[]);
#endif

/*
 * MSVC versions earlier than Visual Studio 2015 (_MSC_VER < 1900) do not
 * declare or define C99 snprintf or vsnprintf.  Definitions are supplied
 * by crypto/msvc2013_snprintf.c, which is built only on the matching
 * Configure target variants.
 */
#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdarg.h>
int msvc_translate_printf_format(const char *format, const char **out,
    char **tmp);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list args);
#endif

#endif
