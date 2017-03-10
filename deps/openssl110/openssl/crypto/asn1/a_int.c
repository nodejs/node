/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include "internal/numbers.h"
#include <limits.h>
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include "asn1_locl.h"

ASN1_INTEGER *ASN1_INTEGER_dup(const ASN1_INTEGER *x)
{
    return ASN1_STRING_dup(x);
}

int ASN1_INTEGER_cmp(const ASN1_INTEGER *x, const ASN1_INTEGER *y)
{
    int neg, ret;
    /* Compare signs */
    neg = x->type & V_ASN1_NEG;
    if (neg != (y->type & V_ASN1_NEG)) {
        if (neg)
            return -1;
        else
            return 1;
    }

    ret = ASN1_STRING_cmp(x, y);

    if (neg)
        return -ret;
    else
        return ret;
}

/*-
 * This converts a big endian buffer and sign into its content encoding.
 * This is used for INTEGER and ENUMERATED types.
 * The internal representation is an ASN1_STRING whose data is a big endian
 * representation of the value, ignoring the sign. The sign is determined by
 * the type: if type & V_ASN1_NEG is true it is negative, otherwise positive.
 *
 * Positive integers are no problem: they are almost the same as the DER
 * encoding, except if the first byte is >= 0x80 we need to add a zero pad.
 *
 * Negative integers are a bit trickier...
 * The DER representation of negative integers is in 2s complement form.
 * The internal form is converted by complementing each octet and finally
 * adding one to the result. This can be done less messily with a little trick.
 * If the internal form has trailing zeroes then they will become FF by the
 * complement and 0 by the add one (due to carry) so just copy as many trailing
 * zeros to the destination as there are in the source. The carry will add one
 * to the last none zero octet: so complement this octet and add one and finally
 * complement any left over until you get to the start of the string.
 *
 * Padding is a little trickier too. If the first bytes is > 0x80 then we pad
 * with 0xff. However if the first byte is 0x80 and one of the following bytes
 * is non-zero we pad with 0xff. The reason for this distinction is that 0x80
 * followed by optional zeros isn't padded.
 */

static size_t i2c_ibuf(const unsigned char *b, size_t blen, int neg,
                       unsigned char **pp)
{
    int pad = 0;
    size_t ret, i;
    unsigned char *p, pb = 0;
    const unsigned char *n;

    if (b == NULL || blen == 0)
        ret = 1;
    else {
        ret = blen;
        i = b[0];
        if (ret == 1 && i == 0)
            neg = 0;
        if (!neg && (i > 127)) {
            pad = 1;
            pb = 0;
        } else if (neg) {
            if (i > 128) {
                pad = 1;
                pb = 0xFF;
            } else if (i == 128) {
                /*
                 * Special case: if any other bytes non zero we pad:
                 * otherwise we don't.
                 */
                for (i = 1; i < blen; i++)
                    if (b[i]) {
                        pad = 1;
                        pb = 0xFF;
                        break;
                    }
            }
        }
        ret += pad;
    }
    if (pp == NULL)
        return ret;
    p = *pp;

    if (pad)
        *(p++) = pb;
    if (b == NULL || blen == 0)
        *p = 0;
    else if (!neg)
        memcpy(p, b, blen);
    else {
        /* Begin at the end of the encoding */
        n = b + blen;
        p += blen;
        i = blen;
        /* Copy zeros to destination as long as source is zero */
        while (!n[-1] && i > 1) {
            *(--p) = 0;
            n--;
            i--;
        }
        /* Complement and increment next octet */
        *(--p) = ((*(--n)) ^ 0xff) + 1;
        i--;
        /* Complement any octets left */
        for (; i > 0; i--)
            *(--p) = *(--n) ^ 0xff;
    }

    *pp += ret;
    return ret;
}

/*
 * convert content octets into a big endian buffer. Returns the length
 * of buffer or 0 on error: for malformed INTEGER. If output buffer is
 * NULL just return length.
 */

