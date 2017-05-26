/* crypto/asn1/a_utctm.c */
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
#include <time.h>
#include "cryptlib.h"
#include "o_time.h"
#include <openssl/asn1.h>
#include "asn1_locl.h"

#if 0
int i2d_ASN1_UTCTIME(ASN1_UTCTIME *a, unsigned char **pp)
{
# ifndef CHARSET_EBCDIC
    return (i2d_ASN1_bytes((ASN1_STRING *)a, pp,
                           V_ASN1_UTCTIME, V_ASN1_UNIVERSAL));
# else
    /* KLUDGE! We convert to ascii before writing DER */
    int len;
    char tmp[24];
    ASN1_STRING x = *(ASN1_STRING *)a;

    len = x.length;
    ebcdic2ascii(tmp, x.data, (len >= sizeof tmp) ? sizeof tmp : len);
    x.data = tmp;
    return i2d_ASN1_bytes(&x, pp, V_ASN1_UTCTIME, V_ASN1_UNIVERSAL);
# endif
}

ASN1_UTCTIME *d2i_ASN1_UTCTIME(ASN1_UTCTIME **a, unsigned char **pp,
                               long length)
{
    ASN1_UTCTIME *ret = NULL;

    ret = (ASN1_UTCTIME *)d2i_ASN1_bytes((ASN1_STRING **)a, pp, length,
                                         V_ASN1_UTCTIME, V_ASN1_UNIVERSAL);
    if (ret == NULL) {
        ASN1err(ASN1_F_D2I_ASN1_UTCTIME, ERR_R_NESTED_ASN1_ERROR);
        return (NULL);
    }
# ifdef CHARSET_EBCDIC
    ascii2ebcdic(ret->data, ret->data, ret->length);
# endif
    if (!ASN1_UTCTIME_check(ret)) {
        ASN1err(ASN1_F_D2I_ASN1_UTCTIME, ASN1_R_INVALID_TIME_FORMAT);
        goto err;
    }

    return (ret);
 err:
    if ((ret != NULL) && ((a == NULL) || (*a != ret)))
        M_ASN1_UTCTIME_free(ret);
    return (NULL);
}

#endif

int asn1_utctime_to_tm(struct tm *tm, const ASN1_UTCTIME *d)
{
    static const int min[8] = { 0, 1, 1, 0, 0, 0, 0, 0 };
    static const int max[8] = { 99, 12, 31, 23, 59, 59, 12, 59 };
    char *a;
    int n, i, l, o;

    if (d->type != V_ASN1_UTCTIME)
        return (0);
    l = d->length;
    a = (char *)d->data;
    o = 0;

    if (l < 11)
        goto err;
    for (i = 0; i < 6; i++) {
        if ((i == 5) && ((a[o] == 'Z') || (a[o] == '+') || (a[o] == '-'))) {
            i++;
            if (tm)
                tm->tm_sec = 0;
            break;
        }
        if ((a[o] < '0') || (a[o] > '9'))
            goto err;
        n = a[o] - '0';
        if (++o > l)
            goto err;

        if ((a[o] < '0') || (a[o] > '9'))
            goto err;
        n = (n * 10) + a[o] - '0';
        if (++o > l)
            goto err;

        if ((n < min[i]) || (n > max[i]))
            goto err;
        if (tm) {
            switch (i) {
            case 0:
                tm->tm_year = n < 50 ? n + 100 : n;
                break;
            case 1:
                tm->tm_mon = n - 1;
                break;
            case 2:
                tm->tm_mday = n;
                break;
            case 3:
                tm->tm_hour = n;
                break;
            case 4:
                tm->tm_min = n;
                break;
            case 5:
                tm->tm_sec = n;
                break;
            }
        }
    }
    if (a[o] == 'Z')
        o++;
    else if ((a[o] == '+') || (a[o] == '-')) {
        int offsign = a[o] == '-' ? 1 : -1, offset = 0;
        o++;
        if (o + 4 > l)
            goto err;
        for (i = 6; i < 8; i++) {
            if ((a[o] < '0') || (a[o] > '9'))
                goto err;
            n = a[o] - '0';
            o++;
            if ((a[o] < '0') || (a[o] > '9'))
                goto err;
            n = (n * 10) + a[o] - '0';
            if ((n < min[i]) || (n > max[i]))
                goto err;
            if (tm) {
                if (i == 6)
                    offset = n * 3600;
                else if (i == 7)
                    offset += n * 60;
            }
            o++;
        }
        if (offset && !OPENSSL_gmtime_adj(tm, 0, offset * offsign))
            return 0;
    }
    return o == l;
 err:
    return 0;
}

