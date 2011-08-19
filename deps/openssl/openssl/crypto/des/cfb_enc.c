/* crypto/des/cfb_enc.c */
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

#include "e_os.h"
#include "des_locl.h"
#include <assert.h>

/* The input and output are loaded in multiples of 8 bits.
 * What this means is that if you hame numbits=12 and length=2
 * the first 12 bits will be retrieved from the first byte and half
 * the second.  The second 12 bits will come from the 3rd and half the 4th
 * byte.
 */
/* Until Aug 1 2003 this function did not correctly implement CFB-r, so it
 * will not be compatible with any encryption prior to that date. Ben. */
void DES_cfb_encrypt(const unsigned char *in, unsigned char *out, int numbits,
		     long length, DES_key_schedule *schedule, DES_cblock *ivec,
		     int enc)
	{
	register DES_LONG d0,d1,v0,v1;
	register unsigned long l=length;
	register int num=numbits/8,n=(numbits+7)/8,i,rem=numbits%8;
	DES_LONG ti[2];
	unsigned char *iv;
#ifndef L_ENDIAN
	unsigned char ovec[16];
#else
	unsigned int  sh[4];
	unsigned char *ovec=(unsigned char *)sh;

	/* I kind of count that compiler optimizes away this assertioni,*/
	assert (sizeof(sh[0])==4);	/* as this holds true for all,	*/
					/* but 16-bit platforms...	*/
					
#endif

	if (numbits<=0 || numbits > 64) return;
	iv = &(*ivec)[0];
	c2l(iv,v0);
	c2l(iv,v1);
	if (enc)
		{
		while (l >= (unsigned long)n)
			{
			l-=n;
			ti[0]=v0;
			ti[1]=v1;
			DES_encrypt1((DES_LONG *)ti,schedule,DES_ENCRYPT);
			c2ln(in,d0,d1,n);
			in+=n;
			d0^=ti[0];
			d1^=ti[1];
			l2cn(d0,d1,out,n);
			out+=n;
			/* 30-08-94 - eay - changed because l>>32 and
			 * l<<32 are bad under gcc :-( */
			if (numbits == 32)
				{ v0=v1; v1=d0; }
			else if (numbits == 64)
				{ v0=d0; v1=d1; }
			else
				{
#ifndef L_ENDIAN
				iv=&ovec[0];
				l2c(v0,iv);
				l2c(v1,iv);
				l2c(d0,iv);
				l2c(d1,iv);
#else
				sh[0]=v0, sh[1]=v1, sh[2]=d0, sh[3]=d1;
#endif
				if (rem==0)
					memmove(ovec,ovec+num,8);
				else
					for(i=0 ; i < 8 ; ++i)
						ovec[i]=ovec[i+num]<<rem |
							ovec[i+num+1]>>(8-rem);
#ifdef L_ENDIAN
				v0=sh[0], v1=sh[1];
#else
				iv=&ovec[0];
				c2l(iv,v0);
				c2l(iv,v1);
#endif
				}
			}
		}
	else
		{
		while (l >= (unsigned long)n)
			{
			l-=n;
			ti[0]=v0;
			ti[1]=v1;
			DES_encrypt1((DES_LONG *)ti,schedule,DES_ENCRYPT);
			c2ln(in,d0,d1,n);
			in+=n;
			/* 30-08-94 - eay - changed because l>>32 and
			 * l<<32 are bad under gcc :-( */
			if (numbits == 32)
				{ v0=v1; v1=d0; }
			else if (numbits == 64)
				{ v0=d0; v1=d1; }
			else
				{
#ifndef L_ENDIAN
				iv=&ovec[0];
				l2c(v0,iv);
				l2c(v1,iv);
				l2c(d0,iv);
				l2c(d1,iv);
#else
				sh[0]=v0, sh[1]=v1, sh[2]=d0, sh[3]=d1;
#endif
				if (rem==0)
					memmove(ovec,ovec+num,8);
				else
					for(i=0 ; i < 8 ; ++i)
						ovec[i]=ovec[i+num]<<rem |
							ovec[i+num+1]>>(8-rem);
#ifdef L_ENDIAN
				v0=sh[0], v1=sh[1];
#else
				iv=&ovec[0];
				c2l(iv,v0);
				c2l(iv,v1);
#endif
				}
			d0^=ti[0];
			d1^=ti[1];
			l2cn(d0,d1,out,n);
			out+=n;
			}
		}
	iv = &(*ivec)[0];
	l2c(v0,iv);
	l2c(v1,iv);
	v0=v1=d0=d1=ti[0]=ti[1]=0;
	}

