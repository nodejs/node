/* crypto/camellia/camellia_cbc.c -*- mode:C; c-file-style: "eay" -*- */
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

#ifndef CAMELLIA_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openssl/camellia.h>
#include "cmll_locl.h"

void Camellia_cbc_encrypt(const unsigned char *in, unsigned char *out,
		     const unsigned long length, const CAMELLIA_KEY *key,
		     unsigned char *ivec, const int enc) {

	unsigned long n;
	unsigned long len = length;
	const unsigned char *iv = ivec;
	union {	u32 t32[CAMELLIA_BLOCK_SIZE/sizeof(u32)];
		u8  t8 [CAMELLIA_BLOCK_SIZE]; } tmp;
	const union { long one; char little; } camellia_endian = {1};


	assert(in && out && key && ivec);
	assert((CAMELLIA_ENCRYPT == enc)||(CAMELLIA_DECRYPT == enc));

	if(((size_t)in|(size_t)out|(size_t)ivec) % sizeof(u32) == 0)
		{
		if (CAMELLIA_ENCRYPT == enc)
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				XOR4WORD2((u32 *)out,
					(u32 *)in, (u32 *)iv);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				key->enc(key->rd_key, (u32 *)out);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				iv = out;
				len -= CAMELLIA_BLOCK_SIZE;
				in += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				for(n=0; n < len; ++n)
					out[n] = in[n] ^ iv[n];
				for(n=len; n < CAMELLIA_BLOCK_SIZE; ++n)
					out[n] = iv[n];
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				key->enc(key->rd_key, (u32 *)out);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				iv = out;
				}
			memcpy(ivec,iv,CAMELLIA_BLOCK_SIZE);
			}
		else if (in != out)
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				memcpy(out,in,CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				key->dec(key->rd_key,(u32 *)out);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				XOR4WORD((u32 *)out, (u32 *)iv);
				iv = in;
				len -= CAMELLIA_BLOCK_SIZE;
				in  += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->dec(key->rd_key, tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				for(n=0; n < len; ++n)
					out[n] = tmp.t8[n] ^ iv[n];
				iv = in;
				}
			memcpy(ivec,iv,CAMELLIA_BLOCK_SIZE);
			}
		else /* in == out */
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				key->dec(key->rd_key, (u32 *)out);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				XOR4WORD((u32 *)out, (u32 *)ivec);
				memcpy(ivec, tmp.t8, CAMELLIA_BLOCK_SIZE);
				len -= CAMELLIA_BLOCK_SIZE;
				in += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				key->dec(key->rd_key,(u32 *)out);
				if (camellia_endian.little)
					SWAP4WORD((u32 *)out);
				for(n=0; n < len; ++n)
					out[n] ^= ivec[n];
				for(n=len; n < CAMELLIA_BLOCK_SIZE; ++n)
					out[n] = tmp.t8[n];
				memcpy(ivec, tmp.t8, CAMELLIA_BLOCK_SIZE);
				}
			}
		}
	else /* no aligned */
		{
		if (CAMELLIA_ENCRYPT == enc)
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				for(n=0; n < CAMELLIA_BLOCK_SIZE; ++n)
					tmp.t8[n] = in[n] ^ iv[n];
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->enc(key->rd_key, tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				memcpy(out, tmp.t8, CAMELLIA_BLOCK_SIZE);
				iv = out;
				len -= CAMELLIA_BLOCK_SIZE;
				in += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				for(n=0; n < len; ++n)
					tmp.t8[n] = in[n] ^ iv[n];
				for(n=len; n < CAMELLIA_BLOCK_SIZE; ++n)
					tmp.t8[n] = iv[n];
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->enc(key->rd_key, tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				memcpy(out, tmp.t8, CAMELLIA_BLOCK_SIZE);
				iv = out;
				}
			memcpy(ivec,iv,CAMELLIA_BLOCK_SIZE);
			}
		else if (in != out)
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				memcpy(tmp.t8,in,CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->dec(key->rd_key,tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				for(n=0; n < CAMELLIA_BLOCK_SIZE; ++n)
					out[n] = tmp.t8[n] ^ iv[n];
				iv = in;
				len -= CAMELLIA_BLOCK_SIZE;
				in  += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->dec(key->rd_key, tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				for(n=0; n < len; ++n)
					out[n] = tmp.t8[n] ^ iv[n];
				iv = in;
				}
			memcpy(ivec,iv,CAMELLIA_BLOCK_SIZE);
			}
		else
			{
			while (len >= CAMELLIA_BLOCK_SIZE)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->dec(key->rd_key, tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				for(n=0; n < CAMELLIA_BLOCK_SIZE; ++n)
					tmp.t8[n] ^= ivec[n];
				memcpy(ivec, in, CAMELLIA_BLOCK_SIZE);
				memcpy(out, tmp.t8, CAMELLIA_BLOCK_SIZE);
				len -= CAMELLIA_BLOCK_SIZE;
				in += CAMELLIA_BLOCK_SIZE;
				out += CAMELLIA_BLOCK_SIZE;
				}
			if (len)
				{
				memcpy(tmp.t8, in, CAMELLIA_BLOCK_SIZE);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				key->dec(key->rd_key,tmp.t32);
				if (camellia_endian.little)
					SWAP4WORD(tmp.t32);
				for(n=0; n < len; ++n)
					tmp.t8[n] ^= ivec[n];
				memcpy(ivec, in, CAMELLIA_BLOCK_SIZE);
				memcpy(out,tmp.t8,len);
				}
			}
		}
}
