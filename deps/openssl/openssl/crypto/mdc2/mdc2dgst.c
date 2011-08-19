/* crypto/mdc2/mdc2dgst.c */
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
#include <stdlib.h>
#include <string.h>
#include <openssl/des.h>
#include <openssl/mdc2.h>
#include <openssl/err.h>
#ifdef OPENSSL_FIPS
#include <openssl/fips.h>
#endif


#undef c2l
#define c2l(c,l)	(l =((DES_LONG)(*((c)++)))    , \
			 l|=((DES_LONG)(*((c)++)))<< 8L, \
			 l|=((DES_LONG)(*((c)++)))<<16L, \
			 l|=((DES_LONG)(*((c)++)))<<24L)

#undef l2c
#define l2c(l,c)	(*((c)++)=(unsigned char)(((l)     )&0xff), \
			*((c)++)=(unsigned char)(((l)>> 8L)&0xff), \
			*((c)++)=(unsigned char)(((l)>>16L)&0xff), \
			*((c)++)=(unsigned char)(((l)>>24L)&0xff))

static void mdc2_body(MDC2_CTX *c, const unsigned char *in, size_t len);
FIPS_NON_FIPS_MD_Init(MDC2)
	{
	c->num=0;
	c->pad_type=1;
	memset(&(c->h[0]),0x52,MDC2_BLOCK);
	memset(&(c->hh[0]),0x25,MDC2_BLOCK);
	return 1;
	}

int MDC2_Update(MDC2_CTX *c, const unsigned char *in, size_t len)
	{
	size_t i,j;

	i=c->num;
	if (i != 0)
		{
		if (i+len < MDC2_BLOCK)
			{
			/* partial block */
			memcpy(&(c->data[i]),in,len);
			c->num+=(int)len;
			return 1;
			}
		else
			{
			/* filled one */
			j=MDC2_BLOCK-i;
			memcpy(&(c->data[i]),in,j);
			len-=j;
			in+=j;
			c->num=0;
			mdc2_body(c,&(c->data[0]),MDC2_BLOCK);
			}
		}
	i=len&~((size_t)MDC2_BLOCK-1);
	if (i > 0) mdc2_body(c,in,i);
	j=len-i;
	if (j > 0)
		{
		memcpy(&(c->data[0]),&(in[i]),j);
		c->num=(int)j;
		}
	return 1;
	}

static void mdc2_body(MDC2_CTX *c, const unsigned char *in, size_t len)
	{
	register DES_LONG tin0,tin1;
	register DES_LONG ttin0,ttin1;
	DES_LONG d[2],dd[2];
	DES_key_schedule k;
	unsigned char *p;
	size_t i;

	for (i=0; i<len; i+=8)
		{
		c2l(in,tin0); d[0]=dd[0]=tin0;
		c2l(in,tin1); d[1]=dd[1]=tin1;
		c->h[0]=(c->h[0]&0x9f)|0x40;
		c->hh[0]=(c->hh[0]&0x9f)|0x20;

		DES_set_odd_parity(&c->h);
		DES_set_key_unchecked(&c->h,&k);
		DES_encrypt1(d,&k,1);

		DES_set_odd_parity(&c->hh);
		DES_set_key_unchecked(&c->hh,&k);
		DES_encrypt1(dd,&k,1);

		ttin0=tin0^dd[0];
		ttin1=tin1^dd[1];
		tin0^=d[0];
		tin1^=d[1];

		p=c->h;
		l2c(tin0,p);
		l2c(ttin1,p);
		p=c->hh;
		l2c(ttin0,p);
		l2c(tin1,p);
		}
	}

int MDC2_Final(unsigned char *md, MDC2_CTX *c)
	{
	unsigned int i;
	int j;

	i=c->num;
	j=c->pad_type;
	if ((i > 0) || (j == 2))
		{
		if (j == 2)
			c->data[i++]=0x80;
		memset(&(c->data[i]),0,MDC2_BLOCK-i);
		mdc2_body(c,c->data,MDC2_BLOCK);
		}
	memcpy(md,(char *)c->h,MDC2_BLOCK);
	memcpy(&(md[MDC2_BLOCK]),(char *)c->hh,MDC2_BLOCK);
	return 1;
	}

#undef TEST

#ifdef TEST
main()
	{
	unsigned char md[MDC2_DIGEST_LENGTH];
	int i;
	MDC2_CTX c;
	static char *text="Now is the time for all ";

	MDC2_Init(&c);
	MDC2_Update(&c,text,strlen(text));
	MDC2_Final(&(md[0]),&c);

	for (i=0; i<MDC2_DIGEST_LENGTH; i++)
		printf("%02X",md[i]);
	printf("\n");
	}

#endif
