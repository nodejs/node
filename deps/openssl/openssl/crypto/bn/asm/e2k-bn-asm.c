/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../bn_local.h"

#include <assert.h>
#include <e2kbuiltin.h>

/*
 * E2Kv5+ BIGNUM accelerator
 *
 * Implemented by Alexander Troosh <trush@yandex.ru>
 *
 */

#undef mul_add
#undef mul
#undef sqr

#if __iset__ < 5
#define mul_add(r, a, w, c)                  \
    {                                        \
        BN_ULONG high, low, ret, tmp = (a);  \
        ret = (r);                           \
        high = __builtin_e2k_umulhd(w, tmp); \
        ret += (c);                          \
        low = (w) * tmp;                     \
        (c) = (ret < (c)) ? 1 : 0;           \
        (c) += high;                         \
        ret += low;                          \
        (c) += (ret < low) ? 1 : 0;          \
        (r) = ret;                           \
    }

#define mul(r, a, w, c)                     \
    {                                       \
        BN_ULONG high, low, ret, ta = (a);  \
        low = (w) * ta;                     \
        high = __builtin_e2k_umulhd(w, ta); \
        ret = low + (c);                    \
        (c) = high;                         \
        (c) += (ret < low) ? 1 : 0;         \
        (r) = ret;                          \
    }
#else
/* { c_out[64], r_out[64] } = (r[64] + c[64]) + a[64]*w[64] */
#define mul_add(r, a, w, c)                             \
    {                                                   \
        BN_ULONG high, low, ret, ta = (a), tc = (c), q; \
        ret = (r);                                      \
        high = __builtin_e2k_umulhd(w, ta);             \
        low = (w) * ta;                                 \
        q = __builtin_e2k_addcd_c(ret, tc, 0);          \
        ret += tc;                                      \
        c = __builtin_e2k_addcd_c(ret, low, 0);         \
        r = ret + low;                                  \
        c += high + q;                                  \
    }

#define mul(r, a, w, c)                       \
    {                                         \
        BN_ULONG high, low, q, ta = (a);      \
        low = (w) * ta;                       \
        high = __builtin_e2k_umulhd(w, ta);   \
        q = __builtin_e2k_addcd_c(low, c, 0); \
        r = low + (c);                        \
        c = high + q;                         \
    }
#endif

#define sqr(r0, r1, a)                         \
    {                                          \
        BN_ULONG tmp = (a);                    \
        (r0) = tmp * tmp;                      \
        (r1) = __builtin_e2k_umulhd(tmp, tmp); \
    }

BN_ULONG bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num,
    BN_ULONG w)
{
    BN_ULONG c1 = 0;

    assert(num >= 0);
    if (num <= 0)
        return c1;

#ifndef OPENSSL_SMALL_FOOTPRINT
    while (num & ~3) {
        mul_add(rp[0], ap[0], w, c1);
        mul_add(rp[1], ap[1], w, c1);
        mul_add(rp[2], ap[2], w, c1);
        mul_add(rp[3], ap[3], w, c1);
        ap += 4;
        rp += 4;
        num -= 4;
    }
#endif
    while (num) {
        mul_add(rp[0], ap[0], w, c1);
        ap++;
        rp++;
        num--;
    }

    return c1;
}

BN_ULONG bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
    BN_ULONG c1 = 0;

    assert(num >= 0);
    if (num <= 0)
        return c1;

#ifndef OPENSSL_SMALL_FOOTPRINT
    while (num & ~3) {
        mul(rp[0], ap[0], w, c1);
        mul(rp[1], ap[1], w, c1);
        mul(rp[2], ap[2], w, c1);
        mul(rp[3], ap[3], w, c1);
        ap += 4;
        rp += 4;
        num -= 4;
    }
#endif
    while (num) {
        mul(rp[0], ap[0], w, c1);
        ap++;
        rp++;
        num--;
    }
    return c1;
}

void bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
    assert(n >= 0);
    if (n <= 0)
        return;

#ifndef OPENSSL_SMALL_FOOTPRINT
    while (n & ~3) {
        sqr(r[0], r[1], a[0]);
        sqr(r[2], r[3], a[1]);
        sqr(r[4], r[5], a[2]);
        sqr(r[6], r[7], a[3]);
        a += 4;
        r += 8;
        n -= 4;
    }
