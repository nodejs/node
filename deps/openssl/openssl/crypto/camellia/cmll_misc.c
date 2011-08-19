/* crypto/camellia/camellia_misc.c -*- mode:C; c-file-style: "eay" -*- */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
 */
 
#include <openssl/opensslv.h>
#include <openssl/camellia.h>
#include "cmll_locl.h"
#include <openssl/crypto.h>
#ifdef OPENSSL_FIPS
#include <openssl/fips.h>
#endif

const char CAMELLIA_version[]="CAMELLIA" OPENSSL_VERSION_PTEXT;

int Camellia_set_key(const unsigned char *userKey, const int bits,
	CAMELLIA_KEY *key)
#ifdef OPENSSL_FIPS
	{
	if (FIPS_mode())
		FIPS_BAD_ABORT(CAMELLIA)
	return private_Camellia_set_key(userKey, bits, key);
	}
int private_Camellia_set_key(const unsigned char *userKey, const int bits,
	CAMELLIA_KEY *key)
#endif
	{
	if (!userKey || !key)
		{
		return -1;
		}
	
	switch(bits)
		{
	case 128:
		camellia_setup128(userKey, (unsigned int *)key->rd_key);
		key->enc = camellia_encrypt128;
		key->dec = camellia_decrypt128;
		break;
	case 192:
		camellia_setup192(userKey, (unsigned int *)key->rd_key);
		key->enc = camellia_encrypt256;
		key->dec = camellia_decrypt256;
		break;
	case 256:
		camellia_setup256(userKey, (unsigned int *)key->rd_key);
		key->enc = camellia_encrypt256;
		key->dec = camellia_decrypt256;
		break;
	default:
		return -2;
		}
	
	key->bitLength = bits;
	return 0;
	}

void Camellia_encrypt(const unsigned char *in, unsigned char *out,
	const CAMELLIA_KEY *key)
	{
	u32 tmp[CAMELLIA_BLOCK_SIZE/sizeof(u32)];
	const union { long one; char little; } camellia_endian = {1};

	memcpy(tmp, in, CAMELLIA_BLOCK_SIZE);
	if (camellia_endian.little) SWAP4WORD(tmp);
	key->enc(key->rd_key, tmp);
	if (camellia_endian.little) SWAP4WORD(tmp);
	memcpy(out, tmp, CAMELLIA_BLOCK_SIZE);
	}

void Camellia_decrypt(const unsigned char *in, unsigned char *out,
	const CAMELLIA_KEY *key)
	{
	u32 tmp[CAMELLIA_BLOCK_SIZE/sizeof(u32)];
	const union { long one; char little; } camellia_endian = {1};

	memcpy(tmp, in, CAMELLIA_BLOCK_SIZE);
	if (camellia_endian.little) SWAP4WORD(tmp);
	key->dec(key->rd_key, tmp);
	if (camellia_endian.little) SWAP4WORD(tmp);
	memcpy(out, tmp, CAMELLIA_BLOCK_SIZE);
	}

