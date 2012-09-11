/* ====================================================================
 * Copyright (c) 2011 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 */

#include <openssl/opensslconf.h>

#include <stdio.h>
#include <string.h>

#if !defined(OPENSSL_NO_AES) && !defined(OPENSSL_NO_SHA1)

#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include "evp_locl.h"

#ifndef EVP_CIPH_FLAG_AEAD_CIPHER
#define EVP_CIPH_FLAG_AEAD_CIPHER	0x200000
#define EVP_CTRL_AEAD_TLS1_AAD		0x16
#define EVP_CTRL_AEAD_SET_MAC_KEY	0x17
#endif

#if !defined(EVP_CIPH_FLAG_DEFAULT_ASN1)
#define EVP_CIPH_FLAG_DEFAULT_ASN1 0
#endif

#define TLS1_1_VERSION 0x0302

typedef struct
    {
    AES_KEY		ks;
    SHA_CTX		head,tail,md;
    size_t		payload_length;	/* AAD length in decrypt case */
    union {
	unsigned int	tls_ver;
    	unsigned char	tls_aad[16];	/* 13 used */
    } aux;
    } EVP_AES_HMAC_SHA1;

#define NO_PAYLOAD_LENGTH	((size_t)-1)

#if	defined(AES_ASM) &&	( \
	defined(__x86_64)	|| defined(__x86_64__)	|| \
	defined(_M_AMD64)	|| defined(_M_X64)	|| \
	defined(__INTEL__)	)

extern unsigned int OPENSSL_ia32cap_P[2];
#define AESNI_CAPABLE   (1<<(57-32))

int aesni_set_encrypt_key(const unsigned char *userKey, int bits,
			      AES_KEY *key);
int aesni_set_decrypt_key(const unsigned char *userKey, int bits,
			      AES_KEY *key);

void aesni_cbc_encrypt(const unsigned char *in,
			   unsigned char *out,
			   size_t length,
			   const AES_KEY *key,
			   unsigned char *ivec, int enc);

void aesni_cbc_sha1_enc (const void *inp, void *out, size_t blocks,
		const AES_KEY *key, unsigned char iv[16],
		SHA_CTX *ctx,const void *in0);

#define data(ctx) ((EVP_AES_HMAC_SHA1 *)(ctx)->cipher_data)

static int aesni_cbc_hmac_sha1_init_key(EVP_CIPHER_CTX *ctx,
			const unsigned char *inkey,
			const unsigned char *iv, int enc)
	{
	EVP_AES_HMAC_SHA1 *key = data(ctx);
	int ret;

	if (enc)
		ret=aesni_set_encrypt_key(inkey,ctx->key_len*8,&key->ks);
	else
		ret=aesni_set_decrypt_key(inkey,ctx->key_len*8,&key->ks);

	SHA1_Init(&key->head);	/* handy when benchmarking */
	key->tail = key->head;
	key->md   = key->head;

	key->payload_length = NO_PAYLOAD_LENGTH;

	return ret<0?0:1;
	}

#define	STITCHED_CALL

#if !defined(STITCHED_CALL)
#define	aes_off 0
#endif

void sha1_block_data_order (void *c,const void *p,size_t len);

static void sha1_update(SHA_CTX *c,const void *data,size_t len)
{	const unsigned char *ptr = data;
	size_t res;

	if ((res = c->num)) {
		res = SHA_CBLOCK-res;
		if (len<res) res=len;
		SHA1_Update (c,ptr,res);
		ptr += res;
		len -= res;
	}

	res = len % SHA_CBLOCK;
	len -= res;

	if (len) {
		sha1_block_data_order(c,ptr,len/SHA_CBLOCK);

		ptr += len;
		c->Nh += len>>29;
		c->Nl += len<<=3;
		if (c->Nl<(unsigned int)len) c->Nh++;
	}

	if (res)
		SHA1_Update(c,ptr,res);
}

#define SHA1_Update sha1_update

