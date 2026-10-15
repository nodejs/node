/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * C99 snprintf and vsnprintf emulation for MSVC versions earlier than
 * Visual Studio 2015 (_MSC_VER < 1900).  Those compilers ship _snprintf
 * and _vsnprintf with non-C99 semantics (return -1 on truncation, no
 * guaranteed NUL termination) and do not provide the standard names at
 * all.  This file supplies the missing C99 names.
 *
 * This file is only compiled when the configured target is one of the
 * Windows MSVC 2013 compatibility variants; on every other platform
 * the standard library already provides these symbols.
 *
 * IMPORTANT: This translation unit MUST define snprintf and vsnprintf
 * and nothing else.  This single file is compiled directly into libcrypto,
 * libssl, and the apps; each references it from its own build.info rather
 * than keeping a copy.  Because each definition lives in its own .obj, the
 * linker's archive-search rule ensures only one copy is pulled into any
 * final binary.  Defining any additional symbol here would risk pulling
 * multiple copies and producing a duplicate-symbol link error.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>

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
int msvc_translate_printf_format(const char *format, const char **out,
    char **tmp)
{
    /* Valid printf conversion specifiers, grouped by category: signed
     * integers (d i), unsigned (o u x X), floating-point (f F e E g G a A),
     * misc (c s p n) and MSVC-specific (S Z C). */
    static const char conv[] = "diouxXfFeEgGaAcspnSZC";
    const char *p = format;
    char *dst = NULL, *q = NULL;

    /*
     * The VS 2013 CRT does not understand the C99 z, t and j length
     * modifiers. Translate z and t to I (both are pointer-sized on Windows)
     * and j to I64 (intmax_t is 64 bits). Every input character expands to
     * at most three output characters (j -> I64), so 3 * length is a safe
     * bound for the buffer.
     *
     * This is done in a single pass: nothing is allocated until the first
     * modifier is seen, so formats that need no translation return the
     * original string untouched. EMIT_CHAR() appends a character to the
     * output once the buffer exists; before that it is a no-op.
     */
#define EMIT_CHAR(c)     \
    do {                 \
        if (dst != NULL) \
            *q++ = (c);  \
    } while (0)

    *out = format;
    *tmp = NULL;

    while (*p != '\0') {
        if (*p != '%') { /* literal character */
            EMIT_CHAR(*p);
            p++;
            continue;
        }
        p++; /* consume '%' */
        if (*p == '%') { /* literal "%%" */
            EMIT_CHAR('%');
            EMIT_CHAR('%');
            p++;
            continue;
        }
        EMIT_CHAR('%');
        while (*p != '\0' && strchr(conv, *p) == NULL) {
            char c = *p++;
            if (c != 'z' && c != 't' && c != 'j') { /* verbatim */
                EMIT_CHAR(c);
                continue;
            }
            if (dst == NULL) { /* first modifier: allocate + flush prefix */
                size_t len = strlen(format);
                if (len > (SIZE_MAX - 1) / 3) /* make static analysis happy */
                    return 0;
                dst = (char *)OPENSSL_malloc(3 * len + 1);
                if (dst == NULL)
                    return 0;
                q = dst;
                memcpy(q, format, (size_t)(p - 1 - format));
                q += p - 1 - format;
            }
            EMIT_CHAR('I');
            if (c == 'j') {
                EMIT_CHAR('6');
                EMIT_CHAR('4');
            }
        }
        if (*p != '\0') { /* copy the conversion specifier */
            EMIT_CHAR(*p);
            p++;
        }
    }
#undef EMIT_CHAR

    if (dst != NULL) {
        *q = '\0';
        *out = dst;
        *tmp = dst;
    }
    return 1;
}

int vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
    int count = -1;
    va_list args_copy;
    char *fmt_alloc = NULL;
    const char *fmt;

    if (!msvc_translate_printf_format(format, &fmt, &fmt_alloc))
        goto done;
    va_copy(args_copy, args);
    count = _vscprintf(fmt, args_copy);
    va_end(args_copy);

    if (count < 0)
        goto done;

    if (n > 0)
        (void)_vsnprintf_s(buf, n, _TRUNCATE, fmt, args);

done:
    OPENSSL_free(fmt_alloc);
    return count;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, n, fmt, args);
    va_end(args);
    return ret;
}
