/* crypto/ec/ec_cvt.c */
/*
 * Originally written by Bodo Moeller for the OpenSSL project.
 */
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by 
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <openssl/err.h>
#include "ec_lcl.h"


EC_GROUP *EC_GROUP_new_curve_GFp(const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	const EC_METHOD *meth;
	EC_GROUP *ret;

#if defined(OPENSSL_BN_ASM_MONT)
	/*
	 * This might appear controversial, but the fact is that generic
	 * prime method was observed to deliver better performance even
	 * for NIST primes on a range of platforms, e.g.: 60%-15%
	 * improvement on IA-64, ~25% on ARM, 30%-90% on P4, 20%-25%
	 * in 32-bit build and 35%--12% in 64-bit build on Core2...
	 * Coefficients are relative to optimized bn_nist.c for most
	 * intensive ECDSA verify and ECDH operations for 192- and 521-
	 * bit keys respectively. Choice of these boundary values is
	 * arguable, because the dependency of improvement coefficient
	 * from key length is not a "monotone" curve. For example while
	 * 571-bit result is 23% on ARM, 384-bit one is -1%. But it's
	 * generally faster, sometimes "respectfully" faster, sometimes
	 * "tolerably" slower... What effectively happens is that loop
	 * with bn_mul_add_words is put against bn_mul_mont, and the
	 * latter "wins" on short vectors. Correct solution should be
	 * implementing dedicated NxN multiplication subroutines for
	 * small N. But till it materializes, let's stick to generic
	 * prime method...
	 *						<appro>
	 */
	meth = EC_GFp_mont_method();
#else
	meth = EC_GFp_nist_method();
#endif
	
	ret = EC_GROUP_new(meth);
	if (ret == NULL)
		return NULL;

	if (!EC_GROUP_set_curve_GFp(ret, p, a, b, ctx))
		{
		unsigned long err;
		  
		err = ERR_peek_last_error();

		if (!(ERR_GET_LIB(err) == ERR_LIB_EC &&
			((ERR_GET_REASON(err) == EC_R_NOT_A_NIST_PRIME) ||
			 (ERR_GET_REASON(err) == EC_R_NOT_A_SUPPORTED_NIST_PRIME))))
			{
			/* real error */
			
			EC_GROUP_clear_free(ret);
			return NULL;
			}
			
		
		/* not an actual error, we just cannot use EC_GFp_nist_method */

		ERR_clear_error();

		EC_GROUP_clear_free(ret);
		meth = EC_GFp_mont_method();

		ret = EC_GROUP_new(meth);
		if (ret == NULL)
			return NULL;

		if (!EC_GROUP_set_curve_GFp(ret, p, a, b, ctx))
			{
			EC_GROUP_clear_free(ret);
			return NULL;
			}
		}

	return ret;
	}

#ifndef OPENSSL_NO_EC2M
EC_GROUP *EC_GROUP_new_curve_GF2m(const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
	{
	const EC_METHOD *meth;
	EC_GROUP *ret;
	
	meth = EC_GF2m_simple_method();
	
	ret = EC_GROUP_new(meth);
	if (ret == NULL)
		return NULL;

	if (!EC_GROUP_set_curve_GF2m(ret, p, a, b, ctx))
		{
		EC_GROUP_clear_free(ret);
		return NULL;
		}

	return ret;
	}
#endif
