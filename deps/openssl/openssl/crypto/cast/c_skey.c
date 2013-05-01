/* crypto/cast/c_skey.c */
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

#include <openssl/crypto.h>
#include <openssl/cast.h>
#include "cast_lcl.h"
#include "cast_s.h"

#define CAST_exp(l,A,a,n) \
	A[n/4]=l; \
	a[n+3]=(l    )&0xff; \
	a[n+2]=(l>> 8)&0xff; \
	a[n+1]=(l>>16)&0xff; \
	a[n+0]=(l>>24)&0xff;

#define S4 CAST_S_table4
#define S5 CAST_S_table5
#define S6 CAST_S_table6
#define S7 CAST_S_table7
void CAST_set_key(CAST_KEY *key, int len, const unsigned char *data)
#ifdef OPENSSL_FIPS
	{
	fips_cipher_abort(CAST);
	private_CAST_set_key(key, len, data);
	}
void private_CAST_set_key(CAST_KEY *key, int len, const unsigned char *data)
#endif
	{
	CAST_LONG x[16];
	CAST_LONG z[16];
	CAST_LONG k[32];
	CAST_LONG X[4],Z[4];
	CAST_LONG l,*K;
	int i;

	for (i=0; i<16; i++) x[i]=0;
	if (len > 16) len=16;
	for (i=0; i<len; i++)
		x[i]=data[i];
	if(len <= 10)
	    key->short_key=1;
	else
	    key->short_key=0;

	K= &k[0];
	X[0]=((x[ 0]<<24)|(x[ 1]<<16)|(x[ 2]<<8)|x[ 3])&0xffffffffL;
	X[1]=((x[ 4]<<24)|(x[ 5]<<16)|(x[ 6]<<8)|x[ 7])&0xffffffffL;
	X[2]=((x[ 8]<<24)|(x[ 9]<<16)|(x[10]<<8)|x[11])&0xffffffffL;
	X[3]=((x[12]<<24)|(x[13]<<16)|(x[14]<<8)|x[15])&0xffffffffL;

	for (;;)
		{
	l=X[0]^S4[x[13]]^S5[x[15]]^S6[x[12]]^S7[x[14]]^S6[x[ 8]];
	CAST_exp(l,Z,z, 0);
	l=X[2]^S4[z[ 0]]^S5[z[ 2]]^S6[z[ 1]]^S7[z[ 3]]^S7[x[10]];
	CAST_exp(l,Z,z, 4);
	l=X[3]^S4[z[ 7]]^S5[z[ 6]]^S6[z[ 5]]^S7[z[ 4]]^S4[x[ 9]];
	CAST_exp(l,Z,z, 8);
	l=X[1]^S4[z[10]]^S5[z[ 9]]^S6[z[11]]^S7[z[ 8]]^S5[x[11]];
	CAST_exp(l,Z,z,12);

	K[ 0]= S4[z[ 8]]^S5[z[ 9]]^S6[z[ 7]]^S7[z[ 6]]^S4[z[ 2]];
	K[ 1]= S4[z[10]]^S5[z[11]]^S6[z[ 5]]^S7[z[ 4]]^S5[z[ 6]];
	K[ 2]= S4[z[12]]^S5[z[13]]^S6[z[ 3]]^S7[z[ 2]]^S6[z[ 9]];
	K[ 3]= S4[z[14]]^S5[z[15]]^S6[z[ 1]]^S7[z[ 0]]^S7[z[12]];

	l=Z[2]^S4[z[ 5]]^S5[z[ 7]]^S6[z[ 4]]^S7[z[ 6]]^S6[z[ 0]];
	CAST_exp(l,X,x, 0);
	l=Z[0]^S4[x[ 0]]^S5[x[ 2]]^S6[x[ 1]]^S7[x[ 3]]^S7[z[ 2]];
	CAST_exp(l,X,x, 4);
	l=Z[1]^S4[x[ 7]]^S5[x[ 6]]^S6[x[ 5]]^S7[x[ 4]]^S4[z[ 1]];
	CAST_exp(l,X,x, 8);
	l=Z[3]^S4[x[10]]^S5[x[ 9]]^S6[x[11]]^S7[x[ 8]]^S5[z[ 3]];
	CAST_exp(l,X,x,12);

	K[ 4]= S4[x[ 3]]^S5[x[ 2]]^S6[x[12]]^S7[x[13]]^S4[x[ 8]];
	K[ 5]= S4[x[ 1]]^S5[x[ 0]]^S6[x[14]]^S7[x[15]]^S5[x[13]];
	K[ 6]= S4[x[ 7]]^S5[x[ 6]]^S6[x[ 8]]^S7[x[ 9]]^S6[x[ 3]];
	K[ 7]= S4[x[ 5]]^S5[x[ 4]]^S6[x[10]]^S7[x[11]]^S7[x[ 7]];

	l=X[0]^S4[x[13]]^S5[x[15]]^S6[x[12]]^S7[x[14]]^S6[x[ 8]];
	CAST_exp(l,Z,z, 0);
	l=X[2]^S4[z[ 0]]^S5[z[ 2]]^S6[z[ 1]]^S7[z[ 3]]^S7[x[10]];
	CAST_exp(l,Z,z, 4);
	l=X[3]^S4[z[ 7]]^S5[z[ 6]]^S6[z[ 5]]^S7[z[ 4]]^S4[x[ 9]];
	CAST_exp(l,Z,z, 8);
	l=X[1]^S4[z[10]]^S5[z[ 9]]^S6[z[11]]^S7[z[ 8]]^S5[x[11]];
	CAST_exp(l,Z,z,12);

	K[ 8]= S4[z[ 3]]^S5[z[ 2]]^S6[z[12]]^S7[z[13]]^S4[z[ 9]];
	K[ 9]= S4[z[ 1]]^S5[z[ 0]]^S6[z[14]]^S7[z[15]]^S5[z[12]];
	K[10]= S4[z[ 7]]^S5[z[ 6]]^S6[z[ 8]]^S7[z[ 9]]^S6[z[ 2]];
	K[11]= S4[z[ 5]]^S5[z[ 4]]^S6[z[10]]^S7[z[11]]^S7[z[ 6]];

	l=Z[2]^S4[z[ 5]]^S5[z[ 7]]^S6[z[ 4]]^S7[z[ 6]]^S6[z[ 0]];
	CAST_exp(l,X,x, 0);
	l=Z[0]^S4[x[ 0]]^S5[x[ 2]]^S6[x[ 1]]^S7[x[ 3]]^S7[z[ 2]];
	CAST_exp(l,X,x, 4);
	l=Z[1]^S4[x[ 7]]^S5[x[ 6]]^S6[x[ 5]]^S7[x[ 4]]^S4[z[ 1]];
	CAST_exp(l,X,x, 8);
	l=Z[3]^S4[x[10]]^S5[x[ 9]]^S6[x[11]]^S7[x[ 8]]^S5[z[ 3]];
	CAST_exp(l,X,x,12);

	K[12]= S4[x[ 8]]^S5[x[ 9]]^S6[x[ 7]]^S7[x[ 6]]^S4[x[ 3]];
	K[13]= S4[x[10]]^S5[x[11]]^S6[x[ 5]]^S7[x[ 4]]^S5[x[ 7]];
	K[14]= S4[x[12]]^S5[x[13]]^S6[x[ 3]]^S7[x[ 2]]^S6[x[ 8]];
	K[15]= S4[x[14]]^S5[x[15]]^S6[x[ 1]]^S7[x[ 0]]^S7[x[13]];
	if (K != k)  break;
	K+=16;
		}

	for (i=0; i<16; i++)
		{
		key->data[i*2]=k[i];
		key->data[i*2+1]=((k[i+16])+16)&0x1f;
		}
	}