static size_t c2i_ibuf(unsigned char *b, int *pneg,
                       const unsigned char *p, size_t plen)
{
    size_t i;
    int neg, pad;
    /* Zero content length is illegal */
    if (plen == 0) {
        ASN1err(ASN1_F_C2I_IBUF, ASN1_R_ILLEGAL_ZERO_CONTENT);
        return 0;
    }
    neg = p[0] & 0x80;
    if (pneg)
        *pneg = neg;
    /* Handle common case where length is 1 octet separately */
    if (plen == 1) {
        if (b) {
            if (neg)
                b[0] = (p[0] ^ 0xFF) + 1;
            else
                b[0] = p[0];
        }
        return 1;
    }
    if (p[0] == 0 || p[0] == 0xFF)
        pad = 1;
    else
        pad = 0;
    /* reject illegal padding: first two octets MSB can't match */
    if (pad && (neg == (p[1] & 0x80))) {
        ASN1err(ASN1_F_C2I_IBUF, ASN1_R_ILLEGAL_PADDING);
        return 0;
    }
    /* If positive just copy across */
    if (neg == 0) {
        if (b)
            memcpy(b, p + pad, plen - pad);
        return plen - pad;
    }

    if (neg && pad) {
        /* check is any following octets are non zero */
        for (i = 1; i < plen; i++) {
            if (p[i] != 0)
                break;
        }
        /* if all bytes are zero handle as special case */
        if (i == plen) {
            if (b) {
                b[0] = 1;
                memset(b + 1, 0, plen - 1);
            }
            return plen;
        }
    }

    plen -= pad;
    /* Must be negative: calculate twos complement */
    if (b) {
        const unsigned char *from = p + plen - 1 + pad;
        unsigned char *to = b + plen;
        i = plen;
        while (*from == 0 && i) {
            *--to = 0;
            i--;
            from--;
        }
        *--to = (*from-- ^ 0xff) + 1;
        OPENSSL_assert(i != 0);
        i--;
        for (; i > 0; i--)
            *--to = *from-- ^ 0xff;
    }
    return plen;
}

int i2c_ASN1_INTEGER(ASN1_INTEGER *a, unsigned char **pp)
{
    return i2c_ibuf(a->data, a->length, a->type & V_ASN1_NEG, pp);
}

/* Convert big endian buffer into uint64_t, return 0 on error */
static int asn1_get_uint64(uint64_t *pr, const unsigned char *b, size_t blen)
{
    size_t i;
    if (blen > sizeof(*pr)) {
        ASN1err(ASN1_F_ASN1_GET_UINT64, ASN1_R_TOO_LARGE);
        return 0;
    }
    *pr = 0;
    if (b == NULL)
        return 0;
    for (i = 0; i < blen; i++) {
        *pr <<= 8;
        *pr |= b[i];
    }
    return 1;
}

static size_t asn1_put_uint64(unsigned char *b, uint64_t r)
{
    if (r >= 0x100) {
        unsigned char *p;
        uint64_t rtmp = r;
        size_t i = 0;

        /* Work out how many bytes we need */
        while (rtmp) {
            rtmp >>= 8;
            i++;
        }

        /* Copy from end to beginning */
        p = b + i - 1;

        do {
            *p-- = r & 0xFF;
            r >>= 8;
        } while (p >= b);

        return i;
    }

    b[0] = (unsigned char)r;
    return 1;

}

/*
 * Absolute value of INT64_MIN: we can't just use -INT64_MIN as it produces
 * overflow warnings.
 */

#define ABS_INT64_MIN \
    ((uint64_t)INT64_MAX + (uint64_t)(-(INT64_MIN + INT64_MAX)))

/* signed version of asn1_get_uint64 */
static int asn1_get_int64(int64_t *pr, const unsigned char *b, size_t blen,
                          int neg)
{
    uint64_t r;
    if (asn1_get_uint64(&r, b, blen) == 0)
        return 0;
    if (neg) {
        if (r > ABS_INT64_MIN) {
            ASN1err(ASN1_F_ASN1_GET_INT64, ASN1_R_TOO_SMALL);
            return 0;
        }
        *pr = 0 - (uint64_t)r;
    } else {
        if (r > INT64_MAX) {
            ASN1err(ASN1_F_ASN1_GET_INT64, ASN1_R_TOO_LARGE);
            return 0;
        }
        *pr = (int64_t)r;
    }
    return 1;
}

/* Convert ASN1 INTEGER content octets to ASN1_INTEGER structure */
ASN1_INTEGER *c2i_ASN1_INTEGER(ASN1_INTEGER **a, const unsigned char **pp,
                               long len)
{
    ASN1_INTEGER *ret = NULL;
    size_t r;
    int neg;

    r = c2i_ibuf(NULL, NULL, *pp, len);

    if (r == 0)
        return NULL;

    if ((a == NULL) || ((*a) == NULL)) {
        ret = ASN1_INTEGER_new();
        if (ret == NULL)
            return NULL;
        ret->type = V_ASN1_INTEGER;
    } else
        ret = *a;

    if (ASN1_STRING_set(ret, NULL, r) == 0)
        goto err;

    c2i_ibuf(ret->data, &neg, *pp, len);

    if (neg)
        ret->type |= V_ASN1_NEG;

    *pp += len;
    if (a != NULL)
        (*a) = ret;
    return ret;
 err:
    ASN1err(ASN1_F_C2I_ASN1_INTEGER, ERR_R_MALLOC_FAILURE);
    if ((a == NULL) || (*a != ret))
        ASN1_INTEGER_free(ret);
    return NULL;
}

