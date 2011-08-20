/* crypto/evp/enc_min.c */
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
#include <openssl/err.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include "evp_locl.h"

void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *ctx)
	{
#ifdef OPENSSL_FIPS
	FIPS_selftest_check();
#endif
	memset(ctx,0,sizeof(EVP_CIPHER_CTX));
	/* ctx->cipher=NULL; */
	}

#ifdef OPENSSL_FIPS

/* The purpose of these is to trap programs that attempt to use non FIPS
 * algorithms in FIPS mode and ignore the errors.
 */

static int bad_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		    const unsigned char *iv, int enc)
	{ FIPS_ERROR_IGNORED("Cipher init"); return 0;}

static int bad_do_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			 const unsigned char *in, unsigned int inl)
	{ FIPS_ERROR_IGNORED("Cipher update"); return 0;}

/* NB: no cleanup because it is allowed after failed init */

static int bad_set_asn1(EVP_CIPHER_CTX *ctx, ASN1_TYPE *typ)
	{ FIPS_ERROR_IGNORED("Cipher set_asn1"); return 0;}
static int bad_get_asn1(EVP_CIPHER_CTX *ctx, ASN1_TYPE *typ)
	{ FIPS_ERROR_IGNORED("Cipher get_asn1"); return 0;}
static int bad_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
	{ FIPS_ERROR_IGNORED("Cipher ctrl"); return 0;}

static const EVP_CIPHER bad_cipher =
	{
	0,
	0,
	0,
	0,
	0,
	bad_init,
	bad_do_cipher,
	NULL,
	0,
	bad_set_asn1,
	bad_get_asn1,
	bad_ctrl,
	NULL
	};

#endif

#ifndef OPENSSL_NO_ENGINE

#ifdef OPENSSL_FIPS

static int do_engine_null(ENGINE *impl) { return 0;}
static int do_evp_enc_engine_null(EVP_CIPHER_CTX *ctx,
				const EVP_CIPHER **pciph, ENGINE *impl)
	{ return 1; }

static int (*do_engine_finish)(ENGINE *impl)
		= do_engine_null;

static int (*do_evp_enc_engine)
	(EVP_CIPHER_CTX *ctx, const EVP_CIPHER **pciph, ENGINE *impl)
		= do_evp_enc_engine_null;

void int_EVP_CIPHER_set_engine_callbacks(
	int (*eng_ciph_fin)(ENGINE *impl),
	int (*eng_ciph_evp)
		(EVP_CIPHER_CTX *ctx, const EVP_CIPHER **pciph, ENGINE *impl))
	{
	do_engine_finish = eng_ciph_fin;
	do_evp_enc_engine = eng_ciph_evp;
	}

#else

#define do_engine_finish ENGINE_finish

static int do_evp_enc_engine(EVP_CIPHER_CTX *ctx, const EVP_CIPHER **pcipher, ENGINE *impl)
	{
	if(impl)
		{
		if (!ENGINE_init(impl))
			{
			EVPerr(EVP_F_DO_EVP_ENC_ENGINE, EVP_R_INITIALIZATION_ERROR);
			return 0;
			}
		}
	else
		/* Ask if an ENGINE is reserved for this job */
		impl = ENGINE_get_cipher_engine((*pcipher)->nid);
	if(impl)
		{
		/* There's an ENGINE for this job ... (apparently) */
		const EVP_CIPHER *c = ENGINE_get_cipher(impl, (*pcipher)->nid);
		if(!c)
			{
			/* One positive side-effect of US's export
			 * control history, is that we should at least
			 * be able to avoid using US mispellings of
			 * "initialisation"? */
			EVPerr(EVP_F_DO_EVP_ENC_ENGINE, EVP_R_INITIALIZATION_ERROR);
			return 0;
			}
		/* We'll use the ENGINE's private cipher definition */
		*pcipher = c;
		/* Store the ENGINE functional reference so we know
		 * 'cipher' came from an ENGINE and we need to release
		 * it when done. */
		ctx->engine = impl;
		}
	else
		ctx->engine = NULL;
	return 1;
	}

#endif

#endif

int EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
	     const unsigned char *key, const unsigned char *iv, int enc)
	{
	if (enc == -1)
		enc = ctx->encrypt;
	else
		{
		if (enc)
			enc = 1;
		ctx->encrypt = enc;
		}
#ifdef OPENSSL_FIPS
	if(FIPS_selftest_failed())
		{
		FIPSerr(FIPS_F_EVP_CIPHERINIT_EX,FIPS_R_FIPS_SELFTEST_FAILED);
		ctx->cipher = &bad_cipher;
		return 0;
		}
#endif
#ifndef OPENSSL_NO_ENGINE
	/* Whether it's nice or not, "Inits" can be used on "Final"'d contexts
	 * so this context may already have an ENGINE! Try to avoid releasing
	 * the previous handle, re-querying for an ENGINE, and having a
	 * reinitialisation, when it may all be unecessary. */
	if (ctx->engine && ctx->cipher && (!cipher ||
			(cipher && (cipher->nid == ctx->cipher->nid))))
		goto skip_to_init;
#endif
	if (cipher)
		{
		/* Ensure a context left lying around from last time is cleared
		 * (the previous check attempted to avoid this if the same
		 * ENGINE and EVP_CIPHER could be used). */
		EVP_CIPHER_CTX_cleanup(ctx);

		/* Restore encrypt field: it is zeroed by cleanup */
		ctx->encrypt = enc;
#ifndef OPENSSL_NO_ENGINE
		if (!do_evp_enc_engine(ctx, &cipher, impl))
			return 0;
#endif

		ctx->cipher=cipher;
		if (ctx->cipher->ctx_size)
			{
			ctx->cipher_data=OPENSSL_malloc(ctx->cipher->ctx_size);
			if (!ctx->cipher_data)
				{
				EVPerr(EVP_F_EVP_CIPHERINIT_EX, ERR_R_MALLOC_FAILURE);
				return 0;
				}
			}
		else
			{
			ctx->cipher_data = NULL;
			}
		ctx->key_len = cipher->key_len;
		ctx->flags = 0;
		if(ctx->cipher->flags & EVP_CIPH_CTRL_INIT)
			{
			if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_INIT, 0, NULL))
				{
				EVPerr(EVP_F_EVP_CIPHERINIT_EX, EVP_R_INITIALIZATION_ERROR);
				return 0;
				}
			}
		}
	else if(!ctx->cipher)
		{
		EVPerr(EVP_F_EVP_CIPHERINIT_EX, EVP_R_NO_CIPHER_SET);
		return 0;
		}
