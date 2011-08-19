/* crypto/des/ofb64ede.c */
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

#include "des_locl.h"

/* The input and output encrypted as though 64bit ofb mode is being
 * used.  The extra state information to record how much of the
 * 64bit block we have used is contained in *num;
 */
void DES_ede3_ofb64_encrypt(register const unsigned char *in,
			    register unsigned char *out, long length,
			    DES_key_schedule *k1, DES_key_schedule *k2,
			    DES_key_schedule *k3, DES_cblock *ivec,
			    int *num)
	{
	register DES_LONG v0,v1;
	register int n= *num;
	register long l=length;
	DES_cblock d;
	register char *dp;
	DES_LONG ti[2];
	unsigned char *iv;
	int save=0;

	iv = &(*ivec)[0];
	c2l(iv,v0);
	c2l(iv,v1);
	ti[0]=v0;
	ti[1]=v1;
	dp=(char *)d;
	l2c(v0,dp);
	l2c(v1,dp);
	while (l--)
		{
		if (n == 0)
			{
			/* ti[0]=v0; */
			/* ti[1]=v1; */
			DES_encrypt3(ti,k1,k2,k3);
			v0=ti[0];
			v1=ti[1];

			dp=(char *)d;
			l2c(v0,dp);
			l2c(v1,dp);
			save++;
			}
		*(out++)= *(in++)^d[n];
		n=(n+1)&0x07;
		}
	if (save)
		{
/*		v0=ti[0];
		v1=ti[1];*/
		iv = &(*ivec)[0];
		l2c(v0,iv);
		l2c(v1,iv);
		}
	v0=v1=ti[0]=ti[1]=0;
	*num=n;
	}

#ifdef undef /* MACRO */
void DES_ede2_ofb64_encrypt(register unsigned char *in,
	     register unsigned char *out, long length, DES_key_schedule k1,
	     DES_key_schedule k2, DES_cblock (*ivec), int *num)
	{
	DES_ede3_ofb64_encrypt(in, out, length, k1,k2,k1, ivec, num);
	}
#endif
