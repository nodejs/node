/* rsa_pss.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project.  All rights reserved.
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
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

static const unsigned char zeroes[] = {0,0,0,0,0,0,0,0};

#if defined(_MSC_VER) && defined(_ARM_)
#pragma optimize("g", off)
#endif

int RSA_verify_PKCS1_PSS(RSA *rsa, const unsigned char *mHash,
			const EVP_MD *Hash, const unsigned char *EM, int sLen)
	{
	int i;
	int ret = 0;
	int hLen, maskedDBLen, MSBits, emLen;
	const unsigned char *H;
	unsigned char *DB = NULL;
	EVP_MD_CTX ctx;
	unsigned char H_[EVP_MAX_MD_SIZE];

	hLen = M_EVP_MD_size(Hash);
	/*
	 * Negative sLen has special meanings:
	 *	-1	sLen == hLen
	 *	-2	salt length is autorecovered from signature
	 *	-N	reserved
	 */
	if      (sLen == -1)	sLen = hLen;
	else if (sLen == -2)	sLen = -2;
	else if (sLen < -2)
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_SLEN_CHECK_FAILED);
		goto err;
		}

	MSBits = (BN_num_bits(rsa->n) - 1) & 0x7;
	emLen = RSA_size(rsa);
	if (EM[0] & (0xFF << MSBits))
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_FIRST_OCTET_INVALID);
		goto err;
		}
	if (MSBits == 0)
		{
		EM++;
		emLen--;
		}
	if (emLen < (hLen + sLen + 2)) /* sLen can be small negative */
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_DATA_TOO_LARGE);
		goto err;
		}
	if (EM[emLen - 1] != 0xbc)
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_LAST_OCTET_INVALID);
		goto err;
		}
	maskedDBLen = emLen - hLen - 1;
	H = EM + maskedDBLen;
	DB = OPENSSL_malloc(maskedDBLen);
	if (!DB)
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, ERR_R_MALLOC_FAILURE);
		goto err;
		}
	PKCS1_MGF1(DB, maskedDBLen, H, hLen, Hash);
	for (i = 0; i < maskedDBLen; i++)
		DB[i] ^= EM[i];
	if (MSBits)
		DB[0] &= 0xFF >> (8 - MSBits);
	for (i = 0; DB[i] == 0 && i < (maskedDBLen-1); i++) ;
	if (DB[i++] != 0x1)
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_SLEN_RECOVERY_FAILED);
		goto err;
		}
	if (sLen >= 0 && (maskedDBLen - i) != sLen)
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_SLEN_CHECK_FAILED);
		goto err;
		}
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, Hash, NULL);
	EVP_DigestUpdate(&ctx, zeroes, sizeof zeroes);
	EVP_DigestUpdate(&ctx, mHash, hLen);
	if (maskedDBLen - i)
		EVP_DigestUpdate(&ctx, DB + i, maskedDBLen - i);
	EVP_DigestFinal(&ctx, H_, NULL);
	EVP_MD_CTX_cleanup(&ctx);
	if (memcmp(H_, H, hLen))
		{
		RSAerr(RSA_F_RSA_VERIFY_PKCS1_PSS, RSA_R_BAD_SIGNATURE);
		ret = 0;
		}
	else 
		ret = 1;

	err:
	if (DB)
		OPENSSL_free(DB);

	return ret;

	}

int RSA_padding_add_PKCS1_PSS(RSA *rsa, unsigned char *EM,
			const unsigned char *mHash,
			const EVP_MD *Hash, int sLen)
	{
	int i;
	int ret = 0;
	int hLen, maskedDBLen, MSBits, emLen;
	unsigned char *H, *salt = NULL, *p;
	EVP_MD_CTX ctx;

	hLen = M_EVP_MD_size(Hash);
	/*
	 * Negative sLen has special meanings:
	 *	-1	sLen == hLen
	 *	-2	salt length is maximized
	 *	-N	reserved
	 */
	if      (sLen == -1)	sLen = hLen;
	else if (sLen == -2)	sLen = -2;
	else if (sLen < -2)
		{
		RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_PSS, RSA_R_SLEN_CHECK_FAILED);
		goto err;
		}

	MSBits = (BN_num_bits(rsa->n) - 1) & 0x7;
	emLen = RSA_size(rsa);
	if (MSBits == 0)
		{
		*EM++ = 0;
		emLen--;
		}
	if (sLen == -2)
		{
		sLen = emLen - hLen - 2;
		}
	else if (emLen < (hLen + sLen + 2))
		{
		RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_PSS,
		   RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
		goto err;
		}
	if (sLen > 0)
		{
		salt = OPENSSL_malloc(sLen);
		if (!salt)
			{
			RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_PSS,
		   		ERR_R_MALLOC_FAILURE);
			goto err;
			}
		if (RAND_bytes(salt, sLen) <= 0)
			goto err;
		}
	maskedDBLen = emLen - hLen - 1;
	H = EM + maskedDBLen;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, Hash, NULL);
	EVP_DigestUpdate(&ctx, zeroes, sizeof zeroes);
	EVP_DigestUpdate(&ctx, mHash, hLen);
	if (sLen)
		EVP_DigestUpdate(&ctx, salt, sLen);
	EVP_DigestFinal(&ctx, H, NULL);
	EVP_MD_CTX_cleanup(&ctx);

	/* Generate dbMask in place then perform XOR on it */
	PKCS1_MGF1(EM, maskedDBLen, H, hLen, Hash);

	p = EM;

	/* Initial PS XORs with all zeroes which is a NOP so just update
	 * pointer. Note from a test above this value is guaranteed to
	 * be non-negative.
	 */
	p += emLen - sLen - hLen - 2;
	*p++ ^= 0x1;
	if (sLen > 0)
		{
		for (i = 0; i < sLen; i++)
			*p++ ^= salt[i];
		}
	if (MSBits)
		EM[0] &= 0xFF >> (8 - MSBits);

	/* H is already in place so just set final 0xbc */

	EM[emLen - 1] = 0xbc;

	ret = 1;

	err:
	if (salt)
		OPENSSL_free(salt);

	return ret;

	}

#if defined(_MSC_VER)
#pragma optimize("",on)
#endif
