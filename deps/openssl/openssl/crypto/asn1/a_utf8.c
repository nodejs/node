/* crypto/asn1/a_utf8.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1.h>

/* UTF8 utilities */

/*-
 * This parses a UTF8 string one character at a time. It is passed a pointer
 * to the string and the length of the string. It sets 'value' to the value of
 * the current character. It returns the number of characters read or a
 * negative error code:
 * -1 = string too short
 * -2 = illegal character
 * -3 = subsequent characters not of the form 10xxxxxx
 * -4 = character encoded incorrectly (not minimal length).
 */

int UTF8_getc(const unsigned char *str, int len, unsigned long *val)
{
    const unsigned char *p;
    unsigned long value;
    int ret;
    if (len <= 0)
        return 0;
    p = str;

    /* Check syntax and work out the encoded value (if correct) */
    if ((*p & 0x80) == 0) {
        value = *p++ & 0x7f;
        ret = 1;
    } else if ((*p & 0xe0) == 0xc0) {
        if (len < 2)
            return -1;
        if ((p[1] & 0xc0) != 0x80)
            return -3;
        value = (*p++ & 0x1f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x80)
            return -4;
        ret = 2;
    } else if ((*p & 0xf0) == 0xe0) {
        if (len < 3)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80))
            return -3;
        value = (*p++ & 0xf) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x800)
            return -4;
        ret = 3;
    } else if ((*p & 0xf8) == 0xf0) {
        if (len < 4)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x7)) << 18;
        value |= (*p++ & 0x3f) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x10000)
            return -4;
        ret = 4;
    } else if ((*p & 0xfc) == 0xf8) {
        if (len < 5)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80)
            || ((p[4] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x3)) << 24;
        value |= ((unsigned long)(*p++ & 0x3f)) << 18;
        value |= ((unsigned long)(*p++ & 0x3f)) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x200000)
            return -4;
        ret = 5;
    } else if ((*p & 0xfe) == 0xfc) {
        if (len < 6)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80)
            || ((p[4] & 0xc0) != 0x80)
            || ((p[5] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x1)) << 30;
        value |= ((unsigned long)(*p++ & 0x3f)) << 24;
        value |= ((unsigned long)(*p++ & 0x3f)) << 18;
        value |= ((unsigned long)(*p++ & 0x3f)) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x4000000)
            return -4;
        ret = 6;
    } else
        return -2;
    *val = value;
    return ret;
}

/*
 * This takes a character 'value' and writes the UTF8 encoded value in 'str'
 * where 'str' is a buffer containing 'len' characters. Returns the number of
 * characters written or -1 if 'len' is too small. 'str' can be set to NULL
 * in which case it just returns the number of characters. It will need at
 * most 6 characters.
 */

int UTF8_putc(unsigned char *str, int len, unsigned long value)
{
    if (!str)
        len = 6;                /* Maximum we will need */
    else if (len <= 0)
        return -1;
    if (value < 0x80) {
        if (str)
            *str = (unsigned char)value;
        return 1;
    }
    if (value < 0x800) {
        if (len < 2)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 6) & 0x1f) | 0xc0);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 2;
    }
    if (value < 0x10000) {
        if (len < 3)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 12) & 0xf) | 0xe0);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 3;
    }
    if (value < 0x200000) {
        if (len < 4)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 18) & 0x7) | 0xf0);
            *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 4;
    }
    if (value < 0x4000000) {
        if (len < 5)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 24) & 0x3) | 0xf8);
            *str++ = (unsigned char)(((value >> 18) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 5;
    }
    if (len < 6)
        return -1;
    if (str) {
        *str++ = (unsigned char)(((value >> 30) & 0x1) | 0xfc);
        *str++ = (unsigned char)(((value >> 24) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 18) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
        *str = (unsigned char)((value & 0x3f) | 0x80);
    }
    return 6;
}
