/* crypto/camellia/camellia_cfb.c -*- mode:C; c-file-style: "eay" -*- */
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

#ifndef CAMELLIA_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif
#include <assert.h>
#include <string.h>

#include <openssl/camellia.h>
#include "cmll_locl.h"
#include "e_os.h"


/* The input and output encrypted as though 128bit cfb mode is being
 * used.  The extra state information to record how much of the
 * 128bit block we have used is contained in *num;
 */

void Camellia_cfb128_encrypt(const unsigned char *in, unsigned char *out,
	const unsigned long length, const CAMELLIA_KEY *key,
	unsigned char *ivec, int *num, const int enc)
	{

	unsigned int n;
	unsigned long l = length;
	unsigned char c;

	assert(in && out && key && ivec && num);

	n = *num;

	if (enc) 
		{
		while (l--) 
			{
			if (n == 0) 
				{
				Camellia_encrypt(ivec, ivec, key);
				}
			ivec[n] = *(out++) = *(in++) ^ ivec[n];
			n = (n+1) % CAMELLIA_BLOCK_SIZE;
			}
		} 
	else 
		{
		while (l--) 
			{
			if (n == 0) 
				{
				Camellia_encrypt(ivec, ivec, key);
				}
			c = *(in);
			*(out++) = *(in++) ^ ivec[n];
			ivec[n] = c;
			n = (n+1) % CAMELLIA_BLOCK_SIZE;
			}
		}

	*num=n;
	}

/* This expects a single block of size nbits for both in and out. Note that
   it corrupts any extra bits in the last byte of out */
void Camellia_cfbr_encrypt_block(const unsigned char *in,unsigned char *out,
	const int nbits,const CAMELLIA_KEY *key,
	unsigned char *ivec,const int enc)
	{
	int n,rem,num;
	unsigned char ovec[CAMELLIA_BLOCK_SIZE*2];

	if (nbits<=0 || nbits>128) return;

	/* fill in the first half of the new IV with the current IV */
	memcpy(ovec,ivec,CAMELLIA_BLOCK_SIZE);
	/* construct the new IV */
	Camellia_encrypt(ivec,ivec,key);
	num = (nbits+7)/8;
	if (enc)	/* encrypt the input */
		for(n=0 ; n < num ; ++n)
			out[n] = (ovec[CAMELLIA_BLOCK_SIZE+n] = in[n] ^ ivec[n]);
	else		/* decrypt the input */
		for(n=0 ; n < num ; ++n)
			out[n] = (ovec[CAMELLIA_BLOCK_SIZE+n] = in[n]) ^ ivec[n];
	/* shift ovec left... */
	rem = nbits%8;
	num = nbits/8;
	if(rem==0)
		memcpy(ivec,ovec+num,CAMELLIA_BLOCK_SIZE);
	else
		for(n=0 ; n < CAMELLIA_BLOCK_SIZE ; ++n)
			ivec[n] = ovec[n+num]<<rem | ovec[n+num+1]>>(8-rem);

	/* it is not necessary to cleanse ovec, since the IV is not secret */
	}

/* N.B. This expects the input to be packed, MS bit first */
void Camellia_cfb1_encrypt(const unsigned char *in, unsigned char *out,
	const unsigned long length, const CAMELLIA_KEY *key,
	unsigned char *ivec, int *num, const int enc)
	{
	unsigned int n;
	unsigned char c[1],d[1];

	assert(in && out && key && ivec && num);
	assert(*num == 0);

	memset(out,0,(length+7)/8);
	for(n=0 ; n < length ; ++n)
		{
		c[0]=(in[n/8]&(1 << (7-n%8))) ? 0x80 : 0;
		Camellia_cfbr_encrypt_block(c,d,1,key,ivec,enc);
		out[n/8]=(out[n/8]&~(1 << (7-n%8)))|((d[0]&0x80) >> (n%8));
		}
	}

void Camellia_cfb8_encrypt(const unsigned char *in, unsigned char *out,
	const unsigned long length, const CAMELLIA_KEY *key,
	unsigned char *ivec, int *num, const int enc)
	{
	unsigned int n;

	assert(in && out && key && ivec && num);
	assert(*num == 0);

	for(n=0 ; n < length ; ++n)
		Camellia_cfbr_encrypt_block(&in[n],&out[n],8,key,ivec,enc);
	}

