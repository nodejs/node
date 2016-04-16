/* crypto/bn/bn_div.c */
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
#include <openssl/bn.h>
#include "cryptlib.h"
#include "bn_lcl.h"

/* The old slow way */
#if 0
int BN_div(BIGNUM *dv, BIGNUM *rem, const BIGNUM *m, const BIGNUM *d,
           BN_CTX *ctx)
{
    int i, nm, nd;
    int ret = 0;
    BIGNUM *D;

    bn_check_top(m);
    bn_check_top(d);
    if (BN_is_zero(d)) {
        BNerr(BN_F_BN_DIV, BN_R_DIV_BY_ZERO);
        return (0);
    }

    if (BN_ucmp(m, d) < 0) {
        if (rem != NULL) {
            if (BN_copy(rem, m) == NULL)
                return (0);
        }
        if (dv != NULL)
            BN_zero(dv);
        return (1);
    }

    BN_CTX_start(ctx);
    D = BN_CTX_get(ctx);
    if (dv == NULL)
        dv = BN_CTX_get(ctx);
    if (rem == NULL)
        rem = BN_CTX_get(ctx);
    if (D == NULL || dv == NULL || rem == NULL)
        goto end;

    nd = BN_num_bits(d);
    nm = BN_num_bits(m);
    if (BN_copy(D, d) == NULL)
        goto end;
    if (BN_copy(rem, m) == NULL)
        goto end;

    /*
     * The next 2 are needed so we can do a dv->d[0]|=1 later since
     * BN_lshift1 will only work once there is a value :-)
     */
    BN_zero(dv);
    if (bn_wexpand(dv, 1) == NULL)
        goto end;
    dv->top = 1;

    if (!BN_lshift(D, D, nm - nd))
        goto end;
    for (i = nm - nd; i >= 0; i--) {
        if (!BN_lshift1(dv, dv))
            goto end;
        if (BN_ucmp(rem, D) >= 0) {
            dv->d[0] |= 1;
            if (!BN_usub(rem, rem, D))
                goto end;
        }
/* CAN IMPROVE (and have now :=) */
        if (!BN_rshift1(D, D))
            goto end;
    }
    rem->neg = BN_is_zero(rem) ? 0 : m->neg;
    dv->neg = m->neg ^ d->neg;
    ret = 1;
 end:
    BN_CTX_end(ctx);
    return (ret);
}

#else

# if !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM) \
    && !defined(PEDANTIC) && !defined(BN_DIV3W)
#  if defined(__GNUC__) && __GNUC__>=2
#   if defined(__i386) || defined (__i386__)
   /*-
    * There were two reasons for implementing this template:
    * - GNU C generates a call to a function (__udivdi3 to be exact)
    *   in reply to ((((BN_ULLONG)n0)<<BN_BITS2)|n1)/d0 (I fail to
    *   understand why...);
    * - divl doesn't only calculate quotient, but also leaves
    *   remainder in %edx which we can definitely use here:-)
    *
    *                                   <appro@fy.chalmers.se>
    */
#    undef bn_div_words
#    define bn_div_words(n0,n1,d0)                \
        ({  asm volatile (                      \
                "divl   %4"                     \
                : "=a"(q), "=d"(rem)            \
                : "a"(n1), "d"(n0), "g"(d0)     \
                : "cc");                        \
            q;                                  \
        })
#    define REMAINDER_IS_ALREADY_CALCULATED
#   elif defined(__x86_64) && defined(SIXTY_FOUR_BIT_LONG)
   /*
    * Same story here, but it's 128-bit by 64-bit division. Wow!
    *                                   <appro@fy.chalmers.se>
    */
#    undef bn_div_words
#    define bn_div_words(n0,n1,d0)                \
        ({  asm volatile (                      \
                "divq   %4"                     \
                : "=a"(q), "=d"(rem)            \
                : "a"(n1), "d"(n0), "g"(d0)     \
                : "cc");                        \
            q;                                  \
        })
#    define REMAINDER_IS_ALREADY_CALCULATED
#   endif                       /* __<cpu> */
#  endif                        /* __GNUC__ */
# endif                         /* OPENSSL_NO_ASM */

/*-
 * BN_div computes  dv := num / divisor,  rounding towards
 * zero, and sets up rm  such that  dv*divisor + rm = num  holds.
 * Thus:
 *     dv->neg == num->neg ^ divisor->neg  (unless the result is zero)
 *     rm->neg == num->neg                 (unless the remainder is zero)
 * If 'dv' or 'rm' is NULL, the respective value is not returned.
 */