#ifndef OPENSSL_NO_ENGINE
skip_to_init:
#endif
	/* we assume block size is a power of 2 in *cryptUpdate */
	OPENSSL_assert(ctx->cipher->block_size == 1
	    || ctx->cipher->block_size == 8
	    || ctx->cipher->block_size == 16);

	if(!(EVP_CIPHER_CTX_flags(ctx) & EVP_CIPH_CUSTOM_IV)) {
		switch(EVP_CIPHER_CTX_mode(ctx)) {

			case EVP_CIPH_STREAM_CIPHER:
			case EVP_CIPH_ECB_MODE:
			break;

			case EVP_CIPH_CFB_MODE:
			case EVP_CIPH_OFB_MODE:

			ctx->num = 0;
			/* fall-through */

			case EVP_CIPH_CBC_MODE:

			OPENSSL_assert(EVP_CIPHER_CTX_iv_length(ctx) <=
					(int)sizeof(ctx->iv));
			if(iv) memcpy(ctx->oiv, iv, EVP_CIPHER_CTX_iv_length(ctx));
			memcpy(ctx->iv, ctx->oiv, EVP_CIPHER_CTX_iv_length(ctx));
			break;

			default:
			return 0;
			break;
		}
	}

#ifdef OPENSSL_FIPS
	/* After 'key' is set no further parameters changes are permissible.
	 * So only check for non FIPS enabling at this point.
	 */
	if (key && FIPS_mode())
		{
		if (!(ctx->cipher->flags & EVP_CIPH_FLAG_FIPS)
			& !(ctx->flags & EVP_CIPH_FLAG_NON_FIPS_ALLOW))
			{
			EVPerr(EVP_F_EVP_CIPHERINIT_EX, EVP_R_DISABLED_FOR_FIPS);
#if 0
			ERR_add_error_data(2, "cipher=",
						EVP_CIPHER_name(ctx->cipher));
#endif
			ctx->cipher = &bad_cipher;
			return 0;
			}
		}
#endif

	if(key || (ctx->cipher->flags & EVP_CIPH_ALWAYS_CALL_INIT)) {
		if(!ctx->cipher->init(ctx,key,iv,enc)) return 0;
	}
	ctx->buf_len=0;
	ctx->final_used=0;
	ctx->block_mask=ctx->cipher->block_size-1;
	return 1;
	}

int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *c)
	{
	if (c->cipher != NULL)
		{
		if(c->cipher->cleanup && !c->cipher->cleanup(c))
			return 0;
		/* Cleanse cipher context data */
		if (c->cipher_data)
			OPENSSL_cleanse(c->cipher_data, c->cipher->ctx_size);
		}
	if (c->cipher_data)
		OPENSSL_free(c->cipher_data);
#ifndef OPENSSL_NO_ENGINE
	if (c->engine)
		/* The EVP_CIPHER we used belongs to an ENGINE, release the
		 * functional reference we held for this reason. */
		do_engine_finish(c->engine);
#endif
	memset(c,0,sizeof(EVP_CIPHER_CTX));
	return 1;
	}

int EVP_Cipher(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in, unsigned int inl)
	{
#ifdef OPENSSL_FIPS
	FIPS_selftest_check();
#endif
	return ctx->cipher->do_cipher(ctx,out,in,inl);
	}

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
	int ret;
	if(!ctx->cipher) {
		EVPerr(EVP_F_EVP_CIPHER_CTX_CTRL, EVP_R_NO_CIPHER_SET);
		return 0;
	}

	if(!ctx->cipher->ctrl) {
		EVPerr(EVP_F_EVP_CIPHER_CTX_CTRL, EVP_R_CTRL_NOT_IMPLEMENTED);
		return 0;
	}

	ret = ctx->cipher->ctrl(ctx, type, arg, ptr);
	if(ret == -1) {
		EVPerr(EVP_F_EVP_CIPHER_CTX_CTRL, EVP_R_CTRL_OPERATION_NOT_IMPLEMENTED);
		return 0;
	}
	return ret;
}

unsigned long EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx)
	{
	return ctx->cipher->flags;
	}

int EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx)
	{
	return ctx->cipher->iv_len;
	}

int EVP_CIPHER_nid(const EVP_CIPHER *cipher)
	{
	return cipher->nid;
	}
