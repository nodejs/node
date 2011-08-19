/* crypto/rsa/rsa_gen.c */
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
#include <string.h>
#include <time.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#ifndef OPENSSL_FIPS

/* X9.31 RSA key derivation and generation */

int RSA_X931_derive_ex(RSA *rsa, BIGNUM *p1, BIGNUM *p2, BIGNUM *q1, BIGNUM *q2,
			const BIGNUM *Xp1, const BIGNUM *Xp2, const BIGNUM *Xp,
			const BIGNUM *Xq1, const BIGNUM *Xq2, const BIGNUM *Xq,
			const BIGNUM *e, BN_GENCB *cb)
	{
	BIGNUM *r0=NULL,*r1=NULL,*r2=NULL,*r3=NULL;
	BN_CTX *ctx=NULL,*ctx2=NULL;

	if (!rsa) 
		goto err;

	ctx = BN_CTX_new();
	if (!ctx) 
		goto err;
	BN_CTX_start(ctx);

	r0 = BN_CTX_get(ctx);
	r1 = BN_CTX_get(ctx);
	r2 = BN_CTX_get(ctx);
	r3 = BN_CTX_get(ctx);

	if (r3 == NULL)
		goto err;
	if (!rsa->e)
		{
		rsa->e = BN_dup(e);
		if (!rsa->e)
			goto err;
		}
	else
		e = rsa->e;

	/* If not all parameters present only calculate what we can.
	 * This allows test programs to output selective parameters.
	 */

	if (Xp && !rsa->p)
		{
		rsa->p = BN_new();
		if (!rsa->p)
			goto err;

		if (!BN_X931_derive_prime_ex(rsa->p, p1, p2,
					Xp, Xp1, Xp2, e, ctx, cb))
			goto err;
		}

	if (Xq && !rsa->q)
		{
		rsa->q = BN_new();
		if (!rsa->q)
			goto err;
		if (!BN_X931_derive_prime_ex(rsa->q, q1, q2,
					Xq, Xq1, Xq2, e, ctx, cb))
			goto err;
		}

	if (!rsa->p || !rsa->q)
		{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		return 2;
		}

	/* Since both primes are set we can now calculate all remaining 
	 * components.
	 */

	/* calculate n */
	rsa->n=BN_new();
	if (rsa->n == NULL)
		goto err;
	if (!BN_mul(rsa->n,rsa->p,rsa->q,ctx))
		goto err;

	/* calculate d */
	if (!BN_sub(r1,rsa->p,BN_value_one()))
		goto err;	/* p-1 */
	if (!BN_sub(r2,rsa->q,BN_value_one()))
		goto err;	/* q-1 */
	if (!BN_mul(r0,r1,r2,ctx))
		goto err;	/* (p-1)(q-1) */

	if (!BN_gcd(r3, r1, r2, ctx))
		goto err;

	if (!BN_div(r0, NULL, r0, r3, ctx))
		goto err;	/* LCM((p-1)(q-1)) */

	ctx2 = BN_CTX_new();
	if (!ctx2)
		goto err;

	rsa->d=BN_mod_inverse(NULL,rsa->e,r0,ctx2);	/* d */
	if (rsa->d == NULL)
		goto err;

	/* calculate d mod (p-1) */
	rsa->dmp1=BN_new();
	if (rsa->dmp1 == NULL)
		goto err;
	if (!BN_mod(rsa->dmp1,rsa->d,r1,ctx))
		goto err;

	/* calculate d mod (q-1) */
	rsa->dmq1=BN_new();
	if (rsa->dmq1 == NULL)
		goto err;
	if (!BN_mod(rsa->dmq1,rsa->d,r2,ctx))
		goto err;

	/* calculate inverse of q mod p */
	rsa->iqmp=BN_mod_inverse(NULL,rsa->q,rsa->p,ctx2);

	err:
	if (ctx)
		{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		}
	if (ctx2)
		BN_CTX_free(ctx2);
	/* If this is set all calls successful */
	if (rsa && rsa->iqmp != NULL)
		return 1;

	return 0;

	}

int RSA_X931_generate_key_ex(RSA *rsa, int bits, const BIGNUM *e, BN_GENCB *cb)
	{
	int ok = 0;
	BIGNUM *Xp = NULL, *Xq = NULL;
	BN_CTX *ctx = NULL;
	
	ctx = BN_CTX_new();
	if (!ctx)
		goto error;

	BN_CTX_start(ctx);
	Xp = BN_CTX_get(ctx);
	Xq = BN_CTX_get(ctx);
	if (!BN_X931_generate_Xpq(Xp, Xq, bits, ctx))
		goto error;

	rsa->p = BN_new();
	rsa->q = BN_new();
	if (!rsa->p || !rsa->q)
		goto error;

	/* Generate two primes from Xp, Xq */

	if (!BN_X931_generate_prime_ex(rsa->p, NULL, NULL, NULL, NULL, Xp,
					e, ctx, cb))
		goto error;

	if (!BN_X931_generate_prime_ex(rsa->q, NULL, NULL, NULL, NULL, Xq,
					e, ctx, cb))
		goto error;

	/* Since rsa->p and rsa->q are valid this call will just derive
	 * remaining RSA components.
	 */

	if (!RSA_X931_derive_ex(rsa, NULL, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL, NULL, NULL, e, cb))
		goto error;

	ok = 1;

	error:
	if (ctx)
		{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		}

	if (ok)
		return 1;

	return 0;

	}

#endif