#endif
    while (n) {
        sqr(r[0], r[1], a[0]);
        a++;
        r += 2;
        n--;
    }
}

/* Divide h,l by d and return the result. */
BN_ULONG bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d)
{
    BN_ULONG dh, dl, q, ret = 0, th, tl, t;
    int i, count = 2;

    if (d == 0)
        return BN_MASK2;

    i = BN_num_bits_word(d);
    assert((i == BN_BITS2) || (h <= (BN_ULONG)1 << i));

    i = BN_BITS2 - i;
    if (h >= d)
        h -= d;

    if (i) {
        d <<= i;
        h = (h << i) | (l >> (BN_BITS2 - i));
        l <<= i;
    }
    dh = (d & BN_MASK2h) >> BN_BITS4;
    dl = (d & BN_MASK2l);
    for (;;) {
        if ((h >> BN_BITS4) == dh)
            q = BN_MASK2l;
        else
            q = h / dh;

        th = q * dh;
        tl = dl * q;
        for (;;) {
            t = h - th;
            if ((t & BN_MASK2h) || ((tl) <= ((t << BN_BITS4) | ((l & BN_MASK2h) >> BN_BITS4))))
                break;
            q--;
            th -= dh;
            tl -= dl;
        }
        t = (tl >> BN_BITS4);
        tl = (tl << BN_BITS4) & BN_MASK2h;
        th += t;

        if (l < tl)
            th++;
        l -= tl;
        if (h < th) {
            h += d;
            q--;
        }
        h -= th;

        if (--count == 0)
            break;

        ret = q << BN_BITS4;
        h = ((h << BN_BITS4) | (l >> BN_BITS4)) & BN_MASK2;
        l = (l & BN_MASK2l) << BN_BITS4;
    }
    ret |= q;
    return ret;
}

BN_ULONG bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
    int n)
{
    BN_ULONG c, l, t;

    assert(n >= 0);
    if (n <= 0)
        return (BN_ULONG)0;

    c = 0;
#ifndef OPENSSL_SMALL_FOOTPRINT
    while (n & ~3) {
#if __iset__ < 5
        t = a[0];
        t = (t + c) & BN_MASK2;
        c = (t < c);
        l = (t + b[0]) & BN_MASK2;
        c += (l < t);
        r[0] = l;
        t = a[1];
        t = (t + c) & BN_MASK2;
        c = (t < c);
        l = (t + b[1]) & BN_MASK2;
        c += (l < t);
        r[1] = l;
        t = a[2];
        t = (t + c) & BN_MASK2;
        c = (t < c);
        l = (t + b[2]) & BN_MASK2;
        c += (l < t);
        r[2] = l;
        t = a[3];
        t = (t + c) & BN_MASK2;
        c = (t < c);
        l = (t + b[3]) & BN_MASK2;
        c += (l < t);
        r[3] = l;
#else
        BN_ULONG t = __builtin_e2k_addcd_c(a[0], b[0], c);
        r[0] = __builtin_e2k_addcd(a[0], b[0], c);
        c = t;
        t = __builtin_e2k_addcd_c(a[1], b[1], c);
        r[1] = __builtin_e2k_addcd(a[1], b[1], c);
        c = t;
        t = __builtin_e2k_addcd_c(a[2], b[2], c);
        r[2] = __builtin_e2k_addcd(a[2], b[2], c);
        c = t;
        t = __builtin_e2k_addcd_c(a[3], b[3], c);
        r[3] = __builtin_e2k_addcd(a[3], b[3], c);
        c = t;
#endif
        a += 4;
        b += 4;
        r += 4;
        n -= 4;
    }
#endif
    while (n) {
#if __iset__ < 5
        t = a[0];
        t = (t + c) & BN_MASK2;
        c = (t < c);
        l = (t + b[0]) & BN_MASK2;
        c += (l < t);
        r[0] = l;
#else
        BN_ULONG t = __builtin_e2k_addcd_c(a[0], b[0], c);
        r[0] = __builtin_e2k_addcd(a[0], b[0], c);
        c = t;
#endif
        a++;
        b++;
        r++;
        n--;
    }
    return (BN_ULONG)c;
}

