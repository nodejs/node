/* crypto/bn/bn_gf2m.c */
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
 * In addition, Sun covenants to all licensees who provide a reciprocal
 * covenant with respect to their own patents if any, not to sue under
 * current and future patent claims necessarily infringed by the making,
 * using, practicing, selling, offering for sale and/or otherwise
 * disposing of the ECC Code as delivered hereunder (or portions thereof),
 * provided that such covenant shall not apply:
 *  1) for code that a licensee deletes from the ECC Code;
 *  2) separates from the ECC Code; or
 *  3) for infringements caused by:
 *       i) the modification of the ECC Code or
 *      ii) the combination of the ECC Code with other software or
 *          devices where such combination causes the infringement.
 *
 * The software is originally written by Sheueling Chang Shantz and
 * Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

/* NOTE: This file is licensed pursuant to the OpenSSL license below
 * and may be modified; but after modifications, the above covenant
 * may no longer apply!  In such cases, the corresponding paragraph
 * ["In addition, Sun covenants ... causes the infringement."] and
 * this note can be edited out; but please keep the Sun copyright
 * notice and attribution. */

/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "cryptlib.h"
#include "bn_lcl.h"

/* Maximum number of iterations before BN_GF2m_mod_solve_quad_arr should fail. */
#define MAX_ITERATIONS 50

static const BN_ULONG SQR_tb[16] =
  {     0,     1,     4,     5,    16,    17,    20,    21,
       64,    65,    68,    69,    80,    81,    84,    85 };
/* Platform-specific macros to accelerate squaring. */
#if defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG)
#define SQR1(w) \
    SQR_tb[(w) >> 60 & 0xF] << 56 | SQR_tb[(w) >> 56 & 0xF] << 48 | \
    SQR_tb[(w) >> 52 & 0xF] << 40 | SQR_tb[(w) >> 48 & 0xF] << 32 | \
    SQR_tb[(w) >> 44 & 0xF] << 24 | SQR_tb[(w) >> 40 & 0xF] << 16 | \
    SQR_tb[(w) >> 36 & 0xF] <<  8 | SQR_tb[(w) >> 32 & 0xF]
#define SQR0(w) \
    SQR_tb[(w) >> 28 & 0xF] << 56 | SQR_tb[(w) >> 24 & 0xF] << 48 | \
    SQR_tb[(w) >> 20 & 0xF] << 40 | SQR_tb[(w) >> 16 & 0xF] << 32 | \
    SQR_tb[(w) >> 12 & 0xF] << 24 | SQR_tb[(w) >>  8 & 0xF] << 16 | \
    SQR_tb[(w) >>  4 & 0xF] <<  8 | SQR_tb[(w)       & 0xF]
#endif
#ifdef THIRTY_TWO_BIT
#define SQR1(w) \
    SQR_tb[(w) >> 28 & 0xF] << 24 | SQR_tb[(w) >> 24 & 0xF] << 16 | \
    SQR_tb[(w) >> 20 & 0xF] <<  8 | SQR_tb[(w) >> 16 & 0xF]
#define SQR0(w) \
    SQR_tb[(w) >> 12 & 0xF] << 24 | SQR_tb[(w) >>  8 & 0xF] << 16 | \
    SQR_tb[(w) >>  4 & 0xF] <<  8 | SQR_tb[(w)       & 0xF]
#endif

/* Product of two polynomials a, b each with degree < BN_BITS2 - 1,
 * result is a polynomial r with degree < 2 * BN_BITS - 1
 * The caller MUST ensure that the variables have the right amount
 * of space allocated.
 */
