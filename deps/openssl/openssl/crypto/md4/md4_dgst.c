/* crypto/md4/md4_dgst.c */
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
#include "md4_locl.h"
#include <openssl/opensslv.h>

const char MD4_version[]="MD4" OPENSSL_VERSION_PTEXT;

/* Implemented from RFC1186 The MD4 Message-Digest Algorithm
 */

#define INIT_DATA_A (unsigned long)0x67452301L
#define INIT_DATA_B (unsigned long)0xefcdab89L
#define INIT_DATA_C (unsigned long)0x98badcfeL
#define INIT_DATA_D (unsigned long)0x10325476L

int MD4_Init(MD4_CTX *c)
	{
	memset (c,0,sizeof(*c));
	c->A=INIT_DATA_A;
	c->B=INIT_DATA_B;
	c->C=INIT_DATA_C;
	c->D=INIT_DATA_D;
	return 1;
	}

#ifndef md4_block_data_order
#ifdef X
#undef X
#endif
void md4_block_data_order (MD4_CTX *c, const void *data_, size_t num)
	{
	const unsigned char *data=data_;
	register unsigned MD32_REG_T A,B,C,D,l;
#ifndef MD32_XARRAY
	/* See comment in crypto/sha/sha_locl.h for details. */
	unsigned MD32_REG_T	XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
				XX8, XX9,XX10,XX11,XX12,XX13,XX14,XX15;
# define X(i)	XX##i
#else
	MD4_LONG XX[MD4_LBLOCK];
# define X(i)	XX[i]
#endif

	A=c->A;
	B=c->B;
	C=c->C;
	D=c->D;

	for (;num--;)
		{
	HOST_c2l(data,l); X( 0)=l;		HOST_c2l(data,l); X( 1)=l;
	/* Round 0 */
	R0(A,B,C,D,X( 0), 3,0);	HOST_c2l(data,l); X( 2)=l;
	R0(D,A,B,C,X( 1), 7,0);	HOST_c2l(data,l); X( 3)=l;
	R0(C,D,A,B,X( 2),11,0);	HOST_c2l(data,l); X( 4)=l;
	R0(B,C,D,A,X( 3),19,0);	HOST_c2l(data,l); X( 5)=l;
	R0(A,B,C,D,X( 4), 3,0);	HOST_c2l(data,l); X( 6)=l;
	R0(D,A,B,C,X( 5), 7,0);	HOST_c2l(data,l); X( 7)=l;
	R0(C,D,A,B,X( 6),11,0);	HOST_c2l(data,l); X( 8)=l;
	R0(B,C,D,A,X( 7),19,0);	HOST_c2l(data,l); X( 9)=l;
	R0(A,B,C,D,X( 8), 3,0);	HOST_c2l(data,l); X(10)=l;
	R0(D,A,B,C,X( 9), 7,0);	HOST_c2l(data,l); X(11)=l;
	R0(C,D,A,B,X(10),11,0);	HOST_c2l(data,l); X(12)=l;
	R0(B,C,D,A,X(11),19,0);	HOST_c2l(data,l); X(13)=l;
	R0(A,B,C,D,X(12), 3,0);	HOST_c2l(data,l); X(14)=l;
	R0(D,A,B,C,X(13), 7,0);	HOST_c2l(data,l); X(15)=l;
	R0(C,D,A,B,X(14),11,0);
	R0(B,C,D,A,X(15),19,0);
	/* Round 1 */
	R1(A,B,C,D,X( 0), 3,0x5A827999L);
	R1(D,A,B,C,X( 4), 5,0x5A827999L);
	R1(C,D,A,B,X( 8), 9,0x5A827999L);
	R1(B,C,D,A,X(12),13,0x5A827999L);
	R1(A,B,C,D,X( 1), 3,0x5A827999L);
	R1(D,A,B,C,X( 5), 5,0x5A827999L);
	R1(C,D,A,B,X( 9), 9,0x5A827999L);
	R1(B,C,D,A,X(13),13,0x5A827999L);
	R1(A,B,C,D,X( 2), 3,0x5A827999L);
	R1(D,A,B,C,X( 6), 5,0x5A827999L);
	R1(C,D,A,B,X(10), 9,0x5A827999L);
	R1(B,C,D,A,X(14),13,0x5A827999L);
	R1(A,B,C,D,X( 3), 3,0x5A827999L);
	R1(D,A,B,C,X( 7), 5,0x5A827999L);
	R1(C,D,A,B,X(11), 9,0x5A827999L);
	R1(B,C,D,A,X(15),13,0x5A827999L);
	/* Round 2 */
	R2(A,B,C,D,X( 0), 3,0x6ED9EBA1L);
	R2(D,A,B,C,X( 8), 9,0x6ED9EBA1L);
	R2(C,D,A,B,X( 4),11,0x6ED9EBA1L);
	R2(B,C,D,A,X(12),15,0x6ED9EBA1L);
	R2(A,B,C,D,X( 2), 3,0x6ED9EBA1L);
	R2(D,A,B,C,X(10), 9,0x6ED9EBA1L);
	R2(C,D,A,B,X( 6),11,0x6ED9EBA1L);
	R2(B,C,D,A,X(14),15,0x6ED9EBA1L);
	R2(A,B,C,D,X( 1), 3,0x6ED9EBA1L);
	R2(D,A,B,C,X( 9), 9,0x6ED9EBA1L);
	R2(C,D,A,B,X( 5),11,0x6ED9EBA1L);
	R2(B,C,D,A,X(13),15,0x6ED9EBA1L);
	R2(A,B,C,D,X( 3), 3,0x6ED9EBA1L);
	R2(D,A,B,C,X(11), 9,0x6ED9EBA1L);
	R2(C,D,A,B,X( 7),11,0x6ED9EBA1L);
	R2(B,C,D,A,X(15),15,0x6ED9EBA1L);

	A = c->A += A;
	B = c->B += B;
	C = c->C += C;
	D = c->D += D;
		}
	}
#endif
