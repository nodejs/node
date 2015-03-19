/* crypto/bn/bn_sqrt.c */
/*
 * Written by Lenka Fibikova <fibikova@exp-math.uni-essen.de> and Bodo
 * Moeller for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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

#include "cryptlib.h"
#include "bn_lcl.h"

BIGNUM *BN_mod_sqrt(BIGNUM *in, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
/*
 * Returns 'ret' such that ret^2 == a (mod p), using the Tonelli/Shanks
 * algorithm (cf. Henri Cohen, "A Course in Algebraic Computational Number
 * Theory", algorithm 1.5.1). 'p' must be prime!
 */
{
    BIGNUM *ret = in;
    int err = 1;
    int r;
    BIGNUM *A, *b, *q, *t, *x, *y;
    int e, i, j;

    if (!BN_is_odd(p) || BN_abs_is_word(p, 1)) {
        if (BN_abs_is_word(p, 2)) {
            if (ret == NULL)
                ret = BN_new();
            if (ret == NULL)
                goto end;
            if (!BN_set_word(ret, BN_is_bit_set(a, 0))) {
                if (ret != in)
                    BN_free(ret);
                return NULL;
            }
            bn_check_top(ret);
            return ret;
        }

        BNerr(BN_F_BN_MOD_SQRT, BN_R_P_IS_NOT_PRIME);
        return (NULL);
    }

    if (BN_is_zero(a) || BN_is_one(a)) {
        if (ret == NULL)
            ret = BN_new();
        if (ret == NULL)
            goto end;
        if (!BN_set_word(ret, BN_is_one(a))) {
            if (ret != in)
                BN_free(ret);
            return NULL;
        }
        bn_check_top(ret);
        return ret;
    }

    BN_CTX_start(ctx);
    A = BN_CTX_get(ctx);
    b = BN_CTX_get(ctx);
    q = BN_CTX_get(ctx);
    t = BN_CTX_get(ctx);
    x = BN_CTX_get(ctx);
    y = BN_CTX_get(ctx);
    if (y == NULL)
        goto end;

    if (ret == NULL)
        ret = BN_new();
    if (ret == NULL)
        goto end;

    /* A = a mod p */
    if (!BN_nnmod(A, a, p, ctx))
        goto end;

    /* now write  |p| - 1  as  2^e*q  where  q  is odd */
    e = 1;
    while (!BN_is_bit_set(p, e))
        e++;
    /* we'll set  q  later (if needed) */

    if (e == 1) {
        /*-
         * The easy case:  (|p|-1)/2  is odd, so 2 has an inverse
         * modulo  (|p|-1)/2,  and square roots can be computed
         * directly by modular exponentiation.
         * We have
         *     2 * (|p|+1)/4 == 1   (mod (|p|-1)/2),
         * so we can use exponent  (|p|+1)/4,  i.e.  (|p|-3)/4 + 1.
         */
        if (!BN_rshift(q, p, 2))
            goto end;
        q->neg = 0;
        if (!BN_add_word(q, 1))
            goto end;
        if (!BN_mod_exp(ret, A, q, p, ctx))
            goto end;
        err = 0;
        goto vrfy;
    }

    if (e == 2) {
        /*-
         * |p| == 5  (mod 8)
         *
         * In this case  2  is always a non-square since
         * Legendre(2,p) = (-1)^((p^2-1)/8)  for any odd prime.
         * So if  a  really is a square, then  2*a  is a non-square.
         * Thus for
         *      b := (2*a)^((|p|-5)/8),
         *      i := (2*a)*b^2
         * we have
         *     i^2 = (2*a)^((1 + (|p|-5)/4)*2)
         *         = (2*a)^((p-1)/2)
         *         = -1;
         * so if we set
         *      x := a*b*(i-1),
         * then
         *     x^2 = a^2 * b^2 * (i^2 - 2*i + 1)
         *         = a^2 * b^2 * (-2*i)
         *         = a*(-i)*(2*a*b^2)
         *         = a*(-i)*i
         *         = a.
         *
         * (This is due to A.O.L. Atkin,
         * <URL: http://listserv.nodak.edu/scripts/wa.exe?A2=ind9211&L=nmbrthry&O=T&P=562>,
         * November 1992.)
         */

        /* t := 2*a */
        if (!BN_mod_lshift1_quick(t, A, p))
            goto end;

        /* b := (2*a)^((|p|-5)/8) */
        if (!BN_rshift(q, p, 3))
            goto end;
        q->neg = 0;
        if (!BN_mod_exp(b, t, q, p, ctx))
            goto end;

        /* y := b^2 */
        if (!BN_mod_sqr(y, b, p, ctx))
            goto end;

        /* t := (2*a)*b^2 - 1 */
        if (!BN_mod_mul(t, t, y, p, ctx))
            goto end;
        if (!BN_sub_word(t, 1))
            goto end;

        /* x = a*b*t */
        if (!BN_mod_mul(x, A, b, p, ctx))
            goto end;
        if (!BN_mod_mul(x, x, t, p, ctx))
            goto end;

        if (!BN_copy(ret, x))
            goto end;
        err = 0;
        goto vrfy;
    }

    /*
     * e > 2, so we really have to use the Tonelli/Shanks algorithm. First,
     * find some y that is not a square.
     */
    if (!BN_copy(q, p))
        goto end;               /* use 'q' as temp */
    q->neg = 0;
    i = 2;
    do {
        /*
         * For efficiency, try small numbers first; if this fails, try random
         * numbers.
         */
        if (i < 22) {
            if (!BN_set_word(y, i))
                goto end;
        } else {
            if (!BN_pseudo_rand(y, BN_num_bits(p), 0, 0))
                goto end;
            if (BN_ucmp(y, p) >= 0) {
                if (!(p->neg ? BN_add : BN_sub) (y, y, p))
                    goto end;
            }
            /* now 0 <= y < |p| */
            if (BN_is_zero(y))
                if (!BN_set_word(y, i))
                    goto end;
        }

        r = BN_kronecker(y, q, ctx); /* here 'q' is |p| */
        if (r < -1)
            goto end;
        if (r == 0) {
            /* m divides p */
            BNerr(BN_F_BN_MOD_SQRT, BN_R_P_IS_NOT_PRIME);
            goto end;
        }
    }
    while (r == 1 && ++i < 82);

    if (r != -1) {
        /*
         * Many rounds and still no non-square -- this is more likely a bug
         * than just bad luck. Even if p is not prime, we should have found
         * some y such that r == -1.
         */
        BNerr(BN_F_BN_MOD_SQRT, BN_R_TOO_MANY_ITERATIONS);
        goto end;
    }

    /* Here's our actual 'q': */
    if (!BN_rshift(q, q, e))
        goto end;

    /*
     * Now that we have some non-square, we can find an element of order 2^e
     * by computing its q'th power.
     */
    if (!BN_mod_exp(y, y, q, p, ctx))
        goto end;
    if (BN_is_one(y)) {
        BNerr(BN_F_BN_MOD_SQRT, BN_R_P_IS_NOT_PRIME);
        goto end;
    }

    /*-
     * Now we know that (if  p  is indeed prime) there is an integer
     * k,  0 <= k < 2^e,  such that
     *
     *      a^q * y^k == 1   (mod p).
     *
     * As  a^q  is a square and  y  is not,  k  must be even.
     * q+1  is even, too, so there is an element
     *
     *     X := a^((q+1)/2) * y^(k/2),
     *
     * and it satisfies
     *
     *     X^2 = a^q * a     * y^k
     *         = a,
     *
     * so it is the square root that we are looking for.
     */

    /* t := (q-1)/2  (note that  q  is odd) */
    if (!BN_rshift1(t, q))
        goto end;

    /* x := a^((q-1)/2) */
    if (BN_is_zero(t)) {        /* special case: p = 2^e + 1 */
        if (!BN_nnmod(t, A, p, ctx))
            goto end;
        if (BN_is_zero(t)) {
            /* special case: a == 0  (mod p) */
            BN_zero(ret);
            err = 0;
            goto end;
        } else if (!BN_one(x))
            goto end;
    } else {
        if (!BN_mod_exp(x, A, t, p, ctx))
            goto end;
        if (BN_is_zero(x)) {
            /* special case: a == 0  (mod p) */
            BN_zero(ret);
            err = 0;
            goto end;
        }
    }

    /* b := a*x^2  (= a^q) */
    if (!BN_mod_sqr(b, x, p, ctx))
        goto end;
    if (!BN_mod_mul(b, b, A, p, ctx))
        goto end;

    /* x := a*x    (= a^((q+1)/2)) */
    if (!BN_mod_mul(x, x, A, p, ctx))
        goto end;

    while (1) {
        /*-
         * Now  b  is  a^q * y^k  for some even  k  (0 <= k < 2^E
         * where  E  refers to the original value of  e,  which we
         * don't keep in a variable),  and  x  is  a^((q+1)/2) * y^(k/2).
         *
         * We have  a*b = x^2,
         *    y^2^(e-1) = -1,
         *    b^2^(e-1) = 1.
         */

        if (BN_is_one(b)) {
            if (!BN_copy(ret, x))
                goto end;
            err = 0;
            goto vrfy;
        }

        /* find smallest  i  such that  b^(2^i) = 1 */
        i = 1;
        if (!BN_mod_sqr(t, b, p, ctx))
            goto end;
        while (!BN_is_one(t)) {
            i++;
            if (i == e) {
                BNerr(BN_F_BN_MOD_SQRT, BN_R_NOT_A_SQUARE);
                goto end;
            }
            if (!BN_mod_mul(t, t, t, p, ctx))
                goto end;
        }

        /* t := y^2^(e - i - 1) */
        if (!BN_copy(t, y))
            goto end;
        for (j = e - i - 1; j > 0; j--) {
            if (!BN_mod_sqr(t, t, p, ctx))
                goto end;
        }
        if (!BN_mod_mul(y, t, t, p, ctx))
            goto end;
        if (!BN_mod_mul(x, x, t, p, ctx))
            goto end;
        if (!BN_mod_mul(b, b, y, p, ctx))
            goto end;
        e = i;
    }

 vrfy:
    if (!err) {
        /*
         * verify the result -- the input might have been not a square (test
         * added in 0.9.8)
         */

        if (!BN_mod_sqr(x, ret, p, ctx))
            err = 1;

        if (!err && 0 != BN_cmp(x, A)) {
            BNerr(BN_F_BN_MOD_SQRT, BN_R_NOT_A_SQUARE);
            err = 1;
        }
    }

 end:
    if (err) {
        if (ret != NULL && ret != in) {
            BN_clear_free(ret);
        }
        ret = NULL;
    }
    BN_CTX_end(ctx);
    bn_check_top(ret);
    return ret;
}