int ASN1_UTCTIME_check(const ASN1_UTCTIME *d)
{
    return asn1_utctime_to_tm(NULL, d);
}

int ASN1_UTCTIME_set_string(ASN1_UTCTIME *s, const char *str)
{
    ASN1_UTCTIME t;

    t.type = V_ASN1_UTCTIME;
    t.length = strlen(str);
    t.data = (unsigned char *)str;
    if (ASN1_UTCTIME_check(&t)) {
        if (s != NULL) {
            if (!ASN1_STRING_set((ASN1_STRING *)s,
                                 (unsigned char *)str, t.length))
                return 0;
            s->type = V_ASN1_UTCTIME;
        }
        return (1);
    } else
        return (0);
}

ASN1_UTCTIME *ASN1_UTCTIME_set(ASN1_UTCTIME *s, time_t t)
{
    return ASN1_UTCTIME_adj(s, t, 0, 0);
}

ASN1_UTCTIME *ASN1_UTCTIME_adj(ASN1_UTCTIME *s, time_t t,
                               int offset_day, long offset_sec)
{
    char *p;
    struct tm *ts;
    struct tm data;
    size_t len = 20;
    int free_s = 0;

    if (s == NULL) {
        free_s = 1;
        s = M_ASN1_UTCTIME_new();
    }
    if (s == NULL)
        goto err;

    ts = OPENSSL_gmtime(&t, &data);
    if (ts == NULL)
        goto err;

    if (offset_day || offset_sec) {
        if (!OPENSSL_gmtime_adj(ts, offset_day, offset_sec))
            goto err;
    }

    if ((ts->tm_year < 50) || (ts->tm_year >= 150))
        goto err;

    p = (char *)s->data;
    if ((p == NULL) || ((size_t)s->length < len)) {
        p = OPENSSL_malloc(len);
        if (p == NULL) {
            ASN1err(ASN1_F_ASN1_UTCTIME_ADJ, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        if (s->data != NULL)
            OPENSSL_free(s->data);
        s->data = (unsigned char *)p;
    }

    BIO_snprintf(p, len, "%02d%02d%02d%02d%02d%02dZ", ts->tm_year % 100,
                 ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min,
                 ts->tm_sec);
    s->length = strlen(p);
    s->type = V_ASN1_UTCTIME;
#ifdef CHARSET_EBCDIC_not
    ebcdic2ascii(s->data, s->data, s->length);
#endif
    return (s);
 err:
    if (free_s && s)
        M_ASN1_UTCTIME_free(s);
    return NULL;
}

int ASN1_UTCTIME_cmp_time_t(const ASN1_UTCTIME *s, time_t t)
{
    struct tm stm, ttm;
    int day, sec;

    if (!asn1_utctime_to_tm(&stm, s))
        return -2;

    if (!OPENSSL_gmtime(&t, &ttm))
        return -2;

    if (!OPENSSL_gmtime_diff(&day, &sec, &ttm, &stm))
        return -2;

    if (day > 0)
        return 1;
    if (day < 0)
        return -1;
    if (sec > 0)
        return 1;
    if (sec < 0)
        return -1;
    return 0;
}

#if 0
time_t ASN1_UTCTIME_get(const ASN1_UTCTIME *s)
{
    struct tm tm;
    int offset;

    memset(&tm, '\0', sizeof tm);

# define g2(p) (((p)[0]-'0')*10+(p)[1]-'0')
    tm.tm_year = g2(s->data);
    if (tm.tm_year < 50)
        tm.tm_year += 100;
    tm.tm_mon = g2(s->data + 2) - 1;
    tm.tm_mday = g2(s->data + 4);
    tm.tm_hour = g2(s->data + 6);
    tm.tm_min = g2(s->data + 8);
    tm.tm_sec = g2(s->data + 10);
    if (s->data[12] == 'Z')
        offset = 0;
    else {
        offset = g2(s->data + 13) * 60 + g2(s->data + 15);
        if (s->data[12] == '-')
            offset = -offset;
    }
# undef g2

    /*
     * FIXME: mktime assumes the current timezone
     * instead of UTC, and unless we rewrite OpenSSL
     * in Lisp we cannot locally change the timezone
     * without possibly interfering with other parts
     * of the program. timegm, which uses UTC, is
     * non-standard.
     * Also time_t is inappropriate for general
     * UTC times because it may a 32 bit type.
     */
    return mktime(&tm) - offset * 60;
}
#endif
