/* crypto/des/cfb64ede.c */
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
#include "e_os.h"

/* The input and output encrypted as though 64bit cfb mode is being
 * used.  The extra state information to record how much of the
 * 64bit block we have used is contained in *num;
 */

void DES_ede3_cfb64_encrypt(const unsigned char *in, unsigned char *out,
			    long length, DES_key_schedule *ks1,
			    DES_key_schedule *ks2, DES_key_schedule *ks3,
			    DES_cblock *ivec, int *num, int enc)
	{
	register DES_LONG v0,v1;
	register long l=length;
	register int n= *num;
	DES_LONG ti[2];
	unsigned char *iv,c,cc;

	iv=&(*ivec)[0];
	if (enc)
		{
		while (l--)
			{
			if (n == 0)
				{
				c2l(iv,v0);
				c2l(iv,v1);

				ti[0]=v0;
				ti[1]=v1;
				DES_encrypt3(ti,ks1,ks2,ks3);
				v0=ti[0];
				v1=ti[1];

				iv = &(*ivec)[0];
				l2c(v0,iv);
				l2c(v1,iv);
				iv = &(*ivec)[0];
				}
			c= *(in++)^iv[n];
			*(out++)=c;
			iv[n]=c;
			n=(n+1)&0x07;
			}
		}
	else
		{
		while (l--)
			{
			if (n == 0)
				{
				c2l(iv,v0);
				c2l(iv,v1);

				ti[0]=v0;
				ti[1]=v1;
				DES_encrypt3(ti,ks1,ks2,ks3);
				v0=ti[0];
				v1=ti[1];

				iv = &(*ivec)[0];
				l2c(v0,iv);
				l2c(v1,iv);
				iv = &(*ivec)[0];
				}
			cc= *(in++);
			c=iv[n];
			iv[n]=cc;
			*(out++)=c^cc;
			n=(n+1)&0x07;
			}
		}
	v0=v1=ti[0]=ti[1]=c=cc=0;
	*num=n;
	}

#ifdef undef /* MACRO */
void DES_ede2_cfb64_encrypt(unsigned char *in, unsigned char *out, long length,
	     DES_key_schedule ks1, DES_key_schedule ks2, DES_cblock (*ivec),
	     int *num, int enc)
	{
	DES_ede3_cfb64_encrypt(in,out,length,ks1,ks2,ks1,ivec,num,enc);
	}
#endif

/* This is compatible with the single key CFB-r for DES, even thought that's
 * not what EVP needs.
 */

void DES_ede3_cfb_encrypt(const unsigned char *in,unsigned char *out,
			  int numbits,long length,DES_key_schedule *ks1,
			  DES_key_schedule *ks2,DES_key_schedule *ks3,
			  DES_cblock *ivec,int enc)
	{
	register DES_LONG d0,d1,v0,v1;
	register unsigned long l=length,n=((unsigned int)numbits+7)/8;
	register int num=numbits,i;
	DES_LONG ti[2];
	unsigned char *iv;
	unsigned char ovec[16];

	if (num > 64) return;
	iv = &(*ivec)[0];
	c2l(iv,v0);
	c2l(iv,v1);
	if (enc)
		{
		while (l >= n)
			{
			l-=n;
			ti[0]=v0;
			ti[1]=v1;
			DES_encrypt3(ti,ks1,ks2,ks3);
			c2ln(in,d0,d1,n);
			in+=n;
			d0^=ti[0];
			d1^=ti[1];
			l2cn(d0,d1,out,n);
			out+=n;
			/* 30-08-94 - eay - changed because l>>32 and
			 * l<<32 are bad under gcc :-( */
			if (num == 32)
				{ v0=v1; v1=d0; }
			else if (num == 64)
				{ v0=d0; v1=d1; }
			else
				{
				iv=&ovec[0];
				l2c(v0,iv);
				l2c(v1,iv);
				l2c(d0,iv);
				l2c(d1,iv);
				/* shift ovec left most of the bits... */
				memmove(ovec,ovec+num/8,8+(num%8 ? 1 : 0));
				/* now the remaining bits */
				if(num%8 != 0)
					for(i=0 ; i < 8 ; ++i)
						{
						ovec[i]<<=num%8;
						ovec[i]|=ovec[i+1]>>(8-num%8);
						}
				iv=&ovec[0];
				c2l(iv,v0);
				c2l(iv,v1);
				}
			}
		}
	else
		{
		while (l >= n)
			{
			l-=n;
			ti[0]=v0;
			ti[1]=v1;
			DES_encrypt3(ti,ks1,ks2,ks3);
			c2ln(in,d0,d1,n);
			in+=n;
			/* 30-08-94 - eay - changed because l>>32 and
			 * l<<32 are bad under gcc :-( */
			if (num == 32)
				{ v0=v1; v1=d0; }
			else if (num == 64)
				{ v0=d0; v1=d1; }
			else
				{
				iv=&ovec[0];
				l2c(v0,iv);
				l2c(v1,iv);
				l2c(d0,iv);
				l2c(d1,iv);
				/* shift ovec left most of the bits... */
				memmove(ovec,ovec+num/8,8+(num%8 ? 1 : 0));
				/* now the remaining bits */
				if(num%8 != 0)
					for(i=0 ; i < 8 ; ++i)
						{
						ovec[i]<<=num%8;
						ovec[i]|=ovec[i+1]>>(8-num%8);
						}
				iv=&ovec[0];
				c2l(iv,v0);
				c2l(iv,v1);
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

