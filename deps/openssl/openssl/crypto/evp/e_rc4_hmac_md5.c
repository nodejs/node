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

#if !defined(OPENSSL_NO_RC4) && !defined(OPENSSL_NO_MD5)

#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>

#ifndef EVP_CIPH_FLAG_AEAD_CIPHER
#define EVP_CIPH_FLAG_AEAD_CIPHER	0x200000
#define EVP_CTRL_AEAD_TLS1_AAD		0x16
#define EVP_CTRL_AEAD_SET_MAC_KEY	0x17
#endif

/* FIXME: surely this is available elsewhere? */
#define EVP_RC4_KEY_SIZE		16

typedef struct
    {
    RC4_KEY		ks;
    MD5_CTX		head,tail,md;
    size_t		payload_length;
    } EVP_RC4_HMAC_MD5;

#define NO_PAYLOAD_LENGTH	((size_t)-1)

void rc4_md5_enc (RC4_KEY *key, const void *in0, void *out,
		MD5_CTX *ctx,const void *inp,size_t blocks);

#define data(ctx) ((EVP_RC4_HMAC_MD5 *)(ctx)->cipher_data)

static int rc4_hmac_md5_init_key(EVP_CIPHER_CTX *ctx,
			const unsigned char *inkey,
			const unsigned char *iv, int enc)
	{
	EVP_RC4_HMAC_MD5 *key = data(ctx);

	RC4_set_key(&key->ks,EVP_CIPHER_CTX_key_length(ctx),
		    inkey);

	MD5_Init(&key->head);	/* handy when benchmarking */
	key->tail = key->head;
	key->md   = key->head;

	key->payload_length = NO_PAYLOAD_LENGTH;

	return 1;
	}

#if	!defined(OPENSSL_NO_ASM) &&	( \
	defined(__x86_64)	|| defined(__x86_64__)	|| \
	defined(_M_AMD64)	|| defined(_M_X64)	|| \
	defined(__INTEL__)		) && \
	!(defined(__APPLE__) && defined(__MACH__))
#define	STITCHED_CALL
#endif

#if !defined(STITCHED_CALL)
#define	rc4_off 0
#define	md5_off 0
#endif

static int rc4_hmac_md5_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
		      const unsigned char *in, size_t len)
	{
	EVP_RC4_HMAC_MD5 *key = data(ctx);
#if defined(STITCHED_CALL)
	size_t	rc4_off = 32-1-(key->ks.x&(32-1)),	/* 32 is $MOD from rc4_md5-x86_64.pl */
		md5_off = MD5_CBLOCK-key->md.num,
		blocks;
	unsigned int l;
	extern unsigned int OPENSSL_ia32cap_P[];
#endif
	size_t	plen = key->payload_length;

	if (plen!=NO_PAYLOAD_LENGTH && len!=(plen+MD5_DIGEST_LENGTH)) return 0;

	if (ctx->encrypt) {
		if (plen==NO_PAYLOAD_LENGTH) plen = len;
#if defined(STITCHED_CALL)
		/* cipher has to "fall behind" */
		if (rc4_off>md5_off) md5_off+=MD5_CBLOCK;

		if (plen>md5_off && (blocks=(plen-md5_off)/MD5_CBLOCK) &&
		    (OPENSSL_ia32cap_P[0]&(1<<20))==0) {
			MD5_Update(&key->md,in,md5_off);
			RC4(&key->ks,rc4_off,in,out);

			rc4_md5_enc(&key->ks,in+rc4_off,out+rc4_off,
				&key->md,in+md5_off,blocks);
			blocks *= MD5_CBLOCK;
			rc4_off += blocks;
			md5_off += blocks;
			key->md.Nh += blocks>>29;
			key->md.Nl += blocks<<=3;
			if (key->md.Nl<(unsigned int)blocks) key->md.Nh++;
		} else {
			rc4_off = 0;
			md5_off = 0;
		}
#endif
		MD5_Update(&key->md,in+md5_off,plen-md5_off);

		if (plen!=len) {	/* "TLS" mode of operation */
			if (in!=out)
				memcpy(out+rc4_off,in+rc4_off,plen-rc4_off);

			/* calculate HMAC and append it to payload */
			MD5_Final(out+plen,&key->md);
			key->md = key->tail;
			MD5_Update(&key->md,out+plen,MD5_DIGEST_LENGTH);
			MD5_Final(out+plen,&key->md);
			/* encrypt HMAC at once */
			RC4(&key->ks,len-rc4_off,out+rc4_off,out+rc4_off);
		} else {
			RC4(&key->ks,len-rc4_off,in+rc4_off,out+rc4_off);
		}
	} else {
		unsigned char mac[MD5_DIGEST_LENGTH];
#if defined(STITCHED_CALL)
		/* digest has to "fall behind" */
		if (md5_off>rc4_off)	rc4_off += 2*MD5_CBLOCK;
		else			rc4_off += MD5_CBLOCK;

		if (len>rc4_off && (blocks=(len-rc4_off)/MD5_CBLOCK) &&
		    (OPENSSL_ia32cap_P[0]&(1<<20))==0) {
			RC4(&key->ks,rc4_off,in,out);
			MD5_Update(&key->md,out,md5_off);

			rc4_md5_enc(&key->ks,in+rc4_off,out+rc4_off,
				&key->md,out+md5_off,blocks);
			blocks *= MD5_CBLOCK;
			rc4_off += blocks;
			md5_off += blocks;
			l = (key->md.Nl+(blocks<<3))&0xffffffffU;
			if (l<key->md.Nl) key->md.Nh++;
			key->md.Nl  = l;
			key->md.Nh += blocks>>29;
		} else {
			md5_off=0;
			rc4_off=0;
		}
#endif
		/* decrypt HMAC at once */
		RC4(&key->ks,len-rc4_off,in+rc4_off,out+rc4_off);
		if (plen!=NO_PAYLOAD_LENGTH) {	/* "TLS" mode of operation */
			MD5_Update(&key->md,out+md5_off,plen-md5_off);

			/* calculate HMAC and verify it */
			MD5_Final(mac,&key->md);
			key->md = key->tail;
			MD5_Update(&key->md,mac,MD5_DIGEST_LENGTH);
			MD5_Final(mac,&key->md);

			if (memcmp(out+plen,mac,MD5_DIGEST_LENGTH))
				return 0;
		} else {
			MD5_Update(&key->md,out+md5_off,len-md5_off);
		}
	}

	key->payload_length = NO_PAYLOAD_LENGTH;

	return 1;
	}

