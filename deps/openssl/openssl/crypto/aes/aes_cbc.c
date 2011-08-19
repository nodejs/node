/* crypto/aes/aes_cbc.c -*- mode:C; c-file-style: "eay" -*- */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
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

#ifndef AES_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>

#include <openssl/aes.h>
#include "aes_locl.h"

#if !defined(OPENSSL_FIPS_AES_ASM)
void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
		     const unsigned long length, const AES_KEY *key,
		     unsigned char *ivec, const int enc) {

	unsigned long n;
	unsigned long len = length;
	unsigned char tmp[AES_BLOCK_SIZE];
	const unsigned char *iv = ivec;

	assert(in && out && key && ivec);
	assert((AES_ENCRYPT == enc)||(AES_DECRYPT == enc));

	if (AES_ENCRYPT == enc) {
		while (len >= AES_BLOCK_SIZE) {
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] = in[n] ^ iv[n];
			AES_encrypt(out, out, key);
			iv = out;
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			for(n=0; n < len; ++n)
				out[n] = in[n] ^ iv[n];
			for(n=len; n < AES_BLOCK_SIZE; ++n)
				out[n] = iv[n];
			AES_encrypt(out, out, key);
			iv = out;
		}
		memcpy(ivec,iv,AES_BLOCK_SIZE);
	} else if (in != out) {
		while (len >= AES_BLOCK_SIZE) {
			AES_decrypt(in, out, key);
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= iv[n];
			iv = in;
			len -= AES_BLOCK_SIZE;
			in  += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			AES_decrypt(in,tmp,key);
			for(n=0; n < len; ++n)
				out[n] = tmp[n] ^ iv[n];
			iv = in;
		}
		memcpy(ivec,iv,AES_BLOCK_SIZE);
	} else {
		while (len >= AES_BLOCK_SIZE) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(in, out, key);
			for(n=0; n < AES_BLOCK_SIZE; ++n)
				out[n] ^= ivec[n];
			memcpy(ivec, tmp, AES_BLOCK_SIZE);
			len -= AES_BLOCK_SIZE;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
		if (len) {
			memcpy(tmp, in, AES_BLOCK_SIZE);
			AES_decrypt(tmp, out, key);
			for(n=0; n < len; ++n)
				out[n] ^= ivec[n];
			for(n=len; n < AES_BLOCK_SIZE; ++n)
				out[n] = tmp[n];
			memcpy(ivec, tmp, AES_BLOCK_SIZE);
		}
	}
}
#endif