BN_ULONG bn_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
    int n)
{
#if __iset__ < 5
    BN_ULONG t1, t2;
#endif
    int c = 0;

    assert(n >= 0);
    if (n <= 0)
        return (BN_ULONG)0;

#ifndef OPENSSL_SMALL_FOOTPRINT
    while (n & ~3) {
#if __iset__ < 5
        t1 = a[0];
        t2 = (t1 - c) & BN_MASK2;
        c = (t2 > t1);
        t1 = b[0];
        t1 = (t2 - t1) & BN_MASK2;
        r[0] = t1;
        c += (t1 > t2);
        t1 = a[1];
        t2 = (t1 - c) & BN_MASK2;
        c = (t2 > t1);
        t1 = b[1];
        t1 = (t2 - t1) & BN_MASK2;
        r[1] = t1;
        c += (t1 > t2);
        t1 = a[2];
        t2 = (t1 - c) & BN_MASK2;
        c = (t2 > t1);
        t1 = b[2];
        t1 = (t2 - t1) & BN_MASK2;
        r[2] = t1;
        c += (t1 > t2);
        t1 = a[3];
        t2 = (t1 - c) & BN_MASK2;
        c = (t2 > t1);
        t1 = b[3];
        t1 = (t2 - t1) & BN_MASK2;
        r[3] = t1;
        c += (t1 > t2);
#else
        BN_ULONG t = __builtin_e2k_subcd_c(a[0], b[0], c);
        r[0] = __builtin_e2k_subcd(a[0], b[0], c);
        c = t;
        t = __builtin_e2k_subcd_c(a[1], b[1], c);
        r[1] = __builtin_e2k_subcd(a[1], b[1], c);
        c = t;
        t = __builtin_e2k_subcd_c(a[2], b[2], c);
        r[2] = __builtin_e2k_subcd(a[2], b[2], c);
        c = t;
        t = __builtin_e2k_subcd_c(a[3], b[3], c);
        r[3] = __builtin_e2k_subcd(a[3], b[3], c);
        c = t;
#endif
        a += 4;
        b += 4;
        r += 4;
        n -= 4;
    }
#endif
    while (n) {
#if __iset__ < 5
        t1 = a[0];
        t2 = (t1 - c) & BN_MASK2;
        c = (t2 > t1);
        t1 = b[0];
        t1 = (t2 - t1) & BN_MASK2;
        r[0] = t1;
        c += (t1 > t2);
#else
        BN_ULONG t = __builtin_e2k_subcd_c(a[0], b[0], c);
        r[0] = __builtin_e2k_subcd(a[0], b[0], c);
        c = t;
#endif
        a++;
        b++;
        r++;
        n--;
    }
    return c;
}

#ifndef OPENSSL_SMALL_FOOTPRINT

/* mul_add_c(a,b,c0,c1,c2)  -- c+=a*b for three word number c=(c2,c1,c0) */
/* mul_add_c2(a,b,c0,c1,c2) -- c+=2*a*b for three word number c=(c2,c1,c0) */
/* sqr_add_c(a,i,c0,c1,c2)  -- c+=a[i]^2 for three word number c=(c2,c1,c0) */
/*
 * sqr_add_c2(a,i,c0,c1,c2) -- c+=2*a[i]*a[j] for three word number
 * c=(c2,c1,c0)
 */

#if __iset__ < 5
/*
 * Keep in mind that carrying into high part of multiplication result
 * can not overflow, because it cannot be all-ones.
 */
#define mul_add_c(a, b, c0, c1, c2)    \
    do {                               \
        BN_ULONG ta = (a), tb = (b);   \
        BN_ULONG lo, hi;               \
        BN_UMULT_LOHI(lo, hi, ta, tb); \
        c0 += lo;                      \
        hi += (c0 < lo);               \
        c1 += hi;                      \
        c2 += (c1 < hi);               \
    } while (0)

#define mul_add_c2(a, b, c0, c1, c2)   \
    do {                               \
        BN_ULONG ta = (a), tb = (b);   \
        BN_ULONG lo, hi, tt;           \
        BN_UMULT_LOHI(lo, hi, ta, tb); \
        c0 += lo;                      \
        tt = hi + (c0 < lo);           \
        c1 += tt;                      \
        c2 += (c1 < tt);               \
        c0 += lo;                      \
        hi += (c0 < lo);               \
        c1 += hi;                      \
        c2 += (c1 < hi);               \
    } while (0)