static int aesni_cbc_hmac_sha1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		      const unsigned char *in, size_t len)
	{
	EVP_AES_HMAC_SHA1 *key = data(ctx);
	unsigned int l;
	size_t	plen = key->payload_length,
		iv = 0,		/* explicit IV in TLS 1.1 and later */
		sha_off = 0;
#if defined(STITCHED_CALL)
	size_t	aes_off = 0,
		blocks;

	sha_off = SHA_CBLOCK-key->md.num;
#endif

	if (len%AES_BLOCK_SIZE) return 0;

	if (ctx->encrypt) {
		if (plen==NO_PAYLOAD_LENGTH)
			plen = len;
		else if (len!=((plen+SHA_DIGEST_LENGTH+AES_BLOCK_SIZE)&-AES_BLOCK_SIZE))
			return 0;
		else if (key->aux.tls_ver >= TLS1_1_VERSION)
			iv = AES_BLOCK_SIZE;

#if defined(STITCHED_CALL)
		if (plen>(sha_off+iv) && (blocks=(plen-(sha_off+iv))/SHA_CBLOCK)) {
			SHA1_Update(&key->md,in+iv,sha_off);

			aesni_cbc_sha1_enc(in,out,blocks,&key->ks,
				ctx->iv,&key->md,in+iv+sha_off);
			blocks *= SHA_CBLOCK;
			aes_off += blocks;
			sha_off += blocks;
			key->md.Nh += blocks>>29;
			key->md.Nl += blocks<<=3;
			if (key->md.Nl<(unsigned int)blocks) key->md.Nh++;
		} else {
			sha_off = 0;
		}
#endif
		sha_off += iv;
		SHA1_Update(&key->md,in+sha_off,plen-sha_off);

		if (plen!=len)	{	/* "TLS" mode of operation */
			if (in!=out)
				memcpy(out+aes_off,in+aes_off,plen-aes_off);

			/* calculate HMAC and append it to payload */
			SHA1_Final(out+plen,&key->md);
			key->md = key->tail;
			SHA1_Update(&key->md,out+plen,SHA_DIGEST_LENGTH);
			SHA1_Final(out+plen,&key->md);

			/* pad the payload|hmac */
			plen += SHA_DIGEST_LENGTH;
			for (l=len-plen-1;plen<len;plen++) out[plen]=l;
			/* encrypt HMAC|padding at once */
			aesni_cbc_encrypt(out+aes_off,out+aes_off,len-aes_off,
					&key->ks,ctx->iv,1);
		} else {
			aesni_cbc_encrypt(in+aes_off,out+aes_off,len-aes_off,
					&key->ks,ctx->iv,1);
		}
	} else {
		unsigned char mac[SHA_DIGEST_LENGTH];

		/* decrypt HMAC|padding at once */
		aesni_cbc_encrypt(in,out,len,
				&key->ks,ctx->iv,0);

		if (plen) {	/* "TLS" mode of operation */
			/* figure out payload length */
			if (len<(size_t)(out[len-1]+1+SHA_DIGEST_LENGTH))
				return 0;

			len -= (out[len-1]+1+SHA_DIGEST_LENGTH);

			if ((key->aux.tls_aad[plen-4]<<8|key->aux.tls_aad[plen-3])
			    >= TLS1_1_VERSION) {
				len -= AES_BLOCK_SIZE;
				iv = AES_BLOCK_SIZE;
			}

			key->aux.tls_aad[plen-2] = len>>8;
			key->aux.tls_aad[plen-1] = len;

			/* calculate HMAC and verify it */
			key->md = key->head;
			SHA1_Update(&key->md,key->aux.tls_aad,plen);
			SHA1_Update(&key->md,out+iv,len);
			SHA1_Final(mac,&key->md);

			key->md = key->tail;
			SHA1_Update(&key->md,mac,SHA_DIGEST_LENGTH);
			SHA1_Final(mac,&key->md);

			if (memcmp(out+iv+len,mac,SHA_DIGEST_LENGTH))
				return 0;
		} else {
			SHA1_Update(&key->md,out,len);
		}
	}

	key->payload_length = NO_PAYLOAD_LENGTH;

	return 1;
	}