int BN_div(BIGNUM *dv, BIGNUM *rm, const BIGNUM *num, const BIGNUM *divisor,
           BN_CTX *ctx)
{
    int norm_shift, i, loop;
    BIGNUM *tmp, wnum, *snum, *sdiv, *res;
    BN_ULONG *resp, *wnump;
    BN_ULONG d0, d1;
    int num_n, div_n;
    int no_branch = 0;

    /*
     * Invalid zero-padding would have particularly bad consequences so don't
     * just rely on bn_check_top() here (bn_check_top() works only for
     * BN_DEBUG builds)
     */
    if ((num->top > 0 && num->d[num->top - 1] == 0) ||
        (divisor->top > 0 && divisor->d[divisor->top - 1] == 0)) {
        BNerr(BN_F_BN_DIV, BN_R_NOT_INITIALIZED);
        return 0;
    }

    bn_check_top(num);
    bn_check_top(divisor);

    if ((BN_get_flags(num, BN_FLG_CONSTTIME) != 0)
        || (BN_get_flags(divisor, BN_FLG_CONSTTIME) != 0)) {
        no_branch = 1;
    }

    bn_check_top(dv);
    bn_check_top(rm);
    /*- bn_check_top(num); *//*
     * 'num' has been checked already
     */
    /*- bn_check_top(divisor); *//*
     * 'divisor' has been checked already
     */

    if (BN_is_zero(divisor)) {
        BNerr(BN_F_BN_DIV, BN_R_DIV_BY_ZERO);
        return (0);
    }

    if (!no_branch && BN_ucmp(num, divisor) < 0) {
        if (rm != NULL) {
            if (BN_copy(rm, num) == NULL)
                return (0);
        }
        if (dv != NULL)
            BN_zero(dv);
        return (1);
    }

    BN_CTX_start(ctx);
    tmp = BN_CTX_get(ctx);
    snum = BN_CTX_get(ctx);
    sdiv = BN_CTX_get(ctx);
    if (dv == NULL)
        res = BN_CTX_get(ctx);
    else
        res = dv;
    if (sdiv == NULL || res == NULL || tmp == NULL || snum == NULL)
        goto err;

    /* First we normalise the numbers */
    norm_shift = BN_BITS2 - ((BN_num_bits(divisor)) % BN_BITS2);
    if (!(BN_lshift(sdiv, divisor, norm_shift)))
        goto err;
    sdiv->neg = 0;
    norm_shift += BN_BITS2;
    if (!(BN_lshift(snum, num, norm_shift)))
        goto err;
    snum->neg = 0;

    if (no_branch) {
        /*
         * Since we don't know whether snum is larger than sdiv, we pad snum
         * with enough zeroes without changing its value.
         */
        if (snum->top <= sdiv->top + 1) {
            if (bn_wexpand(snum, sdiv->top + 2) == NULL)
                goto err;
            for (i = snum->top; i < sdiv->top + 2; i++)
                snum->d[i] = 0;
            snum->top = sdiv->top + 2;
        } else {
            if (bn_wexpand(snum, snum->top + 1) == NULL)
                goto err;
            snum->d[snum->top] = 0;
            snum->top++;
        }
    }

    div_n = sdiv->top;
    num_n = snum->top;
    loop = num_n - div_n;
    /*
     * Lets setup a 'window' into snum This is the part that corresponds to
     * the current 'area' being divided
     */
    wnum.neg = 0;
    wnum.d = &(snum->d[loop]);
    wnum.top = div_n;
    /*
     * only needed when BN_ucmp messes up the values between top and max
     */
    wnum.dmax = snum->dmax - loop; /* so we don't step out of bounds */

    /* Get the top 2 words of sdiv */
    /* div_n=sdiv->top; */
    d0 = sdiv->d[div_n - 1];
    d1 = (div_n == 1) ? 0 : sdiv->d[div_n - 2];

    /* pointer to the 'top' of snum */
    wnump = &(snum->d[num_n - 1]);

    /* Setup to 'res' */
    res->neg = (num->neg ^ divisor->neg);
    if (!bn_wexpand(res, (loop + 1)))
        goto err;
    res->top = loop - no_branch;
    resp = &(res->d[loop - 1]);

    /* space for temp */
    if (!bn_wexpand(tmp, (div_n + 1)))
        goto err;

    if (!no_branch) {
        if (BN_ucmp(&wnum, sdiv) >= 0) {
            /*
             * If BN_DEBUG_RAND is defined BN_ucmp changes (via bn_pollute)
             * the const bignum arguments => clean the values between top and
             * max again
             */
            bn_clear_top2max(&wnum);
            bn_sub_words(wnum.d, wnum.d, sdiv->d, div_n);
            *resp = 1;
        } else
            res->top--;
    }

    /*
     * if res->top == 0 then clear the neg value otherwise decrease the resp
     * pointer
     */
    if (res->top == 0)
        res->neg = 0;
    else
        resp--;

    for (i = 0; i < loop - 1; i++, wnump--, resp--) {
        BN_ULONG q, l0;
        /*
         * the first part of the loop uses the top two words of snum and sdiv
         * to calculate a BN_ULONG q such that | wnum - sdiv * q | < sdiv
         */
# if defined(BN_DIV3W) && !defined(OPENSSL_NO_ASM)
        BN_ULONG bn_div_3_words(BN_ULONG *, BN_ULONG, BN_ULONG);
        q = bn_div_3_words(wnump, d1, d0);
# else
        BN_ULONG n0, n1, rem = 0;

        n0 = wnump[0];
        n1 = wnump[-1];
        if (n0 == d0)
            q = BN_MASK2;
        else {                  /* n0 < d0 */

#  ifdef BN_LLONG
            BN_ULLONG t2;

#   if defined(BN_LLONG) && defined(BN_DIV2W) && !defined(bn_div_words)
            q = (BN_ULONG)(((((BN_ULLONG) n0) << BN_BITS2) | n1) / d0);
#   else
            q = bn_div_words(n0, n1, d0);
#    ifdef BN_DEBUG_LEVITTE
            fprintf(stderr, "DEBUG: bn_div_words(0x%08X,0x%08X,0x%08\
X) -> 0x%08X\n", n0, n1, d0, q);
#    endif
#   endif

#   ifndef REMAINDER_IS_ALREADY_CALCULATED
            /*
             * rem doesn't have to be BN_ULLONG. The least we
             * know it's less that d0, isn't it?
             */
            rem = (n1 - q * d0) & BN_MASK2;
#   endif
            t2 = (BN_ULLONG) d1 *q;

            for (;;) {
                if (t2 <= ((((BN_ULLONG) rem) << BN_BITS2) | wnump[-2]))
                    break;
                q--;
                rem += d0;
                if (rem < d0)
                    break;      /* don't let rem overflow */
                t2 -= d1;
            }
#  else                         /* !BN_LLONG */
            BN_ULONG t2l, t2h;

            q = bn_div_words(n0, n1, d0);
#   ifdef BN_DEBUG_LEVITTE
            fprintf(stderr, "DEBUG: bn_div_words(0x%08X,0x%08X,0x%08\
X) -> 0x%08X\n", n0, n1, d0, q);
#   endif
#   ifndef REMAINDER_IS_ALREADY_CALCULATED
            rem = (n1 - q * d0) & BN_MASK2;
#   endif

#   if defined(BN_UMULT_LOHI)
            BN_UMULT_LOHI(t2l, t2h, d1, q);
#   elif defined(BN_UMULT_HIGH)
            t2l = d1 * q;
            t2h = BN_UMULT_HIGH(d1, q);
#   else
            {
                BN_ULONG ql, qh;
                t2l = LBITS(d1);
                t2h = HBITS(d1);
                ql = LBITS(q);
                qh = HBITS(q);
                mul64(t2l, t2h, ql, qh); /* t2=(BN_ULLONG)d1*q; */
            }
#   endif

            for (;;) {
                if ((t2h < rem) || ((t2h == rem) && (t2l <= wnump[-2])))
                    break;
                q--;
                rem += d0;
                if (rem < d0)
                    break;      /* don't let rem overflow */
                if (t2l < d1)
                    t2h--;
                t2l -= d1;
            }
#  endif                        /* !BN_LLONG */
        }
# endif                         /* !BN_DIV3W */

        l0 = bn_mul_words(tmp->d, sdiv->d, div_n, q);
        tmp->d[div_n] = l0;
        wnum.d--;
        /*
         * ingore top values of the bignums just sub the two BN_ULONG arrays
         * with bn_sub_words
         */
        if (bn_sub_words(wnum.d, wnum.d, tmp->d, div_n + 1)) {
            /*
             * Note: As we have considered only the leading two BN_ULONGs in
             * the calculation of q, sdiv * q might be greater than wnum (but
             * then (q-1) * sdiv is less or equal than wnum)
             */
            q--;
            if (bn_add_words(wnum.d, wnum.d, sdiv->d, div_n))
                /*
                 * we can't have an overflow here (assuming that q != 0, but
                 * if q == 0 then tmp is zero anyway)
                 */
                (*wnump)++;
        }
        /* store part of the result */
        *resp = q;
    }
    bn_correct_top(snum);
    if (rm != NULL) {
        /*
         * Keep a copy of the neg flag in num because if rm==num BN_rshift()
         * will overwrite it.
         */
        int neg = num->neg;
        BN_rshift(rm, snum, norm_shift);
        if (!BN_is_zero(rm))
            rm->neg = neg;
        bn_check_top(rm);
    }
    if (no_branch)
        bn_correct_top(res);
    BN_CTX_end(ctx);
    return (1);
 err:
    bn_check_top(rm);
    BN_CTX_end(ctx);
    return (0);
}
#endif