#define sqr_add_c(a, i, c0, c1, c2)    \
    do {                               \
        BN_ULONG ta = (a)[i];          \
        BN_ULONG lo, hi;               \
        BN_UMULT_LOHI(lo, hi, ta, ta); \
        c0 += lo;                      \
        hi += (c0 < lo);               \
        c1 += hi;                      \
        c2 += (c1 < hi);               \
    } while (0)
#else /* __iset__ >= 5 */
#define mul_add_c(a, b, c0, c1, c2)             \
    do {                                        \
        BN_ULONG ta = (a), tb = (b);            \
        BN_ULONG lo, hi;                        \
        int q;                                  \
        lo = ta * tb;                           \
        hi = __builtin_e2k_umulhd(ta, tb);      \
        q = __builtin_e2k_addcd_c(c0, lo, 0);   \
        c0 += lo;                               \
        c2 += __builtin_e2k_addcd_c(c1, hi, q); \
        c1 = __builtin_e2k_addcd(c1, hi, q);    \
    } while (0)

#define mul_add_c2(a, b, c0, c1, c2)                \
    do {                                            \
        BN_ULONG ta = (a), tb = (b);                \
        BN_ULONG lo, hi, lo_msb;                    \
        int q;                                      \
        lo = ta * tb;                               \
        hi = __builtin_e2k_umulhd(ta, tb);          \
                                                    \
        lo_msb = lo >> 63;                          \
        lo <<= 1;                                   \
        c2 += hi >> 63;                             \
        hi = __builtin_e2k_insfd(hi, 0x7f, lo_msb); \
                                                    \
        q = __builtin_e2k_addcd_c(c0, lo, 0);       \
        c0 += lo;                                   \
        c2 += __builtin_e2k_addcd_c(c1, hi, q);     \
        c1 = __builtin_e2k_addcd(c1, hi, q);        \
    } while (0)

#define sqr_add_c(a, i, c0, c1, c2)             \
    do {                                        \
        BN_ULONG ta = (a)[i];                   \
        BN_ULONG lo, hi;                        \
        int q;                                  \
        lo = ta * ta;                           \
        hi = __builtin_e2k_umulhd(ta, ta);      \
        q = __builtin_e2k_addcd_c(c0, lo, 0);   \
        c0 += lo;                               \
        c2 += __builtin_e2k_addcd_c(c1, hi, q); \
        c1 = __builtin_e2k_addcd(c1, hi, q);    \
    } while (0)
#endif /* __iset__ */

#define sqr_add_c2(a, i, j, c0, c1, c2) \
    mul_add_c2((a)[i], (a)[j], c0, c1, c2)

void bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
    BN_ULONG c1, c2, c3;

    c1 = 0;
    c2 = 0;
    c3 = 0;
    mul_add_c(a[0], b[0], c1, c2, c3);
    r[0] = c1;
    c1 = 0;
    mul_add_c(a[0], b[1], c2, c3, c1);
    mul_add_c(a[1], b[0], c2, c3, c1);
    r[1] = c2;
    c2 = 0;
    mul_add_c(a[2], b[0], c3, c1, c2);
    mul_add_c(a[1], b[1], c3, c1, c2);
    mul_add_c(a[0], b[2], c3, c1, c2);
    r[2] = c3;
    c3 = 0;
    mul_add_c(a[0], b[3], c1, c2, c3);
    mul_add_c(a[1], b[2], c1, c2, c3);
    mul_add_c(a[2], b[1], c1, c2, c3);
    mul_add_c(a[3], b[0], c1, c2, c3);
    r[3] = c1;
    c1 = 0;
    mul_add_c(a[4], b[0], c2, c3, c1);
    mul_add_c(a[3], b[1], c2, c3, c1);
    mul_add_c(a[2], b[2], c2, c3, c1);
    mul_add_c(a[1], b[3], c2, c3, c1);
    mul_add_c(a[0], b[4], c2, c3, c1);
    r[4] = c2;
    c2 = 0;
    mul_add_c(a[0], b[5], c3, c1, c2);
    mul_add_c(a[1], b[4], c3, c1, c2);
    mul_add_c(a[2], b[3], c3, c1, c2);
    mul_add_c(a[3], b[2], c3, c1, c2);
    mul_add_c(a[4], b[1], c3, c1, c2);
    mul_add_c(a[5], b[0], c3, c1, c2);
    r[5] = c3;
    c3 = 0;
    mul_add_c(a[6], b[0], c1, c2, c3);
    mul_add_c(a[5], b[1], c1, c2, c3);
    mul_add_c(a[4], b[2], c1, c2, c3);
    mul_add_c(a[3], b[3], c1, c2, c3);
    mul_add_c(a[2], b[4], c1, c2, c3);
    mul_add_c(a[1], b[5], c1, c2, c3);
    mul_add_c(a[0], b[6], c1, c2, c3);
    r[6] = c1;
    c1 = 0;
    mul_add_c(a[0], b[7], c2, c3, c1);
    mul_add_c(a[1], b[6], c2, c3, c1);
    mul_add_c(a[2], b[5], c2, c3, c1);
    mul_add_c(a[3], b[4], c2, c3, c1);
    mul_add_c(a[4], b[3], c2, c3, c1);
    mul_add_c(a[5], b[2], c2, c3, c1);
    mul_add_c(a[6], b[1], c2, c3, c1);
    mul_add_c(a[7], b[0], c2, c3, c1);
    r[7] = c2;
    c2 = 0;
    mul_add_c(a[7], b[1], c3, c1, c2);
    mul_add_c(a[6], b[2], c3, c1, c2);
    mul_add_c(a[5], b[3], c3, c1, c2);
    mul_add_c(a[4], b[4], c3, c1, c2);
    mul_add_c(a[3], b[5], c3, c1, c2);
    mul_add_c(a[2], b[6], c3, c1, c2);
    mul_add_c(a[1], b[7], c3, c1, c2);
    r[8] = c3;
    c3 = 0;
    mul_add_c(a[2], b[7], c1, c2, c3);
    mul_add_c(a[3], b[6], c1, c2, c3);
    mul_add_c(a[4], b[5], c1, c2, c3);
    mul_add_c(a[5], b[4], c1, c2, c3);
    mul_add_c(a[6], b[3], c1, c2, c3);
    mul_add_c(a[7], b[2], c1, c2, c3);
    r[9] = c1;
    c1 = 0;
    mul_add_c(a[7], b[3], c2, c3, c1);
    mul_add_c(a[6], b[4], c2, c3, c1);
    mul_add_c(a[5], b[5], c2, c3, c1);
    mul_add_c(a[4], b[6], c2, c3, c1);
    mul_add_c(a[3], b[7], c2, c3, c1);
    r[10] = c2;
    c2 = 0;
    mul_add_c(a[4], b[7], c3, c1, c2);
    mul_add_c(a[5], b[6], c3, c1, c2);
    mul_add_c(a[6], b[5], c3, c1, c2);
    mul_add_c(a[7], b[4], c3, c1, c2);
    r[11] = c3;
    c3 = 0;
    mul_add_c(a[7], b[5], c1, c2, c3);
    mul_add_c(a[6], b[6], c1, c2, c3);
    mul_add_c(a[5], b[7], c1, c2, c3);
    r[12] = c1;
    c1 = 0;
    mul_add_c(a[6], b[7], c2, c3, c1);
    mul_add_c(a[7], b[6], c2, c3, c1);
    r[13] = c2;
    c2 = 0;
    mul_add_c(a[7], b[7], c3, c1, c2);
    r[14] = c3;
    r[15] = c1;
}

void bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
    BN_ULONG c1, c2, c3;

    c1 = 0;
    c2 = 0;
    c3 = 0;
    mul_add_c(a[0], b[0], c1, c2, c3);
    r[0] = c1;
    c1 = 0;
    mul_add_c(a[0], b[1], c2, c3, c1);
    mul_add_c(a[1], b[0], c2, c3, c1);
    r[1] = c2;
    c2 = 0;
    mul_add_c(a[2], b[0], c3, c1, c2);
    mul_add_c(a[1], b[1], c3, c1, c2);
    mul_add_c(a[0], b[2], c3, c1, c2);
    r[2] = c3;
    c3 = 0;
    mul_add_c(a[0], b[3], c1, c2, c3);
    mul_add_c(a[1], b[2], c1, c2, c3);
    mul_add_c(a[2], b[1], c1, c2, c3);
    mul_add_c(a[3], b[0], c1, c2, c3);
    r[3] = c1;
    c1 = 0;
    mul_add_c(a[3], b[1], c2, c3, c1);
    mul_add_c(a[2], b[2], c2, c3, c1);
    mul_add_c(a[1], b[3], c2, c3, c1);
    r[4] = c2;
    c2 = 0;
    mul_add_c(a[2], b[3], c3, c1, c2);
    mul_add_c(a[3], b[2], c3, c1, c2);
    r[5] = c3;
    c3 = 0;
    mul_add_c(a[3], b[3], c1, c2, c3);
    r[6] = c1;
    r[7] = c2;
}

void bn_sqr_comba8(BN_ULONG *r, const BN_ULONG *a)
{
    BN_ULONG c1, c2, c3;

    c1 = 0;
    c2 = 0;
    c3 = 0;
    sqr_add_c(a, 0, c1, c2, c3);
    r[0] = c1;
    c1 = 0;
    sqr_add_c2(a, 1, 0, c2, c3, c1);
    r[1] = c2;
    c2 = 0;
    sqr_add_c(a, 1, c3, c1, c2);
    sqr_add_c2(a, 2, 0, c3, c1, c2);
    r[2] = c3;
    c3 = 0;
    sqr_add_c2(a, 3, 0, c1, c2, c3);
    sqr_add_c2(a, 2, 1, c1, c2, c3);
    r[3] = c1;
    c1 = 0;
    sqr_add_c(a, 2, c2, c3, c1);
    sqr_add_c2(a, 3, 1, c2, c3, c1);
    sqr_add_c2(a, 4, 0, c2, c3, c1);
    r[4] = c2;
    c2 = 0;
    sqr_add_c2(a, 5, 0, c3, c1, c2);
    sqr_add_c2(a, 4, 1, c3, c1, c2);
    sqr_add_c2(a, 3, 2, c3, c1, c2);
    r[5] = c3;
    c3 = 0;
    sqr_add_c(a, 3, c1, c2, c3);
    sqr_add_c2(a, 4, 2, c1, c2, c3);
    sqr_add_c2(a, 5, 1, c1, c2, c3);
    sqr_add_c2(a, 6, 0, c1, c2, c3);
    r[6] = c1;
    c1 = 0;
    sqr_add_c2(a, 7, 0, c2, c3, c1);
    sqr_add_c2(a, 6, 1, c2, c3, c1);
    sqr_add_c2(a, 5, 2, c2, c3, c1);
    sqr_add_c2(a, 4, 3, c2, c3, c1);
    r[7] = c2;
    c2 = 0;
    sqr_add_c(a, 4, c3, c1, c2);
    sqr_add_c2(a, 5, 3, c3, c1, c2);
    sqr_add_c2(a, 6, 2, c3, c1, c2);
    sqr_add_c2(a, 7, 1, c3, c1, c2);
    r[8] = c3;
    c3 = 0;
    sqr_add_c2(a, 7, 2, c1, c2, c3);
    sqr_add_c2(a, 6, 3, c1, c2, c3);
    sqr_add_c2(a, 5, 4, c1, c2, c3);
    r[9] = c1;
    c1 = 0;
    sqr_add_c(a, 5, c2, c3, c1);
    sqr_add_c2(a, 6, 4, c2, c3, c1);
    sqr_add_c2(a, 7, 3, c2, c3, c1);
    r[10] = c2;
    c2 = 0;
    sqr_add_c2(a, 7, 4, c3, c1, c2);
    sqr_add_c2(a, 6, 5, c3, c1, c2);
    r[11] = c3;
    c3 = 0;
    sqr_add_c(a, 6, c1, c2, c3);
    sqr_add_c2(a, 7, 5, c1, c2, c3);
    r[12] = c1;
    c1 = 0;
    sqr_add_c2(a, 7, 6, c2, c3, c1);
    r[13] = c2;
    c2 = 0;
    sqr_add_c(a, 7, c3, c1, c2);
    r[14] = c3;
    r[15] = c1;
}

