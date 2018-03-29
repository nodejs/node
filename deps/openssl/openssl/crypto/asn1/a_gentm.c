/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * GENERALIZEDTIME implementation. Based on UTCTIME
 */

#include <stdio.h>
#include <time.h>
#include "internal/cryptlib.h"
#include <openssl/asn1.h>
#include "asn1_locl.h"

int asn1_generalizedtime_to_tm(struct tm *tm, const ASN1_GENERALIZEDTIME *d)
{
    static const int min[9] = { 0, 0, 1, 1, 0, 0, 0, 0, 0 };
    static const int max[9] = { 99, 99, 12, 31, 23, 59, 59, 12, 59 };
    char *a;
    int n, i, l, o;

    if (d->type != V_ASN1_GENERALIZEDTIME)
        return (0);
    l = d->length;
    a = (char *)d->data;
    o = 0;
    /*
     * GENERALIZEDTIME is similar to UTCTIME except the year is represented
     * as YYYY. This stuff treats everything as a two digit field so make
     * first two fields 00 to 99
     */
    if (l < 13)
        goto err;
    for (i = 0; i < 7; i++) {
        if ((i == 6) && ((a[o] == 'Z') || (a[o] == '+') || (a[o] == '-'))) {
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
                tm->tm_year = n * 100 - 1900;
                break;
            case 1:
                tm->tm_year += n;
                break;
            case 2:
                tm->tm_mon = n - 1;
                break;
            case 3:
                tm->tm_mday = n;
                break;
            case 4:
                tm->tm_hour = n;
                break;
            case 5:
                tm->tm_min = n;
                break;
            case 6:
                tm->tm_sec = n;
                break;
            }
        }
    }
    /*
     * Optional fractional seconds: decimal point followed by one or more
     * digits.
     */
    if (a[o] == '.') {
        if (++o > l)
            goto err;
        i = o;
        while ((a[o] >= '0') && (a[o] <= '9') && (o <= l))
            o++;
        /* Must have at least one digit after decimal point */
        if (i == o)
            goto err;
    }

    if (a[o] == 'Z')
        o++;
    else if ((a[o] == '+') || (a[o] == '-')) {
        int offsign = a[o] == '-' ? 1 : -1, offset = 0;
        o++;
        if (o + 4 > l)
            goto err;
        for (i = 7; i < 9; i++) {
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
                if (i == 7)
                    offset = n * 3600;
                else if (i == 8)
                    offset += n * 60;
            }
            o++;
        }
        if (offset && !OPENSSL_gmtime_adj(tm, 0, offset * offsign))
            return 0;
    } else if (a[o]) {
        /* Missing time zone information. */
        goto err;
    }
    return (o == l);
 err:
    return (0);
}

int ASN1_GENERALIZEDTIME_check(const ASN1_GENERALIZEDTIME *d)
{
    return asn1_generalizedtime_to_tm(NULL, d);
}

int ASN1_GENERALIZEDTIME_set_string(ASN1_GENERALIZEDTIME *s, const char *str)
{
    ASN1_GENERALIZEDTIME t;

    t.type = V_ASN1_GENERALIZEDTIME;
    t.length = strlen(str);
    t.data = (unsigned char *)str;
    if (ASN1_GENERALIZEDTIME_check(&t)) {
        if (s != NULL) {
            if (!ASN1_STRING_set((ASN1_STRING *)s, str, t.length))
                return 0;
            s->type = V_ASN1_GENERALIZEDTIME;
        }
        return (1);
    } else
        return (0);
}

ASN1_GENERALIZEDTIME *ASN1_GENERALIZEDTIME_set(ASN1_GENERALIZEDTIME *s,
                                               time_t t)
{
    return ASN1_GENERALIZEDTIME_adj(s, t, 0, 0);
}

ASN1_GENERALIZEDTIME *ASN1_GENERALIZEDTIME_adj(ASN1_GENERALIZEDTIME *s,
                                               time_t t, int offset_day,
                                               long offset_sec)
{
    char *p;
    struct tm *ts;
    struct tm data;
    size_t len = 20;
    ASN1_GENERALIZEDTIME *tmps = NULL;

    if (s == NULL)
        tmps = ASN1_GENERALIZEDTIME_new();
    else
        tmps = s;
    if (tmps == NULL)
        return NULL;

    ts = OPENSSL_gmtime(&t, &data);
    if (ts == NULL)
        goto err;

    if (offset_day || offset_sec) {
        if (!OPENSSL_gmtime_adj(ts, offset_day, offset_sec))
            goto err;
    }

    p = (char *)tmps->data;
    if ((p == NULL) || ((size_t)tmps->length < len)) {
        p = OPENSSL_malloc(len);
        if (p == NULL) {
            ASN1err(ASN1_F_ASN1_GENERALIZEDTIME_ADJ, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        OPENSSL_free(tmps->data);
        tmps->data = (unsigned char *)p;
    }

    BIO_snprintf(p, len, "%04d%02d%02d%02d%02d%02dZ", ts->tm_year + 1900,
                 ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min,
                 ts->tm_sec);
    tmps->length = strlen(p);
    tmps->type = V_ASN1_GENERALIZEDTIME;
#ifdef CHARSET_EBCDIC_not
    ebcdic2ascii(tmps->data, tmps->data, tmps->length);
#endif
    return tmps;
 err:
    if (s == NULL)
        ASN1_GENERALIZEDTIME_free(tmps);
    return NULL;
}

const char *_asn1_mon[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int ASN1_GENERALIZEDTIME_print(BIO *bp, const ASN1_GENERALIZEDTIME *tm)
{
    char *v;
    int gmt = 0;
    int i;
    int y = 0, M = 0, d = 0, h = 0, m = 0, s = 0;
    char *f = NULL;
    int f_len = 0;

    i = tm->length;
    v = (char *)tm->data;

    if (i < 12)
        goto err;
    if (v[i - 1] == 'Z')
        gmt = 1;
    for (i = 0; i < 12; i++)
        if ((v[i] > '9') || (v[i] < '0'))
            goto err;
    y = (v[0] - '0') * 1000 + (v[1] - '0') * 100
        + (v[2] - '0') * 10 + (v[3] - '0');
    M = (v[4] - '0') * 10 + (v[5] - '0');
    if ((M > 12) || (M < 1))
        goto err;
    d = (v[6] - '0') * 10 + (v[7] - '0');
    h = (v[8] - '0') * 10 + (v[9] - '0');
    m = (v[10] - '0') * 10 + (v[11] - '0');
    if (tm->length >= 14 &&
        (v[12] >= '0') && (v[12] <= '9') &&
        (v[13] >= '0') && (v[13] <= '9')) {
        s = (v[12] - '0') * 10 + (v[13] - '0');
        /* Check for fractions of seconds. */
        if (tm->length >= 15 && v[14] == '.') {
            int l = tm->length;
            f = &v[14];         /* The decimal point. */
            f_len = 1;
            while (14 + f_len < l && f[f_len] >= '0' && f[f_len] <= '9')
                ++f_len;
        }
    }

    if (BIO_printf(bp, "%s %2d %02d:%02d:%02d%.*s %d%s",
                   _asn1_mon[M - 1], d, h, m, s, f_len, f, y,
                   (gmt) ? " GMT" : "") <= 0)
        return (0);
    else
        return (1);
 err:
    BIO_write(bp, "Bad time value", 14);
    return (0);
}
