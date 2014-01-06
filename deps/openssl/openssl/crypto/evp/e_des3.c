/* crypto/evp/e_des3.c */
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
#ifndef OPENSSL_NO_DES
#include <openssl/evp.h>
#include <openssl/objects.h>
#include "evp_locl.h"
#include <openssl/des.h>
#include <openssl/rand.h>

#ifndef OPENSSL_FIPS

static int des_ede_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			    const unsigned char *iv,int enc);

static int des_ede3_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			     const unsigned char *iv,int enc);

static int des3_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr);

typedef struct
    {
    DES_key_schedule ks1;/* key schedule */
    DES_key_schedule ks2;/* key schedule (for ede) */
    DES_key_schedule ks3;/* key schedule (for ede3) */
    } DES_EDE_KEY;

#define data(ctx) ((DES_EDE_KEY *)(ctx)->cipher_data)

/* Because of various casts and different args can't use IMPLEMENT_BLOCK_CIPHER */

static int des_ede_ecb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			      const unsigned char *in, size_t inl)
{
	BLOCK_CIPHER_ecb_loop()
		DES_ecb3_encrypt((const_DES_cblock *)(in + i),
				 (DES_cblock *)(out + i),
				 &data(ctx)->ks1, &data(ctx)->ks2,
				 &data(ctx)->ks3,
				 ctx->encrypt);
	return 1;
}

static int des_ede_ofb_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			      const unsigned char *in, size_t inl)
{
	while (inl>=EVP_MAXCHUNK)
		{
		DES_ede3_ofb64_encrypt(in, out, (long)EVP_MAXCHUNK,
			       &data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
			       (DES_cblock *)ctx->iv, &ctx->num);
		inl-=EVP_MAXCHUNK;
		in +=EVP_MAXCHUNK;
		out+=EVP_MAXCHUNK;
		}
	if (inl)
		DES_ede3_ofb64_encrypt(in, out, (long)inl,
				&data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
                               (DES_cblock *)ctx->iv, &ctx->num);

	return 1;
}

static int des_ede_cbc_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			      const unsigned char *in, size_t inl)
{
#ifdef KSSL_DEBUG
	{
        int i;
        char *cp;
	printf("des_ede_cbc_cipher(ctx=%lx, buflen=%d)\n", ctx, ctx->buf_len);
	printf("\t iv= ");
        for(i=0;i<8;i++)
                printf("%02X",ctx->iv[i]);
	printf("\n");
	}
#endif    /* KSSL_DEBUG */
	while (inl>=EVP_MAXCHUNK)
		{
		DES_ede3_cbc_encrypt(in, out, (long)EVP_MAXCHUNK,
			     &data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
			     (DES_cblock *)ctx->iv, ctx->encrypt);
		inl-=EVP_MAXCHUNK;
		in +=EVP_MAXCHUNK;
		out+=EVP_MAXCHUNK;
		}
	if (inl)
		DES_ede3_cbc_encrypt(in, out, (long)inl,
			     &data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
                             (DES_cblock *)ctx->iv, ctx->encrypt);
	return 1;
}

static int des_ede_cfb64_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			      const unsigned char *in, size_t inl)
{
	while (inl>=EVP_MAXCHUNK)
		{
		DES_ede3_cfb64_encrypt(in, out, (long)EVP_MAXCHUNK, 
			       &data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
			       (DES_cblock *)ctx->iv, &ctx->num, ctx->encrypt);
		inl-=EVP_MAXCHUNK;
		in +=EVP_MAXCHUNK;
		out+=EVP_MAXCHUNK;
		}
	if (inl)
		DES_ede3_cfb64_encrypt(in, out, (long)inl,
			       &data(ctx)->ks1, &data(ctx)->ks2, &data(ctx)->ks3,
                               (DES_cblock *)ctx->iv, &ctx->num, ctx->encrypt);
	return 1;
}

/* Although we have a CFB-r implementation for 3-DES, it doesn't pack the right
   way, so wrap it here */
static int des_ede3_cfb1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
				const unsigned char *in, size_t inl)
    {
    size_t n;
    unsigned char c[1],d[1];

    for(n=0 ; n < inl ; ++n)
	{
	c[0]=(in[n/8]&(1 << (7-n%8))) ? 0x80 : 0;
	DES_ede3_cfb_encrypt(c,d,1,1,
			     &data(ctx)->ks1,&data(ctx)->ks2,&data(ctx)->ks3,
			     (DES_cblock *)ctx->iv,ctx->encrypt);
	out[n/8]=(out[n/8]&~(0x80 >> (unsigned int)(n%8))) |
		 ((d[0]&0x80) >> (unsigned int)(n%8));
	}

    return 1;
    }

