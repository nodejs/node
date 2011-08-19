/* crypto/des/ofb_enc.c */
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

/* The input and output are loaded in multiples of 8 bits.
 * What this means is that if you hame numbits=12 and length=2
 * the first 12 bits will be retrieved from the first byte and half
 * the second.  The second 12 bits will come from the 3rd and half the 4th
 * byte.
 */
void DES_ofb_encrypt(const unsigned char *in, unsigned char *out, int numbits,
		     long length, DES_key_schedule *schedule,
		     DES_cblock *ivec)
	{
	register DES_LONG d0,d1,vv0,vv1,v0,v1,n=(numbits+7)/8;
	register DES_LONG mask0,mask1;
	register long l=length;
	register int num=numbits;
	DES_LONG ti[2];
	unsigned char *iv;

	if (num > 64) return;
	if (num > 32)
		{
		mask0=0xffffffffL;
		if (num >= 64)
			mask1=mask0;
		else
			mask1=(1L<<(num-32))-1;
		}
	else
		{
		if (num == 32)
			mask0=0xffffffffL;
		else
			mask0=(1L<<num)-1;
		mask1=0x00000000L;
		}

	iv = &(*ivec)[0];
	c2l(iv,v0);
	c2l(iv,v1);
	ti[0]=v0;
	ti[1]=v1;
	while (l-- > 0)
		{
		ti[0]=v0;
		ti[1]=v1;
		DES_encrypt1((DES_LONG *)ti,schedule,DES_ENCRYPT);
		vv0=ti[0];
		vv1=ti[1];
		c2ln(in,d0,d1,n);
		in+=n;
		d0=(d0^vv0)&mask0;
		d1=(d1^vv1)&mask1;
		l2cn(d0,d1,out,n);
		out+=n;

		if (num == 32)
			{ v0=v1; v1=vv0; }
		else if (num == 64)
				{ v0=vv0; v1=vv1; }
		else if (num > 32) /* && num != 64 */
			{
			v0=((v1>>(num-32))|(vv0<<(64-num)))&0xffffffffL;
			v1=((vv0>>(num-32))|(vv1<<(64-num)))&0xffffffffL;
			}
		else /* num < 32 */
			{
			v0=((v0>>num)|(v1<<(32-num)))&0xffffffffL;
			v1=((v1>>num)|(vv0<<(32-num)))&0xffffffffL;
			}
		}
	iv = &(*ivec)[0];
	l2c(v0,iv);
	l2c(v1,iv);
	v0=v1=d0=d1=ti[0]=ti[1]=vv0=vv1=0;
	}

