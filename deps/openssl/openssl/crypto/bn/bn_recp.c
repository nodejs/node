/* crypto/bn/bn_recp.c */
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
#include "cryptlib.h"
#include "bn_lcl.h"

void BN_RECP_CTX_init(BN_RECP_CTX *recp)
	{
	BN_init(&(recp->N));
	BN_init(&(recp->Nr));
	recp->num_bits=0;
	recp->flags=0;
	}

BN_RECP_CTX *BN_RECP_CTX_new(void)
	{
	BN_RECP_CTX *ret;

	if ((ret=(BN_RECP_CTX *)OPENSSL_malloc(sizeof(BN_RECP_CTX))) == NULL)
		return(NULL);

	BN_RECP_CTX_init(ret);
	ret->flags=BN_FLG_MALLOCED;
	return(ret);
	}

void BN_RECP_CTX_free(BN_RECP_CTX *recp)
	{
	if(recp == NULL)
	    return;

	BN_free(&(recp->N));
	BN_free(&(recp->Nr));
	if (recp->flags & BN_FLG_MALLOCED)
		OPENSSL_free(recp);
	}

int BN_RECP_CTX_set(BN_RECP_CTX *recp, const BIGNUM *d, BN_CTX *ctx)
	{
	if (!BN_copy(&(recp->N),d)) return 0;
	BN_zero(&(recp->Nr));
	recp->num_bits=BN_num_bits(d);
	recp->shift=0;
	return(1);
	}

int BN_mod_mul_reciprocal(BIGNUM *r, const BIGNUM *x, const BIGNUM *y,
	BN_RECP_CTX *recp, BN_CTX *ctx)
	{
	int ret=0;
	BIGNUM *a;
	const BIGNUM *ca;

	BN_CTX_start(ctx);
	if ((a = BN_CTX_get(ctx)) == NULL) goto err;
	if (y != NULL)
		{
		if (x == y)
			{ if (!BN_sqr(a,x,ctx)) goto err; }
		else
			{ if (!BN_mul(a,x,y,ctx)) goto err; }
		ca = a;
		}
	else
		ca=x; /* Just do the mod */

	ret = BN_div_recp(NULL,r,ca,recp,ctx);
err:
	BN_CTX_end(ctx);
	bn_check_top(r);
	return(ret);
	}

int BN_div_recp(BIGNUM *dv, BIGNUM *rem, const BIGNUM *m,
	BN_RECP_CTX *recp, BN_CTX *ctx)
	{
	int i,j,ret=0;
	BIGNUM *a,*b,*d,*r;

	BN_CTX_start(ctx);
	a=BN_CTX_get(ctx);
	b=BN_CTX_get(ctx);
	if (dv != NULL)
		d=dv;
	else
		d=BN_CTX_get(ctx);
	if (rem != NULL)
		r=rem;
	else
		r=BN_CTX_get(ctx);
	if (a == NULL || b == NULL || d == NULL || r == NULL) goto err;

	if (BN_ucmp(m,&(recp->N)) < 0)
		{
		BN_zero(d);
		if (!BN_copy(r,m)) return 0;
		BN_CTX_end(ctx);
		return(1);
		}

	/* We want the remainder
	 * Given input of ABCDEF / ab
	 * we need multiply ABCDEF by 3 digests of the reciprocal of ab
	 *
	 */

	/* i := max(BN_num_bits(m), 2*BN_num_bits(N)) */
	i=BN_num_bits(m);
	j=recp->num_bits<<1;
	if (j>i) i=j;

	/* Nr := round(2^i / N) */
	if (i != recp->shift)
		recp->shift=BN_reciprocal(&(recp->Nr),&(recp->N),
			i,ctx); /* BN_reciprocal returns i, or -1 for an error */
	if (recp->shift == -1) goto err;

	/* d := |round(round(m / 2^BN_num_bits(N)) * recp->Nr / 2^(i - BN_num_bits(N)))|
	 *    = |round(round(m / 2^BN_num_bits(N)) * round(2^i / N) / 2^(i - BN_num_bits(N)))|
	 *   <= |(m / 2^BN_num_bits(N)) * (2^i / N) * (2^BN_num_bits(N) / 2^i)|
	 *    = |m/N|
	 */
	if (!BN_rshift(a,m,recp->num_bits)) goto err;
	if (!BN_mul(b,a,&(recp->Nr),ctx)) goto err;
	if (!BN_rshift(d,b,i-recp->num_bits)) goto err;
	d->neg=0;

	if (!BN_mul(b,&(recp->N),d,ctx)) goto err;
	if (!BN_usub(r,m,b)) goto err;
	r->neg=0;

#if 1
	j=0;
	while (BN_ucmp(r,&(recp->N)) >= 0)
		{
		if (j++ > 2)
			{
			BNerr(BN_F_BN_DIV_RECP,BN_R_BAD_RECIPROCAL);
			goto err;
			}
		if (!BN_usub(r,r,&(recp->N))) goto err;
		if (!BN_add_word(d,1)) goto err;
		}
#endif

	r->neg=BN_is_zero(r)?0:m->neg;
	d->neg=m->neg^recp->N.neg;
	ret=1;
err:
	BN_CTX_end(ctx);
	bn_check_top(dv);
	bn_check_top(rem);
	return(ret);
	} 

/* len is the expected size of the result
 * We actually calculate with an extra word of precision, so
 * we can do faster division if the remainder is not required.
 */
/* r := 2^len / m */
int BN_reciprocal(BIGNUM *r, const BIGNUM *m, int len, BN_CTX *ctx)
	{
	int ret= -1;
	BIGNUM *t;

	BN_CTX_start(ctx);
	if((t = BN_CTX_get(ctx)) == NULL) goto err;

	if (!BN_set_bit(t,len)) goto err;

	if (!BN_div(r,NULL,t,m,ctx)) goto err;

	ret=len;
err:
	bn_check_top(r);
	BN_CTX_end(ctx);
	return(ret);
	}
