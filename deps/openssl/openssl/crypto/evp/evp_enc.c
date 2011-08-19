/* crypto/evp/evp_enc.c */
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

#ifdef OPENSSL_FIPS
	#define M_do_cipher(ctx, out, in, inl) \
		EVP_Cipher(ctx,out,in,inl)
#else
	#define M_do_cipher(ctx, out, in, inl) \
		ctx->cipher->do_cipher(ctx,out,in,inl)
#endif

const char EVP_version[]="EVP" OPENSSL_VERSION_PTEXT;

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void)
	{
	EVP_CIPHER_CTX *ctx=OPENSSL_malloc(sizeof *ctx);
	if (ctx)
		EVP_CIPHER_CTX_init(ctx);
	return ctx;
	}

int EVP_CipherInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
	     const unsigned char *key, const unsigned char *iv, int enc)
	{
	if (cipher)
		EVP_CIPHER_CTX_init(ctx);
	return EVP_CipherInit_ex(ctx,cipher,NULL,key,iv,enc);
	}

int EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	     const unsigned char *in, int inl)
	{
	if (ctx->encrypt)
		return EVP_EncryptUpdate(ctx,out,outl,in,inl);
	else	return EVP_DecryptUpdate(ctx,out,outl,in,inl);
	}

int EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	if (ctx->encrypt)
		return EVP_EncryptFinal_ex(ctx,out,outl);
	else	return EVP_DecryptFinal_ex(ctx,out,outl);
	}

int EVP_CipherFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	if (ctx->encrypt)
		return EVP_EncryptFinal(ctx,out,outl);
	else	return EVP_DecryptFinal(ctx,out,outl);
	}

int EVP_EncryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
	     const unsigned char *key, const unsigned char *iv)
	{
	return EVP_CipherInit(ctx, cipher, key, iv, 1);
	}

int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx,const EVP_CIPHER *cipher, ENGINE *impl,
		const unsigned char *key, const unsigned char *iv)
	{
	return EVP_CipherInit_ex(ctx, cipher, impl, key, iv, 1);
	}

int EVP_DecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
	     const unsigned char *key, const unsigned char *iv)
	{
	return EVP_CipherInit(ctx, cipher, key, iv, 0);
	}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
	     const unsigned char *key, const unsigned char *iv)
	{
	return EVP_CipherInit_ex(ctx, cipher, impl, key, iv, 0);
	}

int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	     const unsigned char *in, int inl)
	{
	int i,j,bl;

	if (inl <= 0)
		{
		*outl = 0;
		return inl == 0;
		}

	if(ctx->buf_len == 0 && (inl&(ctx->block_mask)) == 0)
		{
		if(M_do_cipher(ctx,out,in,inl))
			{
			*outl=inl;
			return 1;
			}
		else
			{
			*outl=0;
			return 0;
			}
		}
	i=ctx->buf_len;
	bl=ctx->cipher->block_size;
	OPENSSL_assert(bl <= (int)sizeof(ctx->buf));
	if (i != 0)
		{
		if (i+inl < bl)
			{
			memcpy(&(ctx->buf[i]),in,inl);
			ctx->buf_len+=inl;
			*outl=0;
			return 1;
			}
		else
			{
			j=bl-i;
			memcpy(&(ctx->buf[i]),in,j);
			if(!M_do_cipher(ctx,out,ctx->buf,bl)) return 0;
			inl-=j;
			in+=j;
			out+=bl;
			*outl=bl;
			}
		}
	else
		*outl = 0;
	i=inl&(bl-1);
	inl-=i;
	if (inl > 0)
		{
		if(!M_do_cipher(ctx,out,in,inl)) return 0;
		*outl+=inl;
		}

	if (i != 0)
		memcpy(ctx->buf,&(in[inl]),i);
	ctx->buf_len=i;
	return 1;
	}

int EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	int ret;
	ret = EVP_EncryptFinal_ex(ctx, out, outl);
	return ret;
	}