#ifdef THIRTY_TWO_BIT
static void bn_GF2m_mul_1x1(BN_ULONG *r1, BN_ULONG *r0, const BN_ULONG a, const BN_ULONG b)
	{
	register BN_ULONG h, l, s;
	BN_ULONG tab[8], top2b = a >> 30; 
	register BN_ULONG a1, a2, a4;

	a1 = a & (0x3FFFFFFF); a2 = a1 << 1; a4 = a2 << 1;

	tab[0] =  0; tab[1] = a1;    tab[2] = a2;    tab[3] = a1^a2;
	tab[4] = a4; tab[5] = a1^a4; tab[6] = a2^a4; tab[7] = a1^a2^a4;

	s = tab[b       & 0x7]; l  = s;
	s = tab[b >>  3 & 0x7]; l ^= s <<  3; h  = s >> 29;
	s = tab[b >>  6 & 0x7]; l ^= s <<  6; h ^= s >> 26;
	s = tab[b >>  9 & 0x7]; l ^= s <<  9; h ^= s >> 23;
	s = tab[b >> 12 & 0x7]; l ^= s << 12; h ^= s >> 20;
	s = tab[b >> 15 & 0x7]; l ^= s << 15; h ^= s >> 17;
	s = tab[b >> 18 & 0x7]; l ^= s << 18; h ^= s >> 14;
	s = tab[b >> 21 & 0x7]; l ^= s << 21; h ^= s >> 11;
	s = tab[b >> 24 & 0x7]; l ^= s << 24; h ^= s >>  8;
	s = tab[b >> 27 & 0x7]; l ^= s << 27; h ^= s >>  5;
	s = tab[b >> 30      ]; l ^= s << 30; h ^= s >>  2;

	/* compensate for the top two bits of a */

	if (top2b & 01) { l ^= b << 30; h ^= b >> 2; } 
	if (top2b & 02) { l ^= b << 31; h ^= b >> 1; } 

	*r1 = h; *r0 = l;
	} 
#endif
#if defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG)
static void bn_GF2m_mul_1x1(BN_ULONG *r1, BN_ULONG *r0, const BN_ULONG a, const BN_ULONG b)
	{
	register BN_ULONG h, l, s;
	BN_ULONG tab[16], top3b = a >> 61;
	register BN_ULONG a1, a2, a4, a8;

	a1 = a & (0x1FFFFFFFFFFFFFFFULL); a2 = a1 << 1; a4 = a2 << 1; a8 = a4 << 1;

	tab[ 0] = 0;     tab[ 1] = a1;       tab[ 2] = a2;       tab[ 3] = a1^a2;
	tab[ 4] = a4;    tab[ 5] = a1^a4;    tab[ 6] = a2^a4;    tab[ 7] = a1^a2^a4;
	tab[ 8] = a8;    tab[ 9] = a1^a8;    tab[10] = a2^a8;    tab[11] = a1^a2^a8;
	tab[12] = a4^a8; tab[13] = a1^a4^a8; tab[14] = a2^a4^a8; tab[15] = a1^a2^a4^a8;

	s = tab[b       & 0xF]; l  = s;
	s = tab[b >>  4 & 0xF]; l ^= s <<  4; h  = s >> 60;
	s = tab[b >>  8 & 0xF]; l ^= s <<  8; h ^= s >> 56;
	s = tab[b >> 12 & 0xF]; l ^= s << 12; h ^= s >> 52;
	s = tab[b >> 16 & 0xF]; l ^= s << 16; h ^= s >> 48;
	s = tab[b >> 20 & 0xF]; l ^= s << 20; h ^= s >> 44;
	s = tab[b >> 24 & 0xF]; l ^= s << 24; h ^= s >> 40;
	s = tab[b >> 28 & 0xF]; l ^= s << 28; h ^= s >> 36;
	s = tab[b >> 32 & 0xF]; l ^= s << 32; h ^= s >> 32;
	s = tab[b >> 36 & 0xF]; l ^= s << 36; h ^= s >> 28;
	s = tab[b >> 40 & 0xF]; l ^= s << 40; h ^= s >> 24;
	s = tab[b >> 44 & 0xF]; l ^= s << 44; h ^= s >> 20;
	s = tab[b >> 48 & 0xF]; l ^= s << 48; h ^= s >> 16;
	s = tab[b >> 52 & 0xF]; l ^= s << 52; h ^= s >> 12;
	s = tab[b >> 56 & 0xF]; l ^= s << 56; h ^= s >>  8;
	s = tab[b >> 60      ]; l ^= s << 60; h ^= s >>  4;

	/* compensate for the top three bits of a */

	if (top3b & 01) { l ^= b << 61; h ^= b >> 3; } 
	if (top3b & 02) { l ^= b << 62; h ^= b >> 2; } 
	if (top3b & 04) { l ^= b << 63; h ^= b >> 1; } 

	*r1 = h; *r0 = l;
	} 
