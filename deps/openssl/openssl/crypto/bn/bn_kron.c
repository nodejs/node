/* crypto/bn/bn_kron.c */
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

/* least significant word */
#define BN_lsw(n) (((n)->top == 0) ? (BN_ULONG) 0 : (n)->d[0])

/* Returns -2 for errors because both -1 and 0 are valid results. */
int BN_kronecker(const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	int i;
	int ret = -2; /* avoid 'uninitialized' warning */
	int err = 0;
	BIGNUM *A, *B, *tmp;
	/* In 'tab', only odd-indexed entries are relevant:
	 * For any odd BIGNUM n,
	 *     tab[BN_lsw(n) & 7]
	 * is $(-1)^{(n^2-1)/8}$ (using TeX notation).
	 * Note that the sign of n does not matter.
	 */
	static const int tab[8] = {0, 1, 0, -1, 0, -1, 0, 1};

	bn_check_top(a);
	bn_check_top(b);

	BN_CTX_start(ctx);
	A = BN_CTX_get(ctx);
	B = BN_CTX_get(ctx);
	if (B == NULL) goto end;
	
	err = !BN_copy(A, a);
	if (err) goto end;
	err = !BN_copy(B, b);
	if (err) goto end;

	/*
	 * Kronecker symbol, imlemented according to Henri Cohen,
	 * "A Course in Computational Algebraic Number Theory"
	 * (algorithm 1.4.10).
	 */

	/* Cohen's step 1: */

	if (BN_is_zero(B))
		{
		ret = BN_abs_is_word(A, 1);
		goto end;
 		}
	
	/* Cohen's step 2: */

	if (!BN_is_odd(A) && !BN_is_odd(B))
		{
		ret = 0;
		goto end;
		}

	/* now  B  is non-zero */
	i = 0;
	while (!BN_is_bit_set(B, i))
		i++;
	err = !BN_rshift(B, B, i);
	if (err) goto end;
	if (i & 1)
		{
		/* i is odd */
		/* (thus  B  was even, thus  A  must be odd!)  */

		/* set 'ret' to $(-1)^{(A^2-1)/8}$ */
		ret = tab[BN_lsw(A) & 7];
		}
	else
		{
		/* i is even */
		ret = 1;
		}
	
	if (B->neg)
		{
		B->neg = 0;
		if (A->neg)
			ret = -ret;
		}

	/* now  B  is positive and odd, so what remains to be done is
	 * to compute the Jacobi symbol  (A/B)  and multiply it by 'ret' */

	while (1)
		{
		/* Cohen's step 3: */

		/*  B  is positive and odd */

		if (BN_is_zero(A))
			{
			ret = BN_is_one(B) ? ret : 0;
			goto end;
			}

		/* now  A  is non-zero */
		i = 0;
		while (!BN_is_bit_set(A, i))
			i++;
		err = !BN_rshift(A, A, i);
		if (err) goto end;
		if (i & 1)
			{
			/* i is odd */
			/* multiply 'ret' by  $(-1)^{(B^2-1)/8}$ */
			ret = ret * tab[BN_lsw(B) & 7];
			}
	
		/* Cohen's step 4: */
		/* multiply 'ret' by  $(-1)^{(A-1)(B-1)/4}$ */
		if ((A->neg ? ~BN_lsw(A) : BN_lsw(A)) & BN_lsw(B) & 2)
			ret = -ret;
		
		/* (A, B) := (B mod |A|, |A|) */
		err = !BN_nnmod(B, B, A, ctx);
		if (err) goto end;
		tmp = A; A = B; B = tmp;
		tmp->neg = 0;
		}
end:
	BN_CTX_end(ctx);
	if (err)
		return -2;
	else
		return ret;
	}