static int rc4_hmac_md5_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
	{
	EVP_RC4_HMAC_MD5 *key = data(ctx);

	switch (type)
		{
	case EVP_CTRL_AEAD_SET_MAC_KEY:
		{
		unsigned int  i;
		unsigned char hmac_key[64];

		memset (hmac_key,0,sizeof(hmac_key));

		if (arg > (int)sizeof(hmac_key)) {
			MD5_Init(&key->head);
			MD5_Update(&key->head,ptr,arg);
			MD5_Final(hmac_key,&key->head);
		} else {
			memcpy(hmac_key,ptr,arg);
		}

		for (i=0;i<sizeof(hmac_key);i++)
			hmac_key[i] ^= 0x36;		/* ipad */
		MD5_Init(&key->head);
		MD5_Update(&key->head,hmac_key,sizeof(hmac_key));

		for (i=0;i<sizeof(hmac_key);i++)
			hmac_key[i] ^= 0x36^0x5c;	/* opad */
		MD5_Init(&key->tail);
		MD5_Update(&key->tail,hmac_key,sizeof(hmac_key));

		return 1;
		}
	case EVP_CTRL_AEAD_TLS1_AAD:
		{
		unsigned char *p=ptr;
		unsigned int   len=p[arg-2]<<8|p[arg-1];

		if (!ctx->encrypt)
			{
			len -= MD5_DIGEST_LENGTH;
			p[arg-2] = len>>8;
			p[arg-1] = len;
			}
		key->payload_length=len;
		key->md = key->head;
		MD5_Update(&key->md,p,arg);

		return MD5_DIGEST_LENGTH;
		}
	default:
		return -1;
		}
	}

static EVP_CIPHER r4_hmac_md5_cipher=
	{
#ifdef NID_rc4_hmac_md5
	NID_rc4_hmac_md5,
#else
	NID_undef,
#endif
	1,EVP_RC4_KEY_SIZE,0,
	EVP_CIPH_STREAM_CIPHER|EVP_CIPH_VARIABLE_LENGTH|EVP_CIPH_FLAG_AEAD_CIPHER,
	rc4_hmac_md5_init_key,
	rc4_hmac_md5_cipher,
	NULL,
	sizeof(EVP_RC4_HMAC_MD5),
	NULL,
	NULL,
	rc4_hmac_md5_ctrl,
	NULL
	};

const EVP_CIPHER *EVP_rc4_hmac_md5(void)
	{
	return(&r4_hmac_md5_cipher);
	}
#endif