#endif

/* Product of two polynomials a, b each with degree < 2 * BN_BITS2 - 1,
 * result is a polynomial r with degree < 4 * BN_BITS2 - 1
 * The caller MUST ensure that the variables have the right amount
 * of space allocated.
 */
static void bn_GF2m_mul_2x2(BN_ULONG *r, const BN_ULONG a1, const BN_ULONG a0, const BN_ULONG b1, const BN_ULONG b0)
	{
	BN_ULONG m1, m0;
	/* r[3] = h1, r[2] = h0; r[1] = l1; r[0] = l0 */
	bn_GF2m_mul_1x1(r+3, r+2, a1, b1);
	bn_GF2m_mul_1x1(r+1, r, a0, b0);
	bn_GF2m_mul_1x1(&m1, &m0, a0 ^ a1, b0 ^ b1);
	/* Correction on m1 ^= l1 ^ h1; m0 ^= l0 ^ h0; */
	r[2] ^= m1 ^ r[1] ^ r[3];  /* h0 ^= m1 ^ l1 ^ h1; */
	r[1] = r[3] ^ r[2] ^ r[0] ^ m1 ^ m0;  /* l1 ^= l0 ^ h0 ^ m0; */
	}


/* Add polynomials a and b and store result in r; r could be a or b, a and b 
 * could be equal; r is the bitwise XOR of a and b.
 */
int	BN_GF2m_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
	{
	int i;
	const BIGNUM *at, *bt;

	bn_check_top(a);
	bn_check_top(b);

	if (a->top < b->top) { at = b; bt = a; }
	else { at = a; bt = b; }

	if(bn_wexpand(r, at->top) == NULL)
		return 0;

	for (i = 0; i < bt->top; i++)
		{
		r->d[i] = at->d[i] ^ bt->d[i];
		}
	for (; i < at->top; i++)
		{
		r->d[i] = at->d[i];
		}
	
	r->top = at->top;
	bn_correct_top(r);
	
	return 1;
	}


/* Some functions allow for representation of the irreducible polynomials
 * as an int[], say p.  The irreducible f(t) is then of the form:
 *     t^p[0] + t^p[1] + ... + t^p[k]
 * where m = p[0] > p[1] > ... > p[k] = 0.
 */


/* Performs modular reduction of a and store result in r.  r could be a. */
int BN_GF2m_mod_arr(BIGNUM *r, const BIGNUM *a, const int p[])
	{
	int j, k;
	int n, dN, d0, d1;
	BN_ULONG zz, *z;

	bn_check_top(a);

	if (!p[0])
		{
		/* reduction mod 1 => return 0 */
		BN_zero(r);
		return 1;
		}

	/* Since the algorithm does reduction in the r value, if a != r, copy
	 * the contents of a into r so we can do reduction in r. 
	 */
	if (a != r)
		{
		if (!bn_wexpand(r, a->top)) return 0;
		for (j = 0; j < a->top; j++)
			{
			r->d[j] = a->d[j];
			}
		r->top = a->top;
		}
	z = r->d;

	/* start reduction */
	dN = p[0] / BN_BITS2;  
	for (j = r->top - 1; j > dN;)
		{
		zz = z[j];
		if (z[j] == 0) { j--; continue; }
		z[j] = 0;

		for (k = 1; p[k] != 0; k++)
			{
			/* reducing component t^p[k] */
			n = p[0] - p[k];
			d0 = n % BN_BITS2;  d1 = BN_BITS2 - d0;
			n /= BN_BITS2; 
			z[j-n] ^= (zz>>d0);
			if (d0) z[j-n-1] ^= (zz<<d1);
			}

		/* reducing component t^0 */
		n = dN;  
		d0 = p[0] % BN_BITS2;
		d1 = BN_BITS2 - d0;
		z[j-n] ^= (zz >> d0);
		if (d0) z[j-n-1] ^= (zz << d1);
		}

	/* final round of reduction */
	while (j == dN)
		{

		d0 = p[0] % BN_BITS2;
		zz = z[dN] >> d0;
		if (zz == 0) break;
		d1 = BN_BITS2 - d0;
		
		/* clear up the top d1 bits */
		if (d0)
			z[dN] = (z[dN] << d1) >> d1;
		else
			z[dN] = 0;
		z[0] ^= zz; /* reduction t^0 component */

		for (k = 1; p[k] != 0; k++)
			{
			BN_ULONG tmp_ulong;

			/* reducing component t^p[k]*/
			n = p[k] / BN_BITS2;   
			d0 = p[k] % BN_BITS2;
			d1 = BN_BITS2 - d0;
			z[n] ^= (zz << d0);
			tmp_ulong = zz >> d1;
                        if (d0 && tmp_ulong)
                                z[n+1] ^= tmp_ulong;
			}

		
		}

	bn_correct_top(r);
	return 1;
	}

