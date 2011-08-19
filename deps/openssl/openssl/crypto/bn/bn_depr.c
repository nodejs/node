/* crypto/bn/bn_depr.c */
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

/* Support for deprecated functions goes here - static linkage will only slurp
 * this code if applications are using them directly. */

#include <stdio.h>
#include <time.h>
#include "cryptlib.h"
#include "bn_lcl.h"
#include <openssl/rand.h>

static void *dummy=&dummy;

#ifndef OPENSSL_NO_DEPRECATED
BIGNUM *BN_generate_prime(BIGNUM *ret, int bits, int safe,
	const BIGNUM *add, const BIGNUM *rem,
	void (*callback)(int,int,void *), void *cb_arg)
	{
	BN_GENCB cb;
	BIGNUM *rnd=NULL;
	int found = 0;

	BN_GENCB_set_old(&cb, callback, cb_arg);

	if (ret == NULL)
		{
		if ((rnd=BN_new()) == NULL) goto err;
		}
	else
		rnd=ret;
	if(!BN_generate_prime_ex(rnd, bits, safe, add, rem, &cb))
		goto err;

	/* we have a prime :-) */
	found = 1;
err:
	if (!found && (ret == NULL) && (rnd != NULL)) BN_free(rnd);
	return(found ? rnd : NULL);
	}

int BN_is_prime(const BIGNUM *a, int checks, void (*callback)(int,int,void *),
	BN_CTX *ctx_passed, void *cb_arg)
	{
	BN_GENCB cb;
	BN_GENCB_set_old(&cb, callback, cb_arg);
	return BN_is_prime_ex(a, checks, ctx_passed, &cb);
	}

int BN_is_prime_fasttest(const BIGNUM *a, int checks,
		void (*callback)(int,int,void *),
		BN_CTX *ctx_passed, void *cb_arg,
		int do_trial_division)
	{
	BN_GENCB cb;
	BN_GENCB_set_old(&cb, callback, cb_arg);
	return BN_is_prime_fasttest_ex(a, checks, ctx_passed,
				do_trial_division, &cb);
	}
#endif
