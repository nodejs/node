/* crypto/bn/bn_exp2.c */
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

#include <stdio.h>
#include "cryptlib.h"
#include "bn_lcl.h"

#define TABLE_SIZE      32

int BN_mod_exp2_mont(BIGNUM *rr, const BIGNUM *a1, const BIGNUM *p1,
                     const BIGNUM *a2, const BIGNUM *p2, const BIGNUM *m,
                     BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
    int i, j, bits, b, bits1, bits2, ret =
        0, wpos1, wpos2, window1, window2, wvalue1, wvalue2;
    int r_is_one = 1;
    BIGNUM *d, *r;
    const BIGNUM *a_mod_m;
    /* Tables of variables obtained from 'ctx' */
    BIGNUM *val1[TABLE_SIZE], *val2[TABLE_SIZE];
    BN_MONT_CTX *mont = NULL;

    bn_check_top(a1);
    bn_check_top(p1);
    bn_check_top(a2);
    bn_check_top(p2);
    bn_check_top(m);

    if (!(m->d[0] & 1)) {
        BNerr(BN_F_BN_MOD_EXP2_MONT, BN_R_CALLED_WITH_EVEN_MODULUS);
        return (0);
    }
    bits1 = BN_num_bits(p1);
    bits2 = BN_num_bits(p2);
    if ((bits1 == 0) && (bits2 == 0)) {
        ret = BN_one(rr);
        return ret;
    }

    bits = (bits1 > bits2) ? bits1 : bits2;

    BN_CTX_start(ctx);
    d = BN_CTX_get(ctx);
    r = BN_CTX_get(ctx);
    val1[0] = BN_CTX_get(ctx);
    val2[0] = BN_CTX_get(ctx);
    if (!d || !r || !val1[0] || !val2[0])
        goto err;

    if (in_mont != NULL)
        mont = in_mont;
    else {
        if ((mont = BN_MONT_CTX_new()) == NULL)
            goto err;
        if (!BN_MONT_CTX_set(mont, m, ctx))
            goto err;
    }

    window1 = BN_window_bits_for_exponent_size(bits1);
    window2 = BN_window_bits_for_exponent_size(bits2);

    /*
     * Build table for a1:   val1[i] := a1^(2*i + 1) mod m  for i = 0 .. 2^(window1-1)
     */
    if (a1->neg || BN_ucmp(a1, m) >= 0) {
        if (!BN_mod(val1[0], a1, m, ctx))
            goto err;
        a_mod_m = val1[0];
    } else
        a_mod_m = a1;
    if (BN_is_zero(a_mod_m)) {
        BN_zero(rr);
        ret = 1;
        goto err;
    }

    if (!BN_to_montgomery(val1[0], a_mod_m, mont, ctx))
        goto err;
    if (window1 > 1) {
        if (!BN_mod_mul_montgomery(d, val1[0], val1[0], mont, ctx))
            goto err;

        j = 1 << (window1 - 1);
        for (i = 1; i < j; i++) {
            if (((val1[i] = BN_CTX_get(ctx)) == NULL) ||
                !BN_mod_mul_montgomery(val1[i], val1[i - 1], d, mont, ctx))
                goto err;
        }
    }

    /*
     * Build table for a2:   val2[i] := a2^(2*i + 1) mod m  for i = 0 .. 2^(window2-1)
     */
    if (a2->neg || BN_ucmp(a2, m) >= 0) {
        if (!BN_mod(val2[0], a2, m, ctx))
            goto err;
        a_mod_m = val2[0];
    } else
        a_mod_m = a2;
    if (BN_is_zero(a_mod_m)) {
        BN_zero(rr);
        ret = 1;
        goto err;
    }
    if (!BN_to_montgomery(val2[0], a_mod_m, mont, ctx))
        goto err;
    if (window2 > 1) {
        if (!BN_mod_mul_montgomery(d, val2[0], val2[0], mont, ctx))
            goto err;

        j = 1 << (window2 - 1);
        for (i = 1; i < j; i++) {
            if (((val2[i] = BN_CTX_get(ctx)) == NULL) ||
                !BN_mod_mul_montgomery(val2[i], val2[i - 1], d, mont, ctx))
                goto err;
        }
    }

    /* Now compute the power product, using independent windows. */
    r_is_one = 1;
    wvalue1 = 0;                /* The 'value' of the first window */
    wvalue2 = 0;                /* The 'value' of the second window */
    wpos1 = 0;                  /* If wvalue1 > 0, the bottom bit of the
                                 * first window */
    wpos2 = 0;                  /* If wvalue2 > 0, the bottom bit of the
                                 * second window */

    if (!BN_to_montgomery(r, BN_value_one(), mont, ctx))
        goto err;
    for (b = bits - 1; b >= 0; b--) {
        if (!r_is_one) {
            if (!BN_mod_mul_montgomery(r, r, r, mont, ctx))
                goto err;
        }

        if (!wvalue1)
            if (BN_is_bit_set(p1, b)) {
                /*
                 * consider bits b-window1+1 .. b for this window
                 */
                i = b - window1 + 1;
                while (!BN_is_bit_set(p1, i)) /* works for i<0 */
                    i++;
                wpos1 = i;
                wvalue1 = 1;
                for (i = b - 1; i >= wpos1; i--) {
                    wvalue1 <<= 1;
                    if (BN_is_bit_set(p1, i))
                        wvalue1++;
                }
            }

        if (!wvalue2)
            if (BN_is_bit_set(p2, b)) {
                /*
                 * consider bits b-window2+1 .. b for this window
                 */
                i = b - window2 + 1;
                while (!BN_is_bit_set(p2, i))
                    i++;
                wpos2 = i;
                wvalue2 = 1;
                for (i = b - 1; i >= wpos2; i--) {
                    wvalue2 <<= 1;
                    if (BN_is_bit_set(p2, i))
                        wvalue2++;
                }
            }

        if (wvalue1 && b == wpos1) {
            /* wvalue1 is odd and < 2^window1 */
            if (!BN_mod_mul_montgomery(r, r, val1[wvalue1 >> 1], mont, ctx))
                goto err;
            wvalue1 = 0;
            r_is_one = 0;
        }

        if (wvalue2 && b == wpos2) {
            /* wvalue2 is odd and < 2^window2 */
            if (!BN_mod_mul_montgomery(r, r, val2[wvalue2 >> 1], mont, ctx))
                goto err;
            wvalue2 = 0;
            r_is_one = 0;
        }
    }
    if (!BN_from_montgomery(rr, r, mont, ctx))
        goto err;
    ret = 1;
 err:
    if ((in_mont == NULL) && (mont != NULL))
        BN_MONT_CTX_free(mont);
    BN_CTX_end(ctx);
    bn_check_top(rr);
    return (ret);
}
