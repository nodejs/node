/* crypto/evp/p_sign.c */
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
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

#ifdef undef
void EVP_SignInit(EVP_MD_CTX *ctx, EVP_MD *type)
	{
	EVP_DigestInit_ex(ctx,type);
	}

void EVP_SignUpdate(EVP_MD_CTX *ctx, unsigned char *data,
	     unsigned int count)
	{
	EVP_DigestUpdate(ctx,data,count);
	}
#endif

int EVP_SignFinal(EVP_MD_CTX *ctx, unsigned char *sigret, unsigned int *siglen,
	     EVP_PKEY *pkey)
	{
	unsigned char m[EVP_MAX_MD_SIZE];
	unsigned int m_len;
	int i=0,ok=0,v;
	EVP_MD_CTX tmp_ctx;
	EVP_PKEY_CTX *pkctx = NULL;

	*siglen=0;
	EVP_MD_CTX_init(&tmp_ctx);
	if (!EVP_MD_CTX_copy_ex(&tmp_ctx,ctx))
		goto err;  
	if (!EVP_DigestFinal_ex(&tmp_ctx,&(m[0]),&m_len))
		goto err;
	EVP_MD_CTX_cleanup(&tmp_ctx);

	if (ctx->digest->flags & EVP_MD_FLAG_PKEY_METHOD_SIGNATURE)
		{
		size_t sltmp = (size_t)EVP_PKEY_size(pkey);
		i = 0;
		pkctx = EVP_PKEY_CTX_new(pkey, NULL);
		if (!pkctx)
			goto err;
		if (EVP_PKEY_sign_init(pkctx) <= 0)
			goto err;
		if (EVP_PKEY_CTX_set_signature_md(pkctx, ctx->digest) <= 0)
			goto err;
		if (EVP_PKEY_sign(pkctx, sigret, &sltmp, m, m_len) <= 0)
			goto err;
		*siglen = sltmp;
		i = 1;
		err:
		EVP_PKEY_CTX_free(pkctx);
		return i;
		}

	for (i=0; i<4; i++)
		{
		v=ctx->digest->required_pkey_type[i];
		if (v == 0) break;
		if (pkey->type == v)
			{
			ok=1;
			break;
			}
		}
	if (!ok)
		{
		EVPerr(EVP_F_EVP_SIGNFINAL,EVP_R_WRONG_PUBLIC_KEY_TYPE);
		return(0);
		}

	if (ctx->digest->sign == NULL)
		{
		EVPerr(EVP_F_EVP_SIGNFINAL,EVP_R_NO_SIGN_FUNCTION_CONFIGURED);
		return(0);
		}
	return(ctx->digest->sign(ctx->digest->type,m,m_len,sigret,siglen,
		pkey->pkey.ptr));
	}

