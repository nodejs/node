/* crypto/ec/ec2_mult.c */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The software is originally written by Sheueling Chang Shantz and
 * Douglas Stebila of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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

#include <openssl/err.h>

#include "ec_lcl.h"


/* Compute the x-coordinate x/z for the point 2*(x/z) in Montgomery projective 
 * coordinates.
 * Uses algorithm Mdouble in appendix of 
 *     Lopez, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 * modified to not require precomputation of c=b^{2^{m-1}}.
 */
static int gf2m_Mdouble(const EC_GROUP *group, BIGNUM *x, BIGNUM *z, BN_CTX *ctx)
	{
	BIGNUM *t1;
	int ret = 0;
	
	/* Since Mdouble is static we can guarantee that ctx != NULL. */
	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	if (t1 == NULL) goto err;

	if (!group->meth->field_sqr(group, x, x, ctx)) goto err;
	if (!group->meth->field_sqr(group, t1, z, ctx)) goto err;
	if (!group->meth->field_mul(group, z, x, t1, ctx)) goto err;
	if (!group->meth->field_sqr(group, x, x, ctx)) goto err;
	if (!group->meth->field_sqr(group, t1, t1, ctx)) goto err;
	if (!group->meth->field_mul(group, t1, &group->b, t1, ctx)) goto err;
	if (!BN_GF2m_add(x, x, t1)) goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Compute the x-coordinate x1/z1 for the point (x1/z1)+(x2/x2) in Montgomery 
 * projective coordinates.
 * Uses algorithm Madd in appendix of 
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 */
static int gf2m_Madd(const EC_GROUP *group, const BIGNUM *x, BIGNUM *x1, BIGNUM *z1, 
	const BIGNUM *x2, const BIGNUM *z2, BN_CTX *ctx)
	{
	BIGNUM *t1, *t2;
	int ret = 0;
	
	/* Since Madd is static we can guarantee that ctx != NULL. */
	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	t2 = BN_CTX_get(ctx);
	if (t2 == NULL) goto err;

	if (!BN_copy(t1, x)) goto err;
	if (!group->meth->field_mul(group, x1, x1, z2, ctx)) goto err;
	if (!group->meth->field_mul(group, z1, z1, x2, ctx)) goto err;
	if (!group->meth->field_mul(group, t2, x1, z1, ctx)) goto err;
	if (!BN_GF2m_add(z1, z1, x1)) goto err;
	if (!group->meth->field_sqr(group, z1, z1, ctx)) goto err;
	if (!group->meth->field_mul(group, x1, z1, t1, ctx)) goto err;
	if (!BN_GF2m_add(x1, x1, t2)) goto err;

	ret = 1;

 err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Compute the x, y affine coordinates from the point (x1, z1) (x2, z2) 
 * using Montgomery point multiplication algorithm Mxy() in appendix of 
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 * Returns:
 *     0 on error
 *     1 if return value should be the point at infinity
 *     2 otherwise
 */
static int gf2m_Mxy(const EC_GROUP *group, const BIGNUM *x, const BIGNUM *y, BIGNUM *x1, 
	BIGNUM *z1, BIGNUM *x2, BIGNUM *z2, BN_CTX *ctx)
	{
	BIGNUM *t3, *t4, *t5;
	int ret = 0;
	
	if (BN_is_zero(z1))
		{
		BN_zero(x2);
		BN_zero(z2);
		return 1;
		}
	
	if (BN_is_zero(z2))
		{
		if (!BN_copy(x2, x)) return 0;
		if (!BN_GF2m_add(z2, x, y)) return 0;
		return 2;
		}
		
	/* Since Mxy is static we can guarantee that ctx != NULL. */
	BN_CTX_start(ctx);
	t3 = BN_CTX_get(ctx);
	t4 = BN_CTX_get(ctx);
	t5 = BN_CTX_get(ctx);
	if (t5 == NULL) goto err;

	if (!BN_one(t5)) goto err;

	if (!group->meth->field_mul(group, t3, z1, z2, ctx)) goto err;

	if (!group->meth->field_mul(group, z1, z1, x, ctx)) goto err;
	if (!BN_GF2m_add(z1, z1, x1)) goto err;
	if (!group->meth->field_mul(group, z2, z2, x, ctx)) goto err;
	if (!group->meth->field_mul(group, x1, z2, x1, ctx)) goto err;
	if (!BN_GF2m_add(z2, z2, x2)) goto err;

	if (!group->meth->field_mul(group, z2, z2, z1, ctx)) goto err;
	if (!group->meth->field_sqr(group, t4, x, ctx)) goto err;
	if (!BN_GF2m_add(t4, t4, y)) goto err;
	if (!group->meth->field_mul(group, t4, t4, t3, ctx)) goto err;
	if (!BN_GF2m_add(t4, t4, z2)) goto err;

	if (!group->meth->field_mul(group, t3, t3, x, ctx)) goto err;
	if (!group->meth->field_div(group, t3, t5, t3, ctx)) goto err;
	if (!group->meth->field_mul(group, t4, t3, t4, ctx)) goto err;
	if (!group->meth->field_mul(group, x2, x1, t3, ctx)) goto err;
	if (!BN_GF2m_add(z2, x2, x)) goto err;

	if (!group->meth->field_mul(group, z2, z2, t4, ctx)) goto err;
	if (!BN_GF2m_add(z2, z2, y)) goto err;

	ret = 2;

 err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Computes scalar*point and stores the result in r.
 * point can not equal r.
 * Uses algorithm 2P of
 *     Lopex, J. and Dahab, R.  "Fast multiplication on elliptic curves over 
 *     GF(2^m) without precomputation".
 */
static int ec_GF2m_montgomery_point_multiply(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
	const EC_POINT *point, BN_CTX *ctx)
	{
	BIGNUM *x1, *x2, *z1, *z2;
	int ret = 0, i, j;
	BN_ULONG mask;

	if (r == point)
		{
		ECerr(EC_F_EC_GF2M_MONTGOMERY_POINT_MULTIPLY, EC_R_INVALID_ARGUMENT);
		return 0;
		}
	
	/* if result should be point at infinity */
	if ((scalar == NULL) || BN_is_zero(scalar) || (point == NULL) || 
		EC_POINT_is_at_infinity(group, point))
		{
		return EC_POINT_set_to_infinity(group, r);
		}

	/* only support affine coordinates */
	if (!point->Z_is_one) return 0;

	/* Since point_multiply is static we can guarantee that ctx != NULL. */
	BN_CTX_start(ctx);
	x1 = BN_CTX_get(ctx);
	z1 = BN_CTX_get(ctx);
	if (z1 == NULL) goto err;

	x2 = &r->X;
	z2 = &r->Y;

	if (!BN_GF2m_mod_arr(x1, &point->X, group->poly)) goto err; /* x1 = x */
	if (!BN_one(z1)) goto err; /* z1 = 1 */
	if (!group->meth->field_sqr(group, z2, x1, ctx)) goto err; /* z2 = x1^2 = x^2 */
	if (!group->meth->field_sqr(group, x2, z2, ctx)) goto err;
	if (!BN_GF2m_add(x2, x2, &group->b)) goto err; /* x2 = x^4 + b */

	/* find top most bit and go one past it */
	i = scalar->top - 1; j = BN_BITS2 - 1;
	mask = BN_TBIT;
	while (!(scalar->d[i] & mask)) { mask >>= 1; j--; }
	mask >>= 1; j--;
	/* if top most bit was at word break, go to next word */
	if (!mask) 
		{
		i--; j = BN_BITS2 - 1;
		mask = BN_TBIT;
		}

	for (; i >= 0; i--)
		{
		for (; j >= 0; j--)
			{
			if (scalar->d[i] & mask)
				{
				if (!gf2m_Madd(group, &point->X, x1, z1, x2, z2, ctx)) goto err;
				if (!gf2m_Mdouble(group, x2, z2, ctx)) goto err;
				}
			else
				{
				if (!gf2m_Madd(group, &point->X, x2, z2, x1, z1, ctx)) goto err;
				if (!gf2m_Mdouble(group, x1, z1, ctx)) goto err;
				}
			mask >>= 1;
			}
		j = BN_BITS2 - 1;
		mask = BN_TBIT;
		}

	/* convert out of "projective" coordinates */
	i = gf2m_Mxy(group, &point->X, &point->Y, x1, z1, x2, z2, ctx);
	if (i == 0) goto err;
	else if (i == 1) 
		{
		if (!EC_POINT_set_to_infinity(group, r)) goto err;
		}
	else
		{
		if (!BN_one(&r->Z)) goto err;
		r->Z_is_one = 1;
		}

	/* GF(2^m) field elements should always have BIGNUM::neg = 0 */
	BN_set_negative(&r->X, 0);
	BN_set_negative(&r->Y, 0);

	ret = 1;

 err:
	BN_CTX_end(ctx);
	return ret;
	}


/* Computes the sum
 *     scalar*group->generator + scalars[0]*points[0] + ... + scalars[num-1]*points[num-1]
 * gracefully ignoring NULL scalar values.
 */
int ec_GF2m_simple_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
	size_t num, const EC_POINT *points[], const BIGNUM *scalars[], BN_CTX *ctx)
	{
	BN_CTX *new_ctx = NULL;
	int ret = 0;
	size_t i;
	EC_POINT *p=NULL;
	EC_POINT *acc = NULL;

	if (ctx == NULL)
		{
		ctx = new_ctx = BN_CTX_new();
		if (ctx == NULL)
			return 0;
		}

	/* This implementation is more efficient than the wNAF implementation for 2
	 * or fewer points.  Use the ec_wNAF_mul implementation for 3 or more points,
	 * or if we can perform a fast multiplication based on precomputation.
	 */
	if ((scalar && (num > 1)) || (num > 2) || (num == 0 && EC_GROUP_have_precompute_mult(group)))
		{
		ret = ec_wNAF_mul(group, r, scalar, num, points, scalars, ctx);
		goto err;
		}

	if ((p = EC_POINT_new(group)) == NULL) goto err;
	if ((acc = EC_POINT_new(group)) == NULL) goto err;

	if (!EC_POINT_set_to_infinity(group, acc)) goto err;

	if (scalar)
		{
		if (!ec_GF2m_montgomery_point_multiply(group, p, scalar, group->generator, ctx)) goto err;
		if (BN_is_negative(scalar))
			if (!group->meth->invert(group, p, ctx)) goto err;
		if (!group->meth->add(group, acc, acc, p, ctx)) goto err;
		}

	for (i = 0; i < num; i++)
		{
		if (!ec_GF2m_montgomery_point_multiply(group, p, scalars[i], points[i], ctx)) goto err;
		if (BN_is_negative(scalars[i]))
			if (!group->meth->invert(group, p, ctx)) goto err;
		if (!group->meth->add(group, acc, acc, p, ctx)) goto err;
		}

	if (!EC_POINT_copy(r, acc)) goto err;

	ret = 1;

  err:
	if (p) EC_POINT_free(p);
	if (acc) EC_POINT_free(acc);
	if (new_ctx != NULL)
		BN_CTX_free(new_ctx);
	return ret;
	}


/* Precomputation for point multiplication: fall back to wNAF methods
 * because ec_GF2m_simple_mul() uses ec_wNAF_mul() if appropriate */

int ec_GF2m_precompute_mult(EC_GROUP *group, BN_CTX *ctx)
	{
	return ec_wNAF_precompute_mult(group, ctx);
 	}

int ec_GF2m_have_precompute_mult(const EC_GROUP *group)
	{
	return ec_wNAF_have_precompute_mult(group);
 	}