static int asn1_string_get_int64(int64_t *pr, const ASN1_STRING *a, int itype)
{
    if (a == NULL) {
        ASN1err(ASN1_F_ASN1_STRING_GET_INT64, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if ((a->type & ~V_ASN1_NEG) != itype) {
        ASN1err(ASN1_F_ASN1_STRING_GET_INT64, ASN1_R_WRONG_INTEGER_TYPE);
        return 0;
    }
    return asn1_get_int64(pr, a->data, a->length, a->type & V_ASN1_NEG);
}

static int asn1_string_set_int64(ASN1_STRING *a, int64_t r, int itype)
{
    unsigned char tbuf[sizeof(r)];
    size_t l;
    a->type = itype;
    if (r < 0) {
        l = asn1_put_uint64(tbuf, -r);
        a->type |= V_ASN1_NEG;
    } else {
        l = asn1_put_uint64(tbuf, r);
        a->type &= ~V_ASN1_NEG;
    }
    if (l == 0)
        return 0;
    return ASN1_STRING_set(a, tbuf, l);
}

static int asn1_string_get_uint64(uint64_t *pr, const ASN1_STRING *a,
                                  int itype)
{
    if (a == NULL) {
        ASN1err(ASN1_F_ASN1_STRING_GET_UINT64, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if ((a->type & ~V_ASN1_NEG) != itype) {
        ASN1err(ASN1_F_ASN1_STRING_GET_UINT64, ASN1_R_WRONG_INTEGER_TYPE);
        return 0;
    }
    if (a->type & V_ASN1_NEG) {
        ASN1err(ASN1_F_ASN1_STRING_GET_UINT64, ASN1_R_ILLEGAL_NEGATIVE_VALUE);
        return 0;
    }
    return asn1_get_uint64(pr, a->data, a->length);
}

static int asn1_string_set_uint64(ASN1_STRING *a, uint64_t r, int itype)
{
    unsigned char tbuf[sizeof(r)];
    size_t l;
    a->type = itype;
    l = asn1_put_uint64(tbuf, r);
    if (l == 0)
        return 0;
    return ASN1_STRING_set(a, tbuf, l);
}

/*
 * This is a version of d2i_ASN1_INTEGER that ignores the sign bit of ASN1
 * integers: some broken software can encode a positive INTEGER with its MSB
 * set as negative (it doesn't add a padding zero).
 */

ASN1_INTEGER *d2i_ASN1_UINTEGER(ASN1_INTEGER **a, const unsigned char **pp,
                                long length)
{
    ASN1_INTEGER *ret = NULL;
    const unsigned char *p;
    unsigned char *s;
    long len;
    int inf, tag, xclass;
    int i;

    if ((a == NULL) || ((*a) == NULL)) {
        if ((ret = ASN1_INTEGER_new()) == NULL)
            return (NULL);
        ret->type = V_ASN1_INTEGER;
    } else
        ret = (*a);

    p = *pp;
    inf = ASN1_get_object(&p, &len, &tag, &xclass, length);
    if (inf & 0x80) {
        i = ASN1_R_BAD_OBJECT_HEADER;
        goto err;
    }

    if (tag != V_ASN1_INTEGER) {
        i = ASN1_R_EXPECTING_AN_INTEGER;
        goto err;
    }

    /*
     * We must OPENSSL_malloc stuff, even for 0 bytes otherwise it signifies
     * a missing NULL parameter.
     */
    s = OPENSSL_malloc((int)len + 1);
    if (s == NULL) {
        i = ERR_R_MALLOC_FAILURE;
        goto err;
    }
    ret->type = V_ASN1_INTEGER;
    if (len) {
        if ((*p == 0) && (len != 1)) {
            p++;
            len--;
        }
        memcpy(s, p, (int)len);
        p += len;
    }

    OPENSSL_free(ret->data);
    ret->data = s;
    ret->length = (int)len;
    if (a != NULL)
        (*a) = ret;
    *pp = p;
    return (ret);
 err:
    ASN1err(ASN1_F_D2I_ASN1_UINTEGER, i);
    if ((a == NULL) || (*a != ret))
        ASN1_INTEGER_free(ret);
    return (NULL);
}

static ASN1_STRING *bn_to_asn1_string(const BIGNUM *bn, ASN1_STRING *ai,
                                      int atype)
{
    ASN1_INTEGER *ret;
    int len;

    if (ai == NULL) {
        ret = ASN1_STRING_type_new(atype);
    } else {
        ret = ai;
        ret->type = atype;
    }

    if (ret == NULL) {
        ASN1err(ASN1_F_BN_TO_ASN1_STRING, ERR_R_NESTED_ASN1_ERROR);
        goto err;
    }

    if (BN_is_negative(bn) && !BN_is_zero(bn))
        ret->type |= V_ASN1_NEG_INTEGER;

    len = BN_num_bytes(bn);

    if (len == 0)
        len = 1;

    if (ASN1_STRING_set(ret, NULL, len) == 0) {
        ASN1err(ASN1_F_BN_TO_ASN1_STRING, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /* Correct zero case */
    if (BN_is_zero(bn))
        ret->data[0] = 0;
    else
        len = BN_bn2bin(bn, ret->data);
    ret->length = len;
    return ret;
 err:
    if (ret != ai)
        ASN1_INTEGER_free(ret);
    return (NULL);
}

static BIGNUM *asn1_string_to_bn(const ASN1_INTEGER *ai, BIGNUM *bn,
                                 int itype)
{
    BIGNUM *ret;

    if ((ai->type & ~V_ASN1_NEG) != itype) {
        ASN1err(ASN1_F_ASN1_STRING_TO_BN, ASN1_R_WRONG_INTEGER_TYPE);
        return NULL;
    }

    ret = BN_bin2bn(ai->data, ai->length, bn);
    if (ret == 0) {
        ASN1err(ASN1_F_ASN1_STRING_TO_BN, ASN1_R_BN_LIB);
        return NULL;
    }
    if (ai->type & V_ASN1_NEG)
        BN_set_negative(ret, 1);
    return ret;
}

int ASN1_INTEGER_get_int64(int64_t *pr, const ASN1_INTEGER *a)
{
    return asn1_string_get_int64(pr, a, V_ASN1_INTEGER);
}

int ASN1_INTEGER_set_int64(ASN1_INTEGER *a, int64_t r)
{
    return asn1_string_set_int64(a, r, V_ASN1_INTEGER);
}

int ASN1_INTEGER_get_uint64(uint64_t *pr, const ASN1_INTEGER *a)
{
    return asn1_string_get_uint64(pr, a, V_ASN1_INTEGER);
}

int ASN1_INTEGER_set_uint64(ASN1_INTEGER *a, uint64_t r)
{
    return asn1_string_set_uint64(a, r, V_ASN1_INTEGER);
}

int ASN1_INTEGER_set(ASN1_INTEGER *a, long v)
{
    return ASN1_INTEGER_set_int64(a, v);
}

long ASN1_INTEGER_get(const ASN1_INTEGER *a)
{
    int i;
    int64_t r;
    if (a == NULL)
        return 0;
    i = ASN1_INTEGER_get_int64(&r, a);
    if (i == 0)
        return -1;
    if (r > LONG_MAX || r < LONG_MIN)
        return -1;
    return (long)r;
}

ASN1_INTEGER *BN_to_ASN1_INTEGER(const BIGNUM *bn, ASN1_INTEGER *ai)
{
    return bn_to_asn1_string(bn, ai, V_ASN1_INTEGER);
}

BIGNUM *ASN1_INTEGER_to_BN(const ASN1_INTEGER *ai, BIGNUM *bn)
{
    return asn1_string_to_bn(ai, bn, V_ASN1_INTEGER);
}

int ASN1_ENUMERATED_get_int64(int64_t *pr, const ASN1_ENUMERATED *a)
{
    return asn1_string_get_int64(pr, a, V_ASN1_ENUMERATED);
}

int ASN1_ENUMERATED_set_int64(ASN1_ENUMERATED *a, int64_t r)
{
    return asn1_string_set_int64(a, r, V_ASN1_ENUMERATED);
}

int ASN1_ENUMERATED_set(ASN1_ENUMERATED *a, long v)
{
    return ASN1_ENUMERATED_set_int64(a, v);
}

long ASN1_ENUMERATED_get(const ASN1_ENUMERATED *a)
{
    int i;
    int64_t r;
    if (a == NULL)
        return 0;
    if ((a->type & ~V_ASN1_NEG) != V_ASN1_ENUMERATED)
        return -1;
    if (a->length > (int)sizeof(long))
        return 0xffffffffL;
    i = ASN1_ENUMERATED_get_int64(&r, a);
    if (i == 0)
        return -1;
    if (r > LONG_MAX || r < LONG_MIN)
        return -1;
    return (long)r;
}

ASN1_ENUMERATED *BN_to_ASN1_ENUMERATED(const BIGNUM *bn, ASN1_ENUMERATED *ai)
{
    return bn_to_asn1_string(bn, ai, V_ASN1_ENUMERATED);
}

BIGNUM *ASN1_ENUMERATED_to_BN(const ASN1_ENUMERATED *ai, BIGNUM *bn)
{
    return asn1_string_to_bn(ai, bn, V_ASN1_ENUMERATED);
}