/* Performs modular reduction of a by p and store result in r.  r could be a.
 *
 * This function calls down to the BN_GF2m_mod_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_arr function.
 */
int	BN_GF2m_mod(BIGNUM *r, const BIGNUM *a, const BIGNUM *p)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;
	bn_check_top(a);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) * max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_arr(r, a, arr);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}


/* Compute the product of two polynomials a and b, reduce modulo p, and store
 * the result in r.  r could be a or b; a could be b.
 */
int	BN_GF2m_mod_mul_arr(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const int p[], BN_CTX *ctx)
	{
	int zlen, i, j, k, ret = 0;
	BIGNUM *s;
	BN_ULONG x1, x0, y1, y0, zz[4];

	bn_check_top(a);
	bn_check_top(b);

	if (a == b)
		{
		return BN_GF2m_mod_sqr_arr(r, a, p, ctx);
		}

	BN_CTX_start(ctx);
	if ((s = BN_CTX_get(ctx)) == NULL) goto err;
	
	zlen = a->top + b->top + 4;
	if (!bn_wexpand(s, zlen)) goto err;
	s->top = zlen;

	for (i = 0; i < zlen; i++) s->d[i] = 0;

	for (j = 0; j < b->top; j += 2)
		{
		y0 = b->d[j];
		y1 = ((j+1) == b->top) ? 0 : b->d[j+1];
		for (i = 0; i < a->top; i += 2)
			{
			x0 = a->d[i];
			x1 = ((i+1) == a->top) ? 0 : a->d[i+1];
			bn_GF2m_mul_2x2(zz, x1, x0, y1, y0);
			for (k = 0; k < 4; k++) s->d[i+j+k] ^= zz[k];
			}
		}

	bn_correct_top(s);
	if (BN_GF2m_mod_arr(r, s, p))
		ret = 1;
	bn_check_top(r);

err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Compute the product of two polynomials a and b, reduce modulo p, and store
 * the result in r.  r could be a or b; a could equal b.
 *
 * This function calls down to the BN_GF2m_mod_mul_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_mul_arr function.
 */
int	BN_GF2m_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *p, BN_CTX *ctx)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;
	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) * max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD_MUL,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_mul_arr(r, a, b, arr, ctx);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}


/* Square a, reduce the result mod p, and store it in a.  r could be a. */
int	BN_GF2m_mod_sqr_arr(BIGNUM *r, const BIGNUM *a, const int p[], BN_CTX *ctx)
	{
	int i, ret = 0;
	BIGNUM *s;

	bn_check_top(a);
	BN_CTX_start(ctx);
	if ((s = BN_CTX_get(ctx)) == NULL) return 0;
	if (!bn_wexpand(s, 2 * a->top)) goto err;

	for (i = a->top - 1; i >= 0; i--)
		{
		s->d[2*i+1] = SQR1(a->d[i]);
		s->d[2*i  ] = SQR0(a->d[i]);
		}

	s->top = 2 * a->top;
	bn_correct_top(s);
	if (!BN_GF2m_mod_arr(r, s, p)) goto err;
	bn_check_top(r);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Square a, reduce the result mod p, and store it in a.  r could be a.
 *
 * This function calls down to the BN_GF2m_mod_sqr_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_sqr_arr function.
 */
int	BN_GF2m_mod_sqr(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;

	bn_check_top(a);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) * max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD_SQR,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_sqr_arr(r, a, arr, ctx);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}


