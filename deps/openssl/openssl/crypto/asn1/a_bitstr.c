/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <limits.h>
#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/asn1.h>
#include "asn1_local.h"

#include <crypto/asn1.h>

#ifndef OPENSSL_NO_DEPRECATED_4_1
int ASN1_BIT_STRING_set(ASN1_BIT_STRING *x, unsigned char *d, int len)
{
    return ossl_asn1_string_set_internal(x, d, len, /*add_nul_byte=*/0);
}
#endif

int ossl_i2c_ASN1_BIT_STRING(const ASN1_BIT_STRING *a, unsigned char **pp)
{
    int ret = 0, bits = 0, len;
    unsigned char *p, *d;

    if (a == NULL)
        goto err;

    len = a->length;

    if (len > INT_MAX - 1 || len < 0)
        goto err;

    if ((len > 0) && (a->flags & ASN1_STRING_FLAG_BITS_LEFT))
        bits = (int)a->flags & 0x07;

    if (pp == NULL)
        goto done;

    p = *pp;

    *(p++) = (unsigned char)bits;
    d = a->data;
    if (len > 0) {
        memcpy(p, d, len);
        p += len;
        p[-1] &= (0xff << bits);
    }
    *pp = p;

done:
    ret = len + 1;

err:
    return ret;
}

ASN1_BIT_STRING *ossl_c2i_ASN1_BIT_STRING(ASN1_BIT_STRING **a,
    const unsigned char **pp, long len)
{
    ASN1_BIT_STRING *ret = NULL;
    const unsigned char *p;
    unsigned char *s;
    int i = 0;

    if (len < 1) {
        i = ASN1_R_STRING_TOO_SHORT;
        goto err;
    }

    if (len > INT_MAX) {
        i = ASN1_R_STRING_TOO_LONG;
        goto err;
    }

    if ((a == NULL) || ((*a) == NULL)) {
        if ((ret = ASN1_BIT_STRING_new()) == NULL)
            return NULL;
    } else
        ret = (*a);

    p = *pp;
    i = *(p++);
    if (i > 7) {
        i = ASN1_R_INVALID_BIT_STRING_BITS_LEFT;
        goto err;
    }
    /*
     * We do this to preserve the settings.  If we modify the settings, via
     * the _set_bit function, we will recalculate on output
     */
    ossl_asn1_bit_string_set_unused_bits(ret, i);

    if (len-- > 1) { /* using one because of the bits left byte */
        s = OPENSSL_malloc((int)len);
        if (s == NULL) {
            goto err;
        }
        memcpy(s, p, (int)len);
        s[len - 1] &= (0xff << i);
        p += len;
    } else
        s = NULL;

    ASN1_STRING_set0(ret, s, (int)len);
    ret->type = V_ASN1_BIT_STRING;
    if (a != NULL)
        (*a) = ret;
    *pp = p;
    return ret;
err:
    if (i != 0)
        ERR_raise(ERR_LIB_ASN1, i);
    if ((a == NULL) || (*a != ret))
        ASN1_BIT_STRING_free(ret);
    return NULL;
}

/*
 * These next 2 functions from Goetz Babin-Ebell.
 */
int ASN1_BIT_STRING_set_bit(ASN1_BIT_STRING *a, int n, int value)
{
    int w, v, iv;
    unsigned char *c;

    if (n < 0)
        return 0;

    w = n / 8;
    v = 1 << (7 - (n & 0x07));
    iv = ~v;
    if (!value)
        v = 0;

    if (a == NULL)
        return 0;

    a->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07); /* clear, set on write */

    if ((a->length < (w + 1)) || (a->data == NULL)) {
        if (!value)
            return 1; /* Don't need to set */
        c = OPENSSL_clear_realloc(a->data, a->length, w + 1);
        if (c == NULL)
            return 0;
        if (w + 1 - a->length > 0)
            memset(c + a->length, 0, w + 1 - a->length);
        a->data = c;
        a->length = w + 1;
    }
    a->data[w] = ((a->data[w]) & iv) | v;

    while ((a->length > 0) && (a->data[a->length - 1] == 0))
        a->length--;

    if (a->length > 0) {
        uint8_t u8 = a->data[a->length - 1];
        uint8_t unused_bits = 7;

        /* Only keep least significant bit; count trailing zeroes. */
        u8 &= 0x100 - u8;
        if ((u8 & 0x0f) != 0)
            unused_bits -= 4;
        if ((u8 & 0x33) != 0)
            unused_bits -= 2;
        if ((u8 & 0x55) != 0)
            unused_bits -= 1;
        ossl_asn1_bit_string_set_unused_bits(a, unused_bits);
    }
    return 1;
}

int ASN1_BIT_STRING_get_bit(const ASN1_BIT_STRING *a, int n)
{
    int w, v;

    if (n < 0)
        return 0;

    w = n / 8;
    v = 1 << (7 - (n & 0x07));
    if ((a == NULL) || (a->length < (w + 1)) || (a->data == NULL))
        return 0;
    return ((a->data[w] & v) != 0);
}

/*
 * Checks if the given bit string contains only bits specified by
 * the flags vector. Returns 0 if there is at least one bit set in 'a'
 * which is not specified in 'flags', 1 otherwise.
 * 'len' is the length of 'flags'.
 */
int ASN1_BIT_STRING_check(const ASN1_BIT_STRING *a,
    const unsigned char *flags, int flags_len)
{
    int i, ok;
    /* Check if there is one bit set at all. */
    if (!a || !a->data)
        return 1;

    /*
     * Check each byte of the internal representation of the bit string.
     */
    ok = 1;
    for (i = 0; i < a->length && ok; ++i) {
        unsigned char mask = i < flags_len ? ~flags[i] : 0xff;
        /* We are done if there is an unneeded bit set. */
        ok = (a->data[i] & mask) == 0;
    }
    return ok;
}

int ASN1_BIT_STRING_get_length(const ASN1_BIT_STRING *abs, size_t *out_length,
    int *out_unused_bits)
{
    size_t length;
    int unused_bits;

    if (abs == NULL || abs->type != V_ASN1_BIT_STRING)
        return 0;

    if (out_length == NULL || out_unused_bits == NULL || abs->length < 0)
        return 0;

    length = abs->length;
    unused_bits = 0;

    if ((abs->flags & ASN1_STRING_FLAG_BITS_LEFT) != 0)
        unused_bits = abs->flags & 0x07;

    if (length == 0 && unused_bits != 0)
        return 0;

    if (unused_bits != 0) {
        unsigned char mask = (1 << unused_bits) - 1;
        if ((abs->data[length - 1] & mask) != 0)
            return 0;
    }

    *out_length = length;
    *out_unused_bits = unused_bits;

    return 1;
}

int ASN1_BIT_STRING_set1(ASN1_BIT_STRING *abs, const uint8_t *data, size_t length,
    int unused_bits)
{
    if (abs == NULL)
        return 0;

    if (length > INT_MAX || unused_bits < 0 || unused_bits > 7)
        return 0;

    if (length == 0 && unused_bits != 0)
        return 0;

    if (length > 0 && (data[length - 1] & ((1 << unused_bits) - 1)) != 0)
        return 0;

    if (!ossl_asn1_string_set_internal(abs, data, (int)length, /*add_nul_byte=*/0))
        return 0;

    abs->type = V_ASN1_BIT_STRING;

    ossl_asn1_bit_string_set_unused_bits(abs, unused_bits);

    return 1;
}
