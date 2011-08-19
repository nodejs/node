/* crypto/idea/i_skey.c */
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

#include <openssl/idea.h>
#include <openssl/crypto.h>
#ifdef OPENSSL_FIPS
#include <openssl/fips.h>
#endif

#include "idea_lcl.h"

static IDEA_INT inverse(unsigned int xin);

#ifdef OPENSSL_FIPS
void idea_set_encrypt_key(const unsigned char *key, IDEA_KEY_SCHEDULE *ks)
	{
	if (FIPS_mode())
		FIPS_BAD_ABORT(IDEA)
	private_idea_set_encrypt_key(key, ks);
	}
void private_idea_set_encrypt_key(const unsigned char *key,
						IDEA_KEY_SCHEDULE *ks)
#else
void idea_set_encrypt_key(const unsigned char *key, IDEA_KEY_SCHEDULE *ks)
#endif
	{
	int i;
	register IDEA_INT *kt,*kf,r0,r1,r2;

	kt= &(ks->data[0][0]);
	n2s(key,kt[0]); n2s(key,kt[1]); n2s(key,kt[2]); n2s(key,kt[3]);
	n2s(key,kt[4]); n2s(key,kt[5]); n2s(key,kt[6]); n2s(key,kt[7]);

	kf=kt;
	kt+=8;
	for (i=0; i<6; i++)
		{
		r2= kf[1];
		r1= kf[2];
		*(kt++)= ((r2<<9) | (r1>>7))&0xffff;
		r0= kf[3];
		*(kt++)= ((r1<<9) | (r0>>7))&0xffff;
		r1= kf[4];
		*(kt++)= ((r0<<9) | (r1>>7))&0xffff;
		r0= kf[5];
		*(kt++)= ((r1<<9) | (r0>>7))&0xffff;
		r1= kf[6];
		*(kt++)= ((r0<<9) | (r1>>7))&0xffff;
		r0= kf[7];
		*(kt++)= ((r1<<9) | (r0>>7))&0xffff;
		r1= kf[0];
		if (i >= 5) break;
		*(kt++)= ((r0<<9) | (r1>>7))&0xffff;
		*(kt++)= ((r1<<9) | (r2>>7))&0xffff;
		kf+=8;
		}
	}

void idea_set_decrypt_key(const IDEA_KEY_SCHEDULE *ek, IDEA_KEY_SCHEDULE *dk)
	{
	int r;
	register IDEA_INT *tp,t;
	const IDEA_INT *fp;

	tp= &(dk->data[0][0]);
	fp= &(ek->data[8][0]);
	for (r=0; r<9; r++)
		{
		*(tp++)=inverse(fp[0]);
		*(tp++)=((int)(0x10000L-fp[2])&0xffff);
		*(tp++)=((int)(0x10000L-fp[1])&0xffff);
		*(tp++)=inverse(fp[3]);
		if (r == 8) break;
		fp-=6;
		*(tp++)=fp[4];
		*(tp++)=fp[5];
		}

	tp= &(dk->data[0][0]);
	t=tp[1];
	tp[1]=tp[2];
	tp[2]=t;

	t=tp[49];
	tp[49]=tp[50];
	tp[50]=t;
	}

/* taken directly from the 'paper' I'll have a look at it later */
static IDEA_INT inverse(unsigned int xin)
	{
	long n1,n2,q,r,b1,b2,t;

	if (xin == 0)
		b2=0;
	else
		{
		n1=0x10001;
		n2=xin;
		b2=1;
		b1=0;

		do	{
			r=(n1%n2);
			q=(n1-r)/n2;
			if (r == 0)
				{ if (b2 < 0) b2=0x10001+b2; }
			else
				{
				n1=n2;
				n2=r;
				t=b2;
				b2=b1-q*b2;
				b1=t;
				}
			} while (r != 0);
		}
	return((IDEA_INT)b2);
	}
