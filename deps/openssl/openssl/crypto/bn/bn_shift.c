/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#include "bn_lcl.h"

int BN_lshift1(BIGNUM *r, const BIGNUM *a)
{
    register BN_ULONG *ap, *rp, t, c;
    int i;

    bn_check_top(r);
    bn_check_top(a);

    if (r != a) {
        r->neg = a->neg;
        if (bn_wexpand(r, a->top + 1) == NULL)
            return 0;
        r->top = a->top;
    } else {
        if (bn_wexpand(r, a->top + 1) == NULL)
            return 0;
    }
    ap = a->d;
    rp = r->d;
    c = 0;
    for (i = 0; i < a->top; i++) {
        t = *(ap++);
        *(rp++) = ((t << 1) | c) & BN_MASK2;
        c = (t & BN_TBIT) ? 1 : 0;
    }
    if (c) {
        *rp = 1;
        r->top++;
    }
    bn_check_top(r);
    return 1;
}

int BN_rshift1(BIGNUM *r, const BIGNUM *a)
{
    BN_ULONG *ap, *rp, t, c;
    int i, j;

    bn_check_top(r);
    bn_check_top(a);

    if (BN_is_zero(a)) {
        BN_zero(r);
        return 1;
    }
    i = a->top;
    ap = a->d;
    j = i - (ap[i - 1] == 1);
    if (a != r) {
        if (bn_wexpand(r, j) == NULL)
            return 0;
        r->neg = a->neg;
    }
    rp = r->d;
    t = ap[--i];
    c = (t & 1) ? BN_TBIT : 0;
    if (t >>= 1)
        rp[i] = t;
    while (i > 0) {
        t = ap[--i];
        rp[i] = ((t >> 1) & BN_MASK2) | c;
        c = (t & 1) ? BN_TBIT : 0;
    }
    r->top = j;
    if (!r->top)
        r->neg = 0; /* don't allow negative zero */
    bn_check_top(r);
    return 1;
}

int BN_lshift(BIGNUM *r, const BIGNUM *a, int n)
{
    int i, nw, lb, rb;
    BN_ULONG *t, *f;
    BN_ULONG l;

    bn_check_top(r);
    bn_check_top(a);

    if (n < 0) {
        BNerr(BN_F_BN_LSHIFT, BN_R_INVALID_SHIFT);
        return 0;
    }

    nw = n / BN_BITS2;
    if (bn_wexpand(r, a->top + nw + 1) == NULL)
        return 0;
    r->neg = a->neg;
    lb = n % BN_BITS2;
    rb = BN_BITS2 - lb;
    f = a->d;
    t = r->d;
    t[a->top + nw] = 0;
    if (lb == 0)
        for (i = a->top - 1; i >= 0; i--)
            t[nw + i] = f[i];
    else
        for (i = a->top - 1; i >= 0; i--) {
            l = f[i];
            t[nw + i + 1] |= (l >> rb) & BN_MASK2;
            t[nw + i] = (l << lb) & BN_MASK2;
        }
    memset(t, 0, sizeof(*t) * nw);
    r->top = a->top + nw + 1;
    bn_correct_top(r);
    bn_check_top(r);
    return 1;
}

int BN_rshift(BIGNUM *r, const BIGNUM *a, int n)
{
    int i, j, nw, lb, rb;
    BN_ULONG *t, *f;
    BN_ULONG l, tmp;

    bn_check_top(r);
    bn_check_top(a);

    if (n < 0) {
        BNerr(BN_F_BN_RSHIFT, BN_R_INVALID_SHIFT);
        return 0;
    }

    nw = n / BN_BITS2;
    rb = n % BN_BITS2;
    lb = BN_BITS2 - rb;
    if (nw >= a->top || a->top == 0) {
        BN_zero(r);
        return 1;
    }
    i = (BN_num_bits(a) - n + (BN_BITS2 - 1)) / BN_BITS2;
    if (r != a) {
        if (bn_wexpand(r, i) == NULL)
            return 0;
        r->neg = a->neg;
    } else {
        if (n == 0)
            return 1;           /* or the copying loop will go berserk */
    }

    f = &(a->d[nw]);
    t = r->d;
    j = a->top - nw;
    r->top = i;

    if (rb == 0) {
        for (i = j; i != 0; i--)
            *(t++) = *(f++);
    } else {
        l = *(f++);
        for (i = j - 1; i != 0; i--) {
            tmp = (l >> rb) & BN_MASK2;
            l = *(f++);
            *(t++) = (tmp | (l << lb)) & BN_MASK2;
        }
        if ((l = (l >> rb) & BN_MASK2))
            *(t) = l;
    }
    if (!r->top)
        r->neg = 0; /* don't allow negative zero */
    bn_check_top(r);
    return 1;
}