int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	int n,ret;
	unsigned int i, b, bl;

	b=ctx->cipher->block_size;
	OPENSSL_assert(b <= sizeof ctx->buf);
	if (b == 1)
		{
		*outl=0;
		return 1;
		}
	bl=ctx->buf_len;
	if (ctx->flags & EVP_CIPH_NO_PADDING)
		{
		if(bl)
			{
			EVPerr(EVP_F_EVP_ENCRYPTFINAL_EX,EVP_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH);
			return 0;
			}
		*outl = 0;
		return 1;
		}

	n=b-bl;
	for (i=bl; i<b; i++)
		ctx->buf[i]=n;
	ret=M_do_cipher(ctx,out,ctx->buf,b);


	if(ret)
		*outl=b;

	return ret;
	}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	     const unsigned char *in, int inl)
	{
	int fix_len;
	unsigned int b;

	if (inl <= 0)
		{
		*outl = 0;
		return inl == 0;
		}

	if (ctx->flags & EVP_CIPH_NO_PADDING)
		return EVP_EncryptUpdate(ctx, out, outl, in, inl);

	b=ctx->cipher->block_size;
	OPENSSL_assert(b <= sizeof ctx->final);

	if(ctx->final_used)
		{
		memcpy(out,ctx->final,b);
		out+=b;
		fix_len = 1;
		}
	else
		fix_len = 0;


	if(!EVP_EncryptUpdate(ctx,out,outl,in,inl))
		return 0;

	/* if we have 'decrypted' a multiple of block size, make sure
	 * we have a copy of this last block */
	if (b > 1 && !ctx->buf_len)
		{
		*outl-=b;
		ctx->final_used=1;
		memcpy(ctx->final,&out[*outl],b);
		}
	else
		ctx->final_used = 0;

	if (fix_len)
		*outl += b;
		
	return 1;
	}

int EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	int ret;
	ret = EVP_DecryptFinal_ex(ctx, out, outl);
	return ret;
	}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl)
	{
	int i,n;
	unsigned int b;

	*outl=0;
	b=ctx->cipher->block_size;
	if (ctx->flags & EVP_CIPH_NO_PADDING)
		{
		if(ctx->buf_len)
			{
			EVPerr(EVP_F_EVP_DECRYPTFINAL_EX,EVP_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH);
			return 0;
			}
		*outl = 0;
		return 1;
		}
	if (b > 1)
		{
		if (ctx->buf_len || !ctx->final_used)
			{
			EVPerr(EVP_F_EVP_DECRYPTFINAL_EX,EVP_R_WRONG_FINAL_BLOCK_LENGTH);
			return(0);
			}
		OPENSSL_assert(b <= sizeof ctx->final);
		n=ctx->final[b-1];
		if (n == 0 || n > (int)b)
			{
			EVPerr(EVP_F_EVP_DECRYPTFINAL_EX,EVP_R_BAD_DECRYPT);
			return(0);
			}
		for (i=0; i<n; i++)
			{
			if (ctx->final[--b] != n)
				{
				EVPerr(EVP_F_EVP_DECRYPTFINAL_EX,EVP_R_BAD_DECRYPT);
				return(0);
				}
			}
		n=ctx->cipher->block_size-n;
		for (i=0; i<n; i++)
			out[i]=ctx->final[i];
		*outl=n;
		}
	else
		*outl=0;
	return(1);
	}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
	{
	if (ctx)
		{
		EVP_CIPHER_CTX_cleanup(ctx);
		OPENSSL_free(ctx);
		}
	}

int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *c, int keylen)
	{
	if(c->cipher->flags & EVP_CIPH_CUSTOM_KEY_LENGTH) 
		return EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_SET_KEY_LENGTH, keylen, NULL);
	if(c->key_len == keylen) return 1;
	if((keylen > 0) && (c->cipher->flags & EVP_CIPH_VARIABLE_LENGTH))
		{
		c->key_len = keylen;
		return 1;
		}
	EVPerr(EVP_F_EVP_CIPHER_CTX_SET_KEY_LENGTH,EVP_R_INVALID_KEY_LENGTH);
	return 0;
	}

int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad)
	{
	if (pad) ctx->flags &= ~EVP_CIPH_NO_PADDING;
	else ctx->flags |= EVP_CIPH_NO_PADDING;
	return 1;
	}

int EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *ctx, unsigned char *key)
	{
	if (ctx->cipher->flags & EVP_CIPH_RAND_KEY)
		return EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_RAND_KEY, 0, key);
	if (RAND_bytes(key, ctx->key_len) <= 0)
		return 0;
	return 1;
	}

#ifndef OPENSSL_NO_ENGINE

#ifdef OPENSSL_FIPS

static int do_evp_enc_engine_full(EVP_CIPHER_CTX *ctx, const EVP_CIPHER **pcipher, ENGINE *impl)
	{
	if(impl)
		{
		if (!ENGINE_init(impl))
			{
			EVPerr(EVP_F_DO_EVP_ENC_ENGINE_FULL, EVP_R_INITIALIZATION_ERROR);
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
			EVPerr(EVP_F_DO_EVP_ENC_ENGINE_FULL, EVP_R_INITIALIZATION_ERROR);
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

void int_EVP_CIPHER_init_engine_callbacks(void)
	{
	int_EVP_CIPHER_set_engine_callbacks(
		ENGINE_finish, do_evp_enc_engine_full);
	}

#endif

#endif