void bn_sqr_comba4(BN_ULONG *r, const BN_ULONG *a)
{
    BN_ULONG c1, c2, c3;

    c1 = 0;
    c2 = 0;
    c3 = 0;
    sqr_add_c(a, 0, c1, c2, c3);
    r[0] = c1;
    c1 = 0;
    sqr_add_c2(a, 1, 0, c2, c3, c1);
    r[1] = c2;
    c2 = 0;
    sqr_add_c(a, 1, c3, c1, c2);
    sqr_add_c2(a, 2, 0, c3, c1, c2);
    r[2] = c3;
    c3 = 0;
    sqr_add_c2(a, 3, 0, c1, c2, c3);
    sqr_add_c2(a, 2, 1, c1, c2, c3);
    r[3] = c1;
    c1 = 0;
    sqr_add_c(a, 2, c2, c3, c1);
    sqr_add_c2(a, 3, 1, c2, c3, c1);
    r[4] = c2;
    c2 = 0;
    sqr_add_c2(a, 3, 2, c3, c1, c2);
    r[5] = c3;
    c3 = 0;
    sqr_add_c(a, 3, c1, c2, c3);
    r[6] = c1;
    r[7] = c2;
}

#include <alloca.h>
/*
 * This is essentially reference implementation, which may or may not
 * result in performance improvement. E.g. on IA-32 this routine was
 * observed to give 40% faster rsa1024 private key operations and 10%
 * faster rsa4096 ones, while on AMD64 it improves rsa1024 sign only
 * by 10% and *worsens* rsa4096 sign by 15%. Once again, it's a
 * reference implementation, one to be used as starting point for
 * platform-specific assembler. Mentioned numbers apply to compiler
 * generated code compiled with and without -DOPENSSL_BN_ASM_MONT and
 * can vary not only from platform to platform, but even for compiler
 * versions. Assembler vs. assembler improvement coefficients can
 * [and are known to] differ and are to be documented elsewhere.
 */
int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
    const BN_ULONG *np, const BN_ULONG *n0p, int num)
{
    BN_ULONG c0, c1, ml, *tp, n0;
    volatile BN_ULONG *vp;
    int i = 0, j;

#if 0 /* template for platform-specific \
       * implementation */
    if (ap == bp)
        return bn_sqr_mont(rp, ap, np, n0p, num);
#endif
    vp = tp = alloca((num + 2) * sizeof(BN_ULONG));

    n0 = *n0p;

    c0 = 0;
    ml = bp[0];

    for (j = 0; j < num; ++j)
        mul(tp[j], ap[j], ml, c0);

    tp[num] = c0;
    tp[num + 1] = 0;
    goto enter;

    for (i = 0; i < num; i++) {
        c0 = 0;
        ml = bp[i];

        for (j = 0; j < num; ++j)
            mul_add(tp[j], ap[j], ml, c0);

        c1 = (tp[num] + c0) & BN_MASK2;
        tp[num] = c1;
        tp[num + 1] = (c1 < c0 ? 1 : 0);
    enter:
        c1 = tp[0];
        ml = (c1 * n0) & BN_MASK2;
        c0 = 0;

        mul_add(c1, ml, np[0], c0);

        for (j = 1; j < num; j++) {
            c1 = tp[j];
            mul_add(c1, ml, np[j], c0);
            tp[j - 1] = c1 & BN_MASK2;
        }
        c1 = (tp[num] + c0) & BN_MASK2;
        tp[num - 1] = c1;
        tp[num] = tp[num + 1] + (c1 < c0 ? 1 : 0);
    }

    if (tp[num] != 0 || tp[num - 1] >= np[num - 1]) {
        c0 = bn_sub_words(rp, tp, np, num);
        if (tp[num] != 0 || c0 == 0) {
            for (i = 0; i < num + 2; i++)
                vp[i] = 0;
            return 1;
        }
    }
    for (i = 0; i < num; i++)
        rp[i] = tp[i], vp[i] = 0;
    vp[num] = 0;
    vp[num + 1] = 0;
    return 1;
}
#endif /* !OPENSSL_SMALL_FOOTPRINT */
