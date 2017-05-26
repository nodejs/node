/* crypto/asn1/a_time.c */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/*-
 * This is an implementation of the ASN1 Time structure which is:
 *    Time ::= CHOICE {
 *      utcTime        UTCTime,
 *      generalTime    GeneralizedTime }
 * written by Steve Henson.
 */

#include <stdio.h>
#include <time.h>
#include "cryptlib.h"
#include "o_time.h"
#include <openssl/asn1t.h>
#include "asn1_locl.h"

IMPLEMENT_ASN1_MSTRING(ASN1_TIME, B_ASN1_TIME)

IMPLEMENT_ASN1_FUNCTIONS(ASN1_TIME)

#if 0
int i2d_ASN1_TIME(ASN1_TIME *a, unsigned char **pp)
{
# ifdef CHARSET_EBCDIC
    /* KLUDGE! We convert to ascii before writing DER */
    char tmp[24];
    ASN1_STRING tmpstr;

    if (a->type == V_ASN1_UTCTIME || a->type == V_ASN1_GENERALIZEDTIME) {
        int len;

        tmpstr = *(ASN1_STRING *)a;
        len = tmpstr.length;
        ebcdic2ascii(tmp, tmpstr.data,
                     (len >= sizeof tmp) ? sizeof tmp : len);
        tmpstr.data = tmp;
        a = (ASN1_GENERALIZEDTIME *)&tmpstr;
    }
# endif
    if (a->type == V_ASN1_UTCTIME || a->type == V_ASN1_GENERALIZEDTIME)
        return (i2d_ASN1_bytes((ASN1_STRING *)a, pp,
                               a->type, V_ASN1_UNIVERSAL));
    ASN1err(ASN1_F_I2D_ASN1_TIME, ASN1_R_EXPECTING_A_TIME);
    return -1;
}
#endif

ASN1_TIME *ASN1_TIME_set(ASN1_TIME *s, time_t t)
{
    return ASN1_TIME_adj(s, t, 0, 0);
}

ASN1_TIME *ASN1_TIME_adj(ASN1_TIME *s, time_t t,
                         int offset_day, long offset_sec)
{
    struct tm *ts;
    struct tm data;

    ts = OPENSSL_gmtime(&t, &data);
    if (ts == NULL) {
        ASN1err(ASN1_F_ASN1_TIME_ADJ, ASN1_R_ERROR_GETTING_TIME);
        return NULL;
    }
    if (offset_day || offset_sec) {
        if (!OPENSSL_gmtime_adj(ts, offset_day, offset_sec))
            return NULL;
    }
    if ((ts->tm_year >= 50) && (ts->tm_year < 150))
        return ASN1_UTCTIME_adj(s, t, offset_day, offset_sec);
    return ASN1_GENERALIZEDTIME_adj(s, t, offset_day, offset_sec);
}

int ASN1_TIME_check(ASN1_TIME *t)
{
    if (t->type == V_ASN1_GENERALIZEDTIME)
        return ASN1_GENERALIZEDTIME_check(t);
    else if (t->type == V_ASN1_UTCTIME)
        return ASN1_UTCTIME_check(t);
    return 0;
}

/* Convert an ASN1_TIME structure to GeneralizedTime */
ASN1_GENERALIZEDTIME *ASN1_TIME_to_generalizedtime(ASN1_TIME *t,
                                                   ASN1_GENERALIZEDTIME **out)
{
    ASN1_GENERALIZEDTIME *ret = NULL;
    char *str;
    int newlen;

    if (!ASN1_TIME_check(t))
        return NULL;

    if (!out || !*out) {
        if (!(ret = ASN1_GENERALIZEDTIME_new()))
            goto err;
    } else {
        ret = *out;
    }

    /* If already GeneralizedTime just copy across */
    if (t->type == V_ASN1_GENERALIZEDTIME) {
        if (!ASN1_STRING_set(ret, t->data, t->length))
            goto err;
        goto done;
    }

    /* grow the string */
    if (!ASN1_STRING_set(ret, NULL, t->length + 2))
        goto err;
    /* ASN1_STRING_set() allocated 'len + 1' bytes. */
    newlen = t->length + 2 + 1;
    str = (char *)ret->data;
    /* Work out the century and prepend */
    if (t->data[0] >= '5')
        BUF_strlcpy(str, "19", newlen);
    else
        BUF_strlcpy(str, "20", newlen);

    BUF_strlcat(str, (char *)t->data, newlen);

 done:
   if (out != NULL && *out == NULL)
       *out = ret;
   return ret;

 err:
    if (out == NULL || *out != ret)
        ASN1_GENERALIZEDTIME_free(ret);
    return NULL;
}


int ASN1_TIME_set_string(ASN1_TIME *s, const char *str)
{
    ASN1_TIME t;

    t.length = strlen(str);
    t.data = (unsigned char *)str;
    t.flags = 0;

    t.type = V_ASN1_UTCTIME;

    if (!ASN1_TIME_check(&t)) {
        t.type = V_ASN1_GENERALIZEDTIME;
        if (!ASN1_TIME_check(&t))
            return 0;
    }

    if (s && !ASN1_STRING_copy((ASN1_STRING *)s, (ASN1_STRING *)&t))
        return 0;

    return 1;
}

static int asn1_time_to_tm(struct tm *tm, const ASN1_TIME *t)
{
    if (t == NULL) {
        time_t now_t;
        time(&now_t);
        if (OPENSSL_gmtime(&now_t, tm))
            return 1;
        return 0;
    }

    if (t->type == V_ASN1_UTCTIME)
        return asn1_utctime_to_tm(tm, t);
    else if (t->type == V_ASN1_GENERALIZEDTIME)
        return asn1_generalizedtime_to_tm(tm, t);

    return 0;
}

int ASN1_TIME_diff(int *pday, int *psec,
                   const ASN1_TIME *from, const ASN1_TIME *to)
{
    struct tm tm_from, tm_to;
    if (!asn1_time_to_tm(&tm_from, from))
        return 0;
    if (!asn1_time_to_tm(&tm_to, to))
        return 0;
    return OPENSSL_gmtime_diff(pday, psec, &tm_from, &tm_to);
}