/* Invert a, reduce modulo p, and store the result in r. r could be a. 
 * Uses Modified Almost Inverse Algorithm (Algorithm 10) from
 *     Hankerson, D., Hernandez, J.L., and Menezes, A.  "Software Implementation
 *     of Elliptic Curve Cryptography Over Binary Fields".
 */
int BN_GF2m_mod_inv(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
	{
	BIGNUM *b, *c, *u, *v, *tmp;
	int ret = 0;

	bn_check_top(a);
	bn_check_top(p);

	BN_CTX_start(ctx);
	
	b = BN_CTX_get(ctx);
	c = BN_CTX_get(ctx);
	u = BN_CTX_get(ctx);
	v = BN_CTX_get(ctx);
	if (v == NULL) goto err;

	if (!BN_one(b)) goto err;
	if (!BN_GF2m_mod(u, a, p)) goto err;
	if (!BN_copy(v, p)) goto err;

	if (BN_is_zero(u)) goto err;

	while (1)
		{
		while (!BN_is_odd(u))
			{
			if (BN_is_zero(u)) goto err;
			if (!BN_rshift1(u, u)) goto err;
			if (BN_is_odd(b))
				{
				if (!BN_GF2m_add(b, b, p)) goto err;
				}
			if (!BN_rshift1(b, b)) goto err;
			}

		if (BN_abs_is_word(u, 1)) break;

		if (BN_num_bits(u) < BN_num_bits(v))
			{
			tmp = u; u = v; v = tmp;
			tmp = b; b = c; c = tmp;
			}
		
		if (!BN_GF2m_add(u, u, v)) goto err;
		if (!BN_GF2m_add(b, b, c)) goto err;
		}


	if (!BN_copy(r, b)) goto err;
	bn_check_top(r);
	ret = 1;

err:
  	BN_CTX_end(ctx);
	return ret;
	}

/* Invert xx, reduce modulo p, and store the result in r. r could be xx. 
 *
 * This function calls down to the BN_GF2m_mod_inv implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_inv function.
 */
int BN_GF2m_mod_inv_arr(BIGNUM *r, const BIGNUM *xx, const int p[], BN_CTX *ctx)
	{
	BIGNUM *field;
	int ret = 0;

	bn_check_top(xx);
	BN_CTX_start(ctx);
	if ((field = BN_CTX_get(ctx)) == NULL) goto err;
	if (!BN_GF2m_arr2poly(p, field)) goto err;
	
	ret = BN_GF2m_mod_inv(r, xx, field, ctx);
	bn_check_top(r);

err:
	BN_CTX_end(ctx);
	return ret;
	}


#ifndef OPENSSL_SUN_GF2M_DIV
/* Divide y by x, reduce modulo p, and store the result in r. r could be x 
 * or y, x could equal y.
 */
int BN_GF2m_mod_div(BIGNUM *r, const BIGNUM *y, const BIGNUM *x, const BIGNUM *p, BN_CTX *ctx)
	{
	BIGNUM *xinv = NULL;
	int ret = 0;

	bn_check_top(y);
	bn_check_top(x);
	bn_check_top(p);

	BN_CTX_start(ctx);
	xinv = BN_CTX_get(ctx);
	if (xinv == NULL) goto err;
	
	if (!BN_GF2m_mod_inv(xinv, x, p, ctx)) goto err;
	if (!BN_GF2m_mod_mul(r, y, xinv, p, ctx)) goto err;
	bn_check_top(r);
	ret = 1;

err:
	BN_CTX_end(ctx);
	return ret;
	}
#else
/* Divide y by x, reduce modulo p, and store the result in r. r could be x 
 * or y, x could equal y.
 * Uses algorithm Modular_Division_GF(2^m) from 
 *     Chang-Shantz, S.  "From Euclid's GCD to Montgomery Multiplication to 
 *     the Great Divide".
 */
int BN_GF2m_mod_div(BIGNUM *r, const BIGNUM *y, const BIGNUM *x, const BIGNUM *p, BN_CTX *ctx)
	{
	BIGNUM *a, *b, *u, *v;
	int ret = 0;

	bn_check_top(y);
	bn_check_top(x);
	bn_check_top(p);

	BN_CTX_start(ctx);
	
	a = BN_CTX_get(ctx);
	b = BN_CTX_get(ctx);
	u = BN_CTX_get(ctx);
	v = BN_CTX_get(ctx);
	if (v == NULL) goto err;

	/* reduce x and y mod p */
	if (!BN_GF2m_mod(u, y, p)) goto err;
	if (!BN_GF2m_mod(a, x, p)) goto err;
	if (!BN_copy(b, p)) goto err;
	
	while (!BN_is_odd(a))
		{
		if (!BN_rshift1(a, a)) goto err;
		if (BN_is_odd(u)) if (!BN_GF2m_add(u, u, p)) goto err;
		if (!BN_rshift1(u, u)) goto err;
		}

	do
		{
		if (BN_GF2m_cmp(b, a) > 0)
			{
			if (!BN_GF2m_add(b, b, a)) goto err;
			if (!BN_GF2m_add(v, v, u)) goto err;
			do
				{
				if (!BN_rshift1(b, b)) goto err;
				if (BN_is_odd(v)) if (!BN_GF2m_add(v, v, p)) goto err;
				if (!BN_rshift1(v, v)) goto err;
				} while (!BN_is_odd(b));
			}
		else if (BN_abs_is_word(a, 1))
			break;
		else
			{
			if (!BN_GF2m_add(a, a, b)) goto err;
			if (!BN_GF2m_add(u, u, v)) goto err;
			do
				{
				if (!BN_rshift1(a, a)) goto err;
				if (BN_is_odd(u)) if (!BN_GF2m_add(u, u, p)) goto err;
				if (!BN_rshift1(u, u)) goto err;
				} while (!BN_is_odd(a));
			}
		} while (1);

	if (!BN_copy(r, u)) goto err;
	bn_check_top(r);
	ret = 1;

err:
  	BN_CTX_end(ctx);
	return ret;
	}
#endif

/* Divide yy by xx, reduce modulo p, and store the result in r. r could be xx 
 * or yy, xx could equal yy.
 *
 * This function calls down to the BN_GF2m_mod_div implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_div function.
 */
int BN_GF2m_mod_div_arr(BIGNUM *r, const BIGNUM *yy, const BIGNUM *xx, const int p[], BN_CTX *ctx)
	{
	BIGNUM *field;
	int ret = 0;

	bn_check_top(yy);
	bn_check_top(xx);

	BN_CTX_start(ctx);
	if ((field = BN_CTX_get(ctx)) == NULL) goto err;
	if (!BN_GF2m_arr2poly(p, field)) goto err;
	
	ret = BN_GF2m_mod_div(r, yy, xx, field, ctx);
	bn_check_top(r);

err:
	BN_CTX_end(ctx);
	return ret;
	}


/* Compute the bth power of a, reduce modulo p, and store
 * the result in r.  r could be a.
 * Uses simple square-and-multiply algorithm A.5.1 from IEEE P1363.
 */
int	BN_GF2m_mod_exp_arr(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const int p[], BN_CTX *ctx)
	{
	int ret = 0, i, n;
	BIGNUM *u;

	bn_check_top(a);
	bn_check_top(b);

	if (BN_is_zero(b))
		return(BN_one(r));

	if (BN_abs_is_word(b, 1))
		return (BN_copy(r, a) != NULL);

	BN_CTX_start(ctx);
	if ((u = BN_CTX_get(ctx)) == NULL) goto err;
	
	if (!BN_GF2m_mod_arr(u, a, p)) goto err;
	
	n = BN_num_bits(b) - 1;
	for (i = n - 1; i >= 0; i--)
		{
		if (!BN_GF2m_mod_sqr_arr(u, u, p, ctx)) goto err;
		if (BN_is_bit_set(b, i))
			{
			if (!BN_GF2m_mod_mul_arr(u, u, a, p, ctx)) goto err;
			}
		}
	if (!BN_copy(r, u)) goto err;
	bn_check_top(r);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Compute the bth power of a, reduce modulo p, and store
 * the result in r.  r could be a.
 *
 * This function calls down to the BN_GF2m_mod_exp_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_exp_arr function.
 */
int BN_GF2m_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *p, BN_CTX *ctx)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;
	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) * max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD_EXP,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_exp_arr(r, a, b, arr, ctx);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}