static int des_ede3_cfb8_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
				const unsigned char *in, size_t inl)
    {
    while (inl>=EVP_MAXCHUNK)
	{
	DES_ede3_cfb_encrypt(in,out,8,(long)EVP_MAXCHUNK,
			 &data(ctx)->ks1,&data(ctx)->ks2,&data(ctx)->ks3,
			 (DES_cblock *)ctx->iv,ctx->encrypt);
	inl-=EVP_MAXCHUNK;
	in +=EVP_MAXCHUNK;
	out+=EVP_MAXCHUNK;
	}
    if (inl)
	DES_ede3_cfb_encrypt(in,out,8,(long)inl,
			&data(ctx)->ks1,&data(ctx)->ks2,&data(ctx)->ks3,
			(DES_cblock *)ctx->iv,ctx->encrypt);
    return 1;
    }

BLOCK_CIPHER_defs(des_ede, DES_EDE_KEY, NID_des_ede, 8, 16, 8, 64,
			EVP_CIPH_RAND_KEY, des_ede_init_key, NULL, 
			EVP_CIPHER_set_asn1_iv,
			EVP_CIPHER_get_asn1_iv,
			des3_ctrl)

#define des_ede3_cfb64_cipher des_ede_cfb64_cipher
#define des_ede3_ofb_cipher des_ede_ofb_cipher
#define des_ede3_cbc_cipher des_ede_cbc_cipher
#define des_ede3_ecb_cipher des_ede_ecb_cipher

BLOCK_CIPHER_defs(des_ede3, DES_EDE_KEY, NID_des_ede3, 8, 24, 8, 64,
			EVP_CIPH_RAND_KEY, des_ede3_init_key, NULL, 
			EVP_CIPHER_set_asn1_iv,
			EVP_CIPHER_get_asn1_iv,
			des3_ctrl)

BLOCK_CIPHER_def_cfb(des_ede3,DES_EDE_KEY,NID_des_ede3,24,8,1,
		     EVP_CIPH_RAND_KEY, des_ede3_init_key,NULL,
		     EVP_CIPHER_set_asn1_iv,
		     EVP_CIPHER_get_asn1_iv,
		     des3_ctrl)

BLOCK_CIPHER_def_cfb(des_ede3,DES_EDE_KEY,NID_des_ede3,24,8,8,
		     EVP_CIPH_RAND_KEY, des_ede3_init_key,NULL,
		     EVP_CIPHER_set_asn1_iv,
		     EVP_CIPHER_get_asn1_iv,
		     des3_ctrl)

static int des_ede_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			    const unsigned char *iv, int enc)
	{
	DES_cblock *deskey = (DES_cblock *)key;
#ifdef EVP_CHECK_DES_KEY
	if (DES_set_key_checked(&deskey[0],&data(ctx)->ks1)
		!! DES_set_key_checked(&deskey[1],&data(ctx)->ks2))
		return 0;
#else
	DES_set_key_unchecked(&deskey[0],&data(ctx)->ks1);
	DES_set_key_unchecked(&deskey[1],&data(ctx)->ks2);
#endif
	memcpy(&data(ctx)->ks3,&data(ctx)->ks1,
	       sizeof(data(ctx)->ks1));
	return 1;
	}

static int des_ede3_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			     const unsigned char *iv, int enc)
	{
	DES_cblock *deskey = (DES_cblock *)key;
#ifdef KSSL_DEBUG
	{
        int i;
        printf("des_ede3_init_key(ctx=%lx)\n", ctx);
	printf("\tKEY= ");
        for(i=0;i<24;i++) printf("%02X",key[i]); printf("\n");
	printf("\t IV= ");
        for(i=0;i<8;i++) printf("%02X",iv[i]); printf("\n");
	}
#endif	/* KSSL_DEBUG */

#ifdef EVP_CHECK_DES_KEY
	if (DES_set_key_checked(&deskey[0],&data(ctx)->ks1)
		|| DES_set_key_checked(&deskey[1],&data(ctx)->ks2)
		|| DES_set_key_checked(&deskey[2],&data(ctx)->ks3))
		return 0;
#else
	DES_set_key_unchecked(&deskey[0],&data(ctx)->ks1);
	DES_set_key_unchecked(&deskey[1],&data(ctx)->ks2);
	DES_set_key_unchecked(&deskey[2],&data(ctx)->ks3);
#endif
	return 1;
	}

static int des3_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
	{

	DES_cblock *deskey = ptr;

	switch(type)
		{
	case EVP_CTRL_RAND_KEY:
		if (RAND_bytes(ptr, c->key_len) <= 0)
			return 0;
		DES_set_odd_parity(deskey);
		if (c->key_len >= 16)
			DES_set_odd_parity(deskey + 1);
		if (c->key_len >= 24)
			DES_set_odd_parity(deskey + 2);
		return 1;

	default:
		return -1;
		}
	}

const EVP_CIPHER *EVP_des_ede(void)
{
	return &des_ede_ecb;
}

const EVP_CIPHER *EVP_des_ede3(void)
{
	return &des_ede3_ecb;
}
#endif
#endif
