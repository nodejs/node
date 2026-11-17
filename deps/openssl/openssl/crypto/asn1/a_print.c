/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "crypto/ctype.h"
#include "internal/cryptlib.h"
#include <openssl/asn1.h>

#include <crypto/asn1.h>

/**
 * @brief Determine the narrowest ASN.1 string type that can represent bytes.
 *
 * Unlike ASN1_PRINTABLE_type() this requires an explicit length and has no
 * legacy path that treats its input as a NUL terminated C string, so it may
 * be called on ASN1_STRING data.
 *
 * @param s pointer to the bytes to classify
 * @param len the number of bytes available at s
 * @returns V_ASN1_T61STRING, V_ASN1_IA5STRING or V_ASN1_PRINTABLESTRING
 */
static int printable_type(const unsigned char *s, size_t len)
{
    int ia5 = 0;
    int t61 = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        int c = s[i];

        if (!ossl_isasn1print(c))
            ia5 = 1;
        if (!ossl_isascii(c))
            t61 = 1;
    }
    if (t61)
        return V_ASN1_T61STRING;
    if (ia5)
        return V_ASN1_IA5STRING;
    return V_ASN1_PRINTABLESTRING;
}

int ASN1_PRINTABLE_type(const unsigned char *s, int len)
{
    if (s == NULL)
        return V_ASN1_PRINTABLESTRING;

    if (len < 0)
        len = (int)strlen((const char *)s);

    return printable_type(s, (size_t)len);
}

int ASN1_UNIVERSALSTRING_to_string(ASN1_UNIVERSALSTRING *s)
{
    int i;
    unsigned char *p;

    if (s->type != V_ASN1_UNIVERSALSTRING)
        return 0;
    if (s->length < 0 || (s->length % 4) != 0)
        return 0;
    p = s->data;
    for (i = 0; i < s->length; i += 4) {
        if ((p[0] != '\0') || (p[1] != '\0') || (p[2] != '\0'))
            break;
        else
            p += 4;
    }
    if (i < s->length)
        return 0;
    p = s->data;
    for (i = 3; i < s->length; i += 4) {
        *(p++) = s->data[i];
    }
    if (s->length > 0)
        *p = '\0';
    s->length /= 4;
    s->type = printable_type(s->data, (size_t)s->length);
    return 1;
}

int ASN1_STRING_print(BIO *bp, const ASN1_STRING *v)
{
    int i, n;
    char buf[80];
    const char *p;

    if (v == NULL)
        return 0;
    n = 0;
    p = (const char *)v->data;
    for (i = 0; i < v->length; i++) {
        if ((p[i] > '~') || ((p[i] < ' ') && (p[i] != '\n') && (p[i] != '\r')))
            buf[n] = '.';
        else
            buf[n] = p[i];
        n++;
        if (n >= 80) {
            if (BIO_write(bp, buf, n) <= 0)
                return 0;
            n = 0;
        }
    }
    if (n > 0)
        if (BIO_write(bp, buf, n) <= 0)
            return 0;
    return 1;
}