/* Compute the square root of a, reduce modulo p, and store
 * the result in r.  r could be a.
 * Uses exponentiation as in algorithm A.4.1 from IEEE P1363.
 */
int	BN_GF2m_mod_sqrt_arr(BIGNUM *r, const BIGNUM *a, const int p[], BN_CTX *ctx)
	{
	int ret = 0;
	BIGNUM *u;

	bn_check_top(a);

	if (!p[0])
		{
		/* reduction mod 1 => return 0 */
		BN_zero(r);
		return 1;
		}

	BN_CTX_start(ctx);
	if ((u = BN_CTX_get(ctx)) == NULL) goto err;
	
	if (!BN_set_bit(u, p[0] - 1)) goto err;
	ret = BN_GF2m_mod_exp_arr(r, a, u, p, ctx);
	bn_check_top(r);

err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Compute the square root of a, reduce modulo p, and store
 * the result in r.  r could be a.
 *
 * This function calls down to the BN_GF2m_mod_sqrt_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_sqrt_arr function.
 */
int BN_GF2m_mod_sqrt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;
	bn_check_top(a);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) * max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD_SQRT,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_sqrt_arr(r, a, arr, ctx);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}

/* Find r such that r^2 + r = a mod p.  r could be a. If no r exists returns 0.
 * Uses algorithms A.4.7 and A.4.6 from IEEE P1363.
 */
