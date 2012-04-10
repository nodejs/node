/* crypto/rand/rand_lib.c */
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
#include <time.h>
#include "cryptlib.h"
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

#ifndef OPENSSL_NO_ENGINE
/* non-NULL if default_RAND_meth is ENGINE-provided */
static ENGINE *funct_ref =NULL;
#endif
static const RAND_METHOD *default_RAND_meth = NULL;

int RAND_set_rand_method(const RAND_METHOD *meth)
	{
#ifndef OPENSSL_NO_ENGINE
	if(funct_ref)
		{
		ENGINE_finish(funct_ref);
		funct_ref = NULL;
		}
#endif
	default_RAND_meth = meth;
	return 1;
	}

const RAND_METHOD *RAND_get_rand_method(void)
	{
	if (!default_RAND_meth)
		{
#ifndef OPENSSL_NO_ENGINE
		ENGINE *e = ENGINE_get_default_RAND();
		if(e)
			{
			default_RAND_meth = ENGINE_get_RAND(e);
			if(!default_RAND_meth)
				{
				ENGINE_finish(e);
				e = NULL;
				}
			}
		if(e)
			funct_ref = e;
		else
#endif
			default_RAND_meth = RAND_SSLeay();
		}
	return default_RAND_meth;
	}

#ifndef OPENSSL_NO_ENGINE
int RAND_set_rand_engine(ENGINE *engine)
	{
	const RAND_METHOD *tmp_meth = NULL;
	if(engine)
		{
		if(!ENGINE_init(engine))
			return 0;
		tmp_meth = ENGINE_get_RAND(engine);
		if(!tmp_meth)
			{
			ENGINE_finish(engine);
			return 0;
			}
		}
	/* This function releases any prior ENGINE so call it first */
	RAND_set_rand_method(tmp_meth);
	funct_ref = engine;
	return 1;
	}
#endif

void RAND_cleanup(void)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->cleanup)
		meth->cleanup();
	RAND_set_rand_method(NULL);
	}

void RAND_seed(const void *buf, int num)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->seed)
		meth->seed(buf,num);
	}

void RAND_add(const void *buf, int num, double entropy)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->add)
		meth->add(buf,num,entropy);
	}

int RAND_bytes(unsigned char *buf, int num)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->bytes)
		return meth->bytes(buf,num);
	return(-1);
	}

int RAND_pseudo_bytes(unsigned char *buf, int num)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->pseudorand)
		return meth->pseudorand(buf,num);
	return(-1);
	}

int RAND_status(void)
	{
	const RAND_METHOD *meth = RAND_get_rand_method();
	if (meth && meth->status)
		return meth->status();
	return 0;
	}
