/* crypto/evp/e_rc2.c */
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

#ifndef OPENSSL_NO_RC2

#include <openssl/evp.h>
#include <openssl/objects.h>
#include "evp_locl.h"
#include <openssl/rc2.h>

static int rc2_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			const unsigned char *iv,int enc);
static int rc2_meth_to_magic(EVP_CIPHER_CTX *ctx);
static int rc2_magic_to_meth(int i);
static int rc2_set_asn1_type_and_iv(EVP_CIPHER_CTX *c, ASN1_TYPE *type);
static int rc2_get_asn1_type_and_iv(EVP_CIPHER_CTX *c, ASN1_TYPE *type);
static int rc2_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr);

typedef struct
	{
	int key_bits;	/* effective key bits */
	RC2_KEY ks;	/* key schedule */
	} EVP_RC2_KEY;

#define data(ctx)	((EVP_RC2_KEY *)(ctx)->cipher_data)

IMPLEMENT_BLOCK_CIPHER(rc2, ks, RC2, EVP_RC2_KEY, NID_rc2,
			8,
			RC2_KEY_LENGTH, 8, 64,
			EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
			rc2_init_key, NULL,
			rc2_set_asn1_type_and_iv, rc2_get_asn1_type_and_iv, 
			rc2_ctrl)

#define RC2_40_MAGIC	0xa0
#define RC2_64_MAGIC	0x78
#define RC2_128_MAGIC	0x3a

static const EVP_CIPHER r2_64_cbc_cipher=
	{
	NID_rc2_64_cbc,
	8,8 /* 64 bit */,8,
	EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
	rc2_init_key,
	rc2_cbc_cipher,
	NULL,
	sizeof(EVP_RC2_KEY),
	rc2_set_asn1_type_and_iv,
	rc2_get_asn1_type_and_iv,
	rc2_ctrl,
	NULL
	};

static const EVP_CIPHER r2_40_cbc_cipher=
	{
	NID_rc2_40_cbc,
	8,5 /* 40 bit */,8,
	EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
	rc2_init_key,
	rc2_cbc_cipher,
	NULL,
	sizeof(EVP_RC2_KEY),
	rc2_set_asn1_type_and_iv,
	rc2_get_asn1_type_and_iv,
	rc2_ctrl,
	NULL
	};

const EVP_CIPHER *EVP_rc2_64_cbc(void)
	{
	return(&r2_64_cbc_cipher);
	}

const EVP_CIPHER *EVP_rc2_40_cbc(void)
	{
	return(&r2_40_cbc_cipher);
	}
	
static int rc2_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			const unsigned char *iv, int enc)
	{
	RC2_set_key(&data(ctx)->ks,EVP_CIPHER_CTX_key_length(ctx),
		    key,data(ctx)->key_bits);
	return 1;
	}

static int rc2_meth_to_magic(EVP_CIPHER_CTX *e)
	{
	int i;

	EVP_CIPHER_CTX_ctrl(e, EVP_CTRL_GET_RC2_KEY_BITS, 0, &i);
	if 	(i == 128) return(RC2_128_MAGIC);
	else if (i == 64)  return(RC2_64_MAGIC);
	else if (i == 40)  return(RC2_40_MAGIC);
	else return(0);
	}

static int rc2_magic_to_meth(int i)
	{
	if      (i == RC2_128_MAGIC) return 128;
	else if (i == RC2_64_MAGIC)  return 64;
	else if (i == RC2_40_MAGIC)  return 40;
	else
		{
		EVPerr(EVP_F_RC2_MAGIC_TO_METH,EVP_R_UNSUPPORTED_KEY_SIZE);
		return(0);
		}
	}

static int rc2_get_asn1_type_and_iv(EVP_CIPHER_CTX *c, ASN1_TYPE *type)
	{
	long num=0;
	int i=0;
	int key_bits;
	unsigned int l;
	unsigned char iv[EVP_MAX_IV_LENGTH];

	if (type != NULL)
		{
		l=EVP_CIPHER_CTX_iv_length(c);
		OPENSSL_assert(l <= sizeof(iv));
		i=ASN1_TYPE_get_int_octetstring(type,&num,iv,l);
		if (i != (int)l)
			return(-1);
		key_bits =rc2_magic_to_meth((int)num);
		if (!key_bits)
			return(-1);
		if(i > 0) EVP_CipherInit_ex(c, NULL, NULL, NULL, iv, -1);
		EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_SET_RC2_KEY_BITS, key_bits, NULL);
		EVP_CIPHER_CTX_set_key_length(c, key_bits / 8);
		}
	return(i);
	}

static int rc2_set_asn1_type_and_iv(EVP_CIPHER_CTX *c, ASN1_TYPE *type)
	{
	long num;
	int i=0,j;

	if (type != NULL)
		{
		num=rc2_meth_to_magic(c);
		j=EVP_CIPHER_CTX_iv_length(c);
		i=ASN1_TYPE_set_int_octetstring(type,num,c->oiv,j);
		}
	return(i);
	}

static int rc2_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
	{
	switch(type)
		{
	case EVP_CTRL_INIT:
		data(c)->key_bits = EVP_CIPHER_CTX_key_length(c) * 8;
		return 1;

	case EVP_CTRL_GET_RC2_KEY_BITS:
		*(int *)ptr = data(c)->key_bits;
		return 1;
			
	case EVP_CTRL_SET_RC2_KEY_BITS:
		if(arg > 0)
			{
			data(c)->key_bits = arg;
			return 1;
			}
		return 0;

	default:
		return -1;
		}
	}

#endif