int BN_GF2m_mod_solve_quad_arr(BIGNUM *r, const BIGNUM *a_, const int p[], BN_CTX *ctx)
	{
	int ret = 0, count = 0, j;
	BIGNUM *a, *z, *rho, *w, *w2, *tmp;

	bn_check_top(a_);

	if (!p[0])
		{
		/* reduction mod 1 => return 0 */
		BN_zero(r);
		return 1;
		}

	BN_CTX_start(ctx);
	a = BN_CTX_get(ctx);
	z = BN_CTX_get(ctx);
	w = BN_CTX_get(ctx);
	if (w == NULL) goto err;

	if (!BN_GF2m_mod_arr(a, a_, p)) goto err;
	
	if (BN_is_zero(a))
		{
		BN_zero(r);
		ret = 1;
		goto err;
		}

	if (p[0] & 0x1) /* m is odd */
		{
		/* compute half-trace of a */
		if (!BN_copy(z, a)) goto err;
		for (j = 1; j <= (p[0] - 1) / 2; j++)
			{
			if (!BN_GF2m_mod_sqr_arr(z, z, p, ctx)) goto err;
			if (!BN_GF2m_mod_sqr_arr(z, z, p, ctx)) goto err;
			if (!BN_GF2m_add(z, z, a)) goto err;
			}
		
		}
	else /* m is even */
		{
		rho = BN_CTX_get(ctx);
		w2 = BN_CTX_get(ctx);
		tmp = BN_CTX_get(ctx);
		if (tmp == NULL) goto err;
		do
			{
			if (!BN_rand(rho, p[0], 0, 0)) goto err;
			if (!BN_GF2m_mod_arr(rho, rho, p)) goto err;
			BN_zero(z);
			if (!BN_copy(w, rho)) goto err;
			for (j = 1; j <= p[0] - 1; j++)
				{
				if (!BN_GF2m_mod_sqr_arr(z, z, p, ctx)) goto err;
				if (!BN_GF2m_mod_sqr_arr(w2, w, p, ctx)) goto err;
				if (!BN_GF2m_mod_mul_arr(tmp, w2, a, p, ctx)) goto err;
				if (!BN_GF2m_add(z, z, tmp)) goto err;
				if (!BN_GF2m_add(w, w2, rho)) goto err;
				}
			count++;
			} while (BN_is_zero(w) && (count < MAX_ITERATIONS));
		if (BN_is_zero(w))
			{
			BNerr(BN_F_BN_GF2M_MOD_SOLVE_QUAD_ARR,BN_R_TOO_MANY_ITERATIONS);
			goto err;
			}
		}
	
	if (!BN_GF2m_mod_sqr_arr(w, z, p, ctx)) goto err;
	if (!BN_GF2m_add(w, z, w)) goto err;
	if (BN_GF2m_cmp(w, a))
		{
		BNerr(BN_F_BN_GF2M_MOD_SOLVE_QUAD_ARR, BN_R_NO_SOLUTION);
		goto err;
		}

	if (!BN_copy(r, z)) goto err;
	bn_check_top(r);

	ret = 1;

err:
	BN_CTX_end(ctx);
	return ret;
	}

