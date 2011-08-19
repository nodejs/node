/* crypto/bn/bn_mod.c */
/* Includes code written by Lenka Fibikova <fibikova@exp-math.uni-essen.de>
 * for the OpenSSL project. */
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

#include "cryptlib.h"
#include "bn_lcl.h"


#if 0 /* now just a #define */
int BN_mod(BIGNUM *rem, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
	{
	return(BN_div(NULL,rem,m,d,ctx));
	/* note that  rem->neg == m->neg  (unless the remainder is zero) */
	}
#endif


int BN_nnmod(BIGNUM *r, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
	{
	/* like BN_mod, but returns non-negative remainder
	 * (i.e.,  0 <= r < |d|  always holds) */

	if (!(BN_mod(r,m,d,ctx)))
		return 0;
	if (!r->neg)
		return 1;
	/* now   -|d| < r < 0,  so we have to set  r := r + |d| */
	return (d->neg ? BN_sub : BN_add)(r, r, d);
}


int BN_mod_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
	{
	if (!BN_add(r, a, b)) return 0;
	return BN_nnmod(r, r, m, ctx);
	}


/* BN_mod_add variant that may be used if both  a  and  b  are non-negative
 * and less than  m */
int BN_mod_add_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
	{
	if (!BN_uadd(r, a, b)) return 0;
	if (BN_ucmp(r, m) >= 0)
		return BN_usub(r, r, m);
	return 1;
	}


int BN_mod_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
	{
	if (!BN_sub(r, a, b)) return 0;
	return BN_nnmod(r, r, m, ctx);
	}


/* BN_mod_sub variant that may be used if both  a  and  b  are non-negative
 * and less than  m */
int BN_mod_sub_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
	{
	if (!BN_sub(r, a, b)) return 0;
	if (r->neg)
		return BN_add(r, r, m);
	return 1;
	}


/* slow but works */
int BN_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m,
	BN_CTX *ctx)
	{
	BIGNUM *t;
	int ret=0;

	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(m);

	BN_CTX_start(ctx);
	if ((t = BN_CTX_get(ctx)) == NULL) goto err;
	if (a == b)
		{ if (!BN_sqr(t,a,ctx)) goto err; }
	else
		{ if (!BN_mul(t,a,b,ctx)) goto err; }
	if (!BN_nnmod(r,t,m,ctx)) goto err;
	bn_check_top(r);
	ret=1;
err:
	BN_CTX_end(ctx);
	return(ret);
	}


int BN_mod_sqr(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx)
	{
	if (!BN_sqr(r, a, ctx)) return 0;
	/* r->neg == 0,  thus we don't need BN_nnmod */
	return BN_mod(r, r, m, ctx);
	}


int BN_mod_lshift1(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx)
	{
	if (!BN_lshift1(r, a)) return 0;
	bn_check_top(r);
	return BN_nnmod(r, r, m, ctx);
	}


/* BN_mod_lshift1 variant that may be used if  a  is non-negative
 * and less than  m */
int BN_mod_lshift1_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *m)
	{
	if (!BN_lshift1(r, a)) return 0;
	bn_check_top(r);
	if (BN_cmp(r, m) >= 0)
		return BN_sub(r, r, m);
	return 1;
	}


int BN_mod_lshift(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m, BN_CTX *ctx)
	{
	BIGNUM *abs_m = NULL;
	int ret;

	if (!BN_nnmod(r, a, m, ctx)) return 0;

	if (m->neg)
		{
		abs_m = BN_dup(m);
		if (abs_m == NULL) return 0;
		abs_m->neg = 0;
		}
	
	ret = BN_mod_lshift_quick(r, r, n, (abs_m ? abs_m : m));
	bn_check_top(r);

	if (abs_m)
		BN_free(abs_m);
	return ret;
	}


/* BN_mod_lshift variant that may be used if  a  is non-negative
 * and less than  m */
int BN_mod_lshift_quick(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m)
	{
	if (r != a)
		{
		if (BN_copy(r, a) == NULL) return 0;
		}

	while (n > 0)
		{
		int max_shift;
		
		/* 0 < r < m */
		max_shift = BN_num_bits(m) - BN_num_bits(r);
		/* max_shift >= 0 */

		if (max_shift < 0)
			{
			BNerr(BN_F_BN_MOD_LSHIFT_QUICK, BN_R_INPUT_NOT_REDUCED);
			return 0;
			}

		if (max_shift > n)
			max_shift = n;

		if (max_shift)
			{
			if (!BN_lshift(r, r, max_shift)) return 0;
			n -= max_shift;
			}
		else
			{
			if (!BN_lshift1(r, r)) return 0;
			--n;
			}

		/* BN_num_bits(r) <= BN_num_bits(m) */

		if (BN_cmp(r, m) >= 0) 
			{
			if (!BN_sub(r, r, m)) return 0;
			}
		}
	bn_check_top(r);
	
	return 1;
	}
