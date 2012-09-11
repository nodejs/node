/* crypto/bn/bn_print.c */
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
#include <ctype.h>
#include "cryptlib.h"
#include <openssl/buffer.h>
#include "bn_lcl.h"

static const char Hex[]="0123456789ABCDEF";

/* Must 'OPENSSL_free' the returned data */
char *BN_bn2hex(const BIGNUM *a)
	{
	int i,j,v,z=0;
	char *buf;
	char *p;

	buf=(char *)OPENSSL_malloc(a->top*BN_BYTES*2+2);
	if (buf == NULL)
		{
		BNerr(BN_F_BN_BN2HEX,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	p=buf;
	if (a->neg) *(p++)='-';
	if (BN_is_zero(a)) *(p++)='0';
	for (i=a->top-1; i >=0; i--)
		{
		for (j=BN_BITS2-8; j >= 0; j-=8)
			{
			/* strip leading zeros */
			v=((int)(a->d[i]>>(long)j))&0xff;
			if (z || (v != 0))
				{
				*(p++)=Hex[v>>4];
				*(p++)=Hex[v&0x0f];
				z=1;
				}
			}
		}
	*p='\0';
err:
	return(buf);
	}

/* Must 'OPENSSL_free' the returned data */
char *BN_bn2dec(const BIGNUM *a)
	{
	int i=0,num, ok = 0;
	char *buf=NULL;
	char *p;
	BIGNUM *t=NULL;
	BN_ULONG *bn_data=NULL,*lp;

	/* get an upper bound for the length of the decimal integer
	 * num <= (BN_num_bits(a) + 1) * log(2)
	 *     <= 3 * BN_num_bits(a) * 0.1001 + log(2) + 1     (rounding error)
	 *     <= BN_num_bits(a)/10 + BN_num_bits/1000 + 1 + 1 
	 */
	i=BN_num_bits(a)*3;
	num=(i/10+i/1000+1)+1;
	bn_data=(BN_ULONG *)OPENSSL_malloc((num/BN_DEC_NUM+1)*sizeof(BN_ULONG));
	buf=(char *)OPENSSL_malloc(num+3);
	if ((buf == NULL) || (bn_data == NULL))
		{
		BNerr(BN_F_BN_BN2DEC,ERR_R_MALLOC_FAILURE);
		goto err;
		}
	if ((t=BN_dup(a)) == NULL) goto err;

#define BUF_REMAIN (num+3 - (size_t)(p - buf))
	p=buf;
	lp=bn_data;
	if (BN_is_zero(t))
		{
		*(p++)='0';
		*(p++)='\0';
		}
	else
		{
		if (BN_is_negative(t))
			*p++ = '-';

		i=0;
		while (!BN_is_zero(t))
			{
			*lp=BN_div_word(t,BN_DEC_CONV);
			lp++;
			}
		lp--;
		/* We now have a series of blocks, BN_DEC_NUM chars
		 * in length, where the last one needs truncation.
		 * The blocks need to be reversed in order. */
		BIO_snprintf(p,BUF_REMAIN,BN_DEC_FMT1,*lp);
		while (*p) p++;
		while (lp != bn_data)
			{
			lp--;
			BIO_snprintf(p,BUF_REMAIN,BN_DEC_FMT2,*lp);
			while (*p) p++;
			}
		}
	ok = 1;
err:
	if (bn_data != NULL) OPENSSL_free(bn_data);
	if (t != NULL) BN_free(t);
	if (!ok && buf)
		{
		OPENSSL_free(buf);
		buf = NULL;
		}

	return(buf);
	}

int BN_hex2bn(BIGNUM **bn, const char *a)
	{
	BIGNUM *ret=NULL;
	BN_ULONG l=0;
	int neg=0,h,m,i,j,k,c;
	int num;

	if ((a == NULL) || (*a == '\0')) return(0);

	if (*a == '-') { neg=1; a++; }

	for (i=0; isxdigit((unsigned char) a[i]); i++)
		;

	num=i+neg;
	if (bn == NULL) return(num);

	/* a is the start of the hex digits, and it is 'i' long */
	if (*bn == NULL)
		{
		if ((ret=BN_new()) == NULL) return(0);
		}
	else
		{
		ret= *bn;
		BN_zero(ret);
		}

	/* i is the number of hex digests; */
	if (bn_expand(ret,i*4) == NULL) goto err;

	j=i; /* least significant 'hex' */
	m=0;
	h=0;
	while (j > 0)
		{
		m=((BN_BYTES*2) <= j)?(BN_BYTES*2):j;
		l=0;
		for (;;)
			{
			c=a[j-m];
			if ((c >= '0') && (c <= '9')) k=c-'0';
			else if ((c >= 'a') && (c <= 'f')) k=c-'a'+10;
			else if ((c >= 'A') && (c <= 'F')) k=c-'A'+10;
			else k=0; /* paranoia */
			l=(l<<4)|k;

			if (--m <= 0)
				{
				ret->d[h++]=l;
				break;
				}
			}
		j-=(BN_BYTES*2);
		}
	ret->top=h;
	bn_correct_top(ret);
	ret->neg=neg;

	*bn=ret;
	bn_check_top(ret);
	return(num);
err:
	if (*bn == NULL) BN_free(ret);
	return(0);
	}

int BN_dec2bn(BIGNUM **bn, const char *a)
	{
	BIGNUM *ret=NULL;
	BN_ULONG l=0;
	int neg=0,i,j;
	int num;

	if ((a == NULL) || (*a == '\0')) return(0);
	if (*a == '-') { neg=1; a++; }

	for (i=0; isdigit((unsigned char) a[i]); i++)
		;

	num=i+neg;
	if (bn == NULL) return(num);

	/* a is the start of the digits, and it is 'i' long.
	 * We chop it into BN_DEC_NUM digits at a time */
	if (*bn == NULL)
		{
		if ((ret=BN_new()) == NULL) return(0);
		}
	else
		{
		ret= *bn;
		BN_zero(ret);
		}

	/* i is the number of digests, a bit of an over expand; */
	if (bn_expand(ret,i*4) == NULL) goto err;

	j=BN_DEC_NUM-(i%BN_DEC_NUM);
	if (j == BN_DEC_NUM) j=0;
	l=0;
	while (*a)
		{
		l*=10;
		l+= *a-'0';
		a++;
		if (++j == BN_DEC_NUM)
			{
			BN_mul_word(ret,BN_DEC_CONV);
			BN_add_word(ret,l);
			l=0;
			j=0;
			}
		}
	ret->neg=neg;

	bn_correct_top(ret);
	*bn=ret;
	bn_check_top(ret);
	return(num);
err:
	if (*bn == NULL) BN_free(ret);
	return(0);
	}

int BN_asc2bn(BIGNUM **bn, const char *a)
	{
	const char *p = a;
	if (*p == '-')
		p++;

	if (p[0] == '0' && (p[1] == 'X' || p[1] == 'x'))
		{		
		if (!BN_hex2bn(bn, p + 2))
			return 0;
		}
	else
		{
		if (!BN_dec2bn(bn, p))
			return 0;
		}
	if (*a == '-')
		(*bn)->neg = 1;
	return 1;
	}

#ifndef OPENSSL_NO_BIO
#ifndef OPENSSL_NO_FP_API
int BN_print_fp(FILE *fp, const BIGNUM *a)
	{
	BIO *b;
	int ret;

	if ((b=BIO_new(BIO_s_file())) == NULL)
		return(0);
	BIO_set_fp(b,fp,BIO_NOCLOSE);
	ret=BN_print(b,a);
	BIO_free(b);
	return(ret);
	}
#endif

int BN_print(BIO *bp, const BIGNUM *a)
	{
	int i,j,v,z=0;
	int ret=0;

	if ((a->neg) && (BIO_write(bp,"-",1) != 1)) goto end;
	if (BN_is_zero(a) && (BIO_write(bp,"0",1) != 1)) goto end;
	for (i=a->top-1; i >=0; i--)
		{
		for (j=BN_BITS2-4; j >= 0; j-=4)
			{
			/* strip leading zeros */
			v=((int)(a->d[i]>>(long)j))&0x0f;
			if (z || (v != 0))
				{
				if (BIO_write(bp,&(Hex[v]),1) != 1)
					goto end;
				z=1;
				}
			}
		}
	ret=1;
end:
	return(ret);
	}
#endif

char *BN_options(void)
	{
	static int init=0;
	static char data[16];

	if (!init)
		{
		init++;
#ifdef BN_LLONG
		BIO_snprintf(data,sizeof data,"bn(%d,%d)",
			     (int)sizeof(BN_ULLONG)*8,(int)sizeof(BN_ULONG)*8);
#else
		BIO_snprintf(data,sizeof data,"bn(%d,%d)",
			     (int)sizeof(BN_ULONG)*8,(int)sizeof(BN_ULONG)*8);
#endif
		}
	return(data);
	}