static int aesni_cbc_hmac_sha1_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
	{
	EVP_AES_HMAC_SHA1 *key = data(ctx);

	switch (type)
		{
	case EVP_CTRL_AEAD_SET_MAC_KEY:
		{
		unsigned int  i;
		unsigned char hmac_key[64];

		memset (hmac_key,0,sizeof(hmac_key));

		if (arg > (int)sizeof(hmac_key)) {
			SHA1_Init(&key->head);
			SHA1_Update(&key->head,ptr,arg);
			SHA1_Final(hmac_key,&key->head);
		} else {
			memcpy(hmac_key,ptr,arg);
		}

		for (i=0;i<sizeof(hmac_key);i++)
			hmac_key[i] ^= 0x36;		/* ipad */
		SHA1_Init(&key->head);
		SHA1_Update(&key->head,hmac_key,sizeof(hmac_key));

		for (i=0;i<sizeof(hmac_key);i++)
			hmac_key[i] ^= 0x36^0x5c;	/* opad */
		SHA1_Init(&key->tail);
		SHA1_Update(&key->tail,hmac_key,sizeof(hmac_key));

		return 1;
		}
	case EVP_CTRL_AEAD_TLS1_AAD:
		{
		unsigned char *p=ptr;
		unsigned int   len=p[arg-2]<<8|p[arg-1];

		if (ctx->encrypt)
			{
			key->payload_length = len;
			if ((key->aux.tls_ver=p[arg-4]<<8|p[arg-3]) >= TLS1_1_VERSION) {
				len -= AES_BLOCK_SIZE;
				p[arg-2] = len>>8;
				p[arg-1] = len;
			}
			key->md = key->head;
			SHA1_Update(&key->md,p,arg);

			return (int)(((len+SHA_DIGEST_LENGTH+AES_BLOCK_SIZE)&-AES_BLOCK_SIZE)
				- len);
			}
		else
			{
			if (arg>13) arg = 13;
			memcpy(key->aux.tls_aad,ptr,arg);
			key->payload_length = arg;

			return SHA_DIGEST_LENGTH;
			}
		}
	default:
		return -1;
		}
	}

static EVP_CIPHER aesni_128_cbc_hmac_sha1_cipher =
	{
#ifdef NID_aes_128_cbc_hmac_sha1
	NID_aes_128_cbc_hmac_sha1,
#else
	NID_undef,
#endif
	16,16,16,
	EVP_CIPH_CBC_MODE|EVP_CIPH_FLAG_DEFAULT_ASN1|EVP_CIPH_FLAG_AEAD_CIPHER,
	aesni_cbc_hmac_sha1_init_key,
	aesni_cbc_hmac_sha1_cipher,
	NULL,
	sizeof(EVP_AES_HMAC_SHA1),
	EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_set_asn1_iv,
	EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_get_asn1_iv,
	aesni_cbc_hmac_sha1_ctrl,
	NULL
	};

static EVP_CIPHER aesni_256_cbc_hmac_sha1_cipher =
	{
#ifdef NID_aes_256_cbc_hmac_sha1
	NID_aes_256_cbc_hmac_sha1,
#else
	NID_undef,
#endif
	16,32,16,
	EVP_CIPH_CBC_MODE|EVP_CIPH_FLAG_DEFAULT_ASN1|EVP_CIPH_FLAG_AEAD_CIPHER,
	aesni_cbc_hmac_sha1_init_key,
	aesni_cbc_hmac_sha1_cipher,
	NULL,
	sizeof(EVP_AES_HMAC_SHA1),
	EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_set_asn1_iv,
	EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_get_asn1_iv,
	aesni_cbc_hmac_sha1_ctrl,
	NULL
	};

const EVP_CIPHER *EVP_aes_128_cbc_hmac_sha1(void)
	{
	return(OPENSSL_ia32cap_P[1]&AESNI_CAPABLE?
		&aesni_128_cbc_hmac_sha1_cipher:NULL);
	}

const EVP_CIPHER *EVP_aes_256_cbc_hmac_sha1(void)
	{
	return(OPENSSL_ia32cap_P[1]&AESNI_CAPABLE?
		&aesni_256_cbc_hmac_sha1_cipher:NULL);
	}
#else
const EVP_CIPHER *EVP_aes_128_cbc_hmac_sha1(void)
	{
	return NULL;
	}
const EVP_CIPHER *EVP_aes_256_cbc_hmac_sha1(void)
	{
	return NULL;
	}
#endif
#endif