/* Find r such that r^2 + r = a mod p.  r could be a. If no r exists returns 0.
 *
 * This function calls down to the BN_GF2m_mod_solve_quad_arr implementation; this wrapper
 * function is only provided for convenience; for best performance, use the 
 * BN_GF2m_mod_solve_quad_arr function.
 */
int BN_GF2m_mod_solve_quad(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
	{
	int ret = 0;
	const int max = BN_num_bits(p) + 1;
	int *arr=NULL;
	bn_check_top(a);
	bn_check_top(p);
	if ((arr = (int *)OPENSSL_malloc(sizeof(int) *
						max)) == NULL) goto err;
	ret = BN_GF2m_poly2arr(p, arr, max);
	if (!ret || ret > max)
		{
		BNerr(BN_F_BN_GF2M_MOD_SOLVE_QUAD,BN_R_INVALID_LENGTH);
		goto err;
		}
	ret = BN_GF2m_mod_solve_quad_arr(r, a, arr, ctx);
	bn_check_top(r);
err:
	if (arr) OPENSSL_free(arr);
	return ret;
	}

/* Convert the bit-string representation of a polynomial
 * ( \sum_{i=0}^n a_i * x^i) into an array of integers corresponding 
 * to the bits with non-zero coefficient.  Array is terminated with -1.
 * Up to max elements of the array will be filled.  Return value is total
 * number of array elements that would be filled if array was large enough.
 */
int BN_GF2m_poly2arr(const BIGNUM *a, int p[], int max)
	{
	int i, j, k = 0;
	BN_ULONG mask;

	if (BN_is_zero(a))
		return 0;

	for (i = a->top - 1; i >= 0; i--)
		{
		if (!a->d[i])
			/* skip word if a->d[i] == 0 */
			continue;
		mask = BN_TBIT;
		for (j = BN_BITS2 - 1; j >= 0; j--)
			{
			if (a->d[i] & mask) 
				{
				if (k < max) p[k] = BN_BITS2 * i + j;
				k++;
				}
			mask >>= 1;
			}
		}

	if (k < max) {
		p[k] = -1;
		k++;
	}

	return k;
	}

/* Convert the coefficient array representation of a polynomial to a 
 * bit-string.  The array must be terminated by -1.
 */
int BN_GF2m_arr2poly(const int p[], BIGNUM *a)
	{
	int i;

	bn_check_top(a);
	BN_zero(a);
	for (i = 0; p[i] != -1; i++)
		{
		if (BN_set_bit(a, p[i]) == 0)
			return 0;
		}
	bn_check_top(a);

	return 1;
	}

