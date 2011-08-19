/* unused */

/* crypto/bn/bnspeed.c */
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

/* most of this code has been pilfered from my libdes speed.c program */

#define BASENUM	1000000
#undef PROG
#define PROG bnspeed_main

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#if !defined(OPENSSL_SYS_MSDOS) && (!defined(OPENSSL_SYS_VMS) || defined(__DECC)) && !defined(OPENSSL_SYS_MACOSX)
#define TIMES
#endif

#ifndef _IRIX
#include <time.h>
#endif
#ifdef TIMES
#include <sys/types.h>
#include <sys/times.h>
#endif

/* Depending on the VMS version, the tms structure is perhaps defined.
   The __TMS macro will show if it was.  If it wasn't defined, we should
   undefine TIMES, since that tells the rest of the program how things
   should be handled.				-- Richard Levitte */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__TMS)
#undef TIMES
#endif

#ifndef TIMES
#include <sys/timeb.h>
#endif

#if defined(sun) || defined(__ultrix)
#define _POSIX_SOURCE
#include <limits.h>
#include <sys/param.h>
#endif

#include <openssl/bn.h>
#include <openssl/x509.h>

/* The following if from times(3) man page.  It may need to be changed */
#ifndef HZ
# ifndef CLK_TCK
#  ifndef _BSD_CLK_TCK_ /* FreeBSD hack */
#   define HZ	100.0
#  else /* _BSD_CLK_TCK_ */
#   define HZ ((double)_BSD_CLK_TCK_)
#  endif
# else /* CLK_TCK */
#  define HZ ((double)CLK_TCK)
# endif
#endif

#undef BUFSIZE
#define BUFSIZE	((long)1024*8)
int run=0;

static double Time_F(int s);
#define START	0
#define STOP	1

static double Time_F(int s)
	{
	double ret;
#ifdef TIMES
	static struct tms tstart,tend;

	if (s == START)
		{
		times(&tstart);
		return(0);
		}
	else
		{
		times(&tend);
		ret=((double)(tend.tms_utime-tstart.tms_utime))/HZ;
		return((ret < 1e-3)?1e-3:ret);
		}
#else /* !times() */
	static struct timeb tstart,tend;
	long i;

	if (s == START)
		{
		ftime(&tstart);
		return(0);
		}
	else
		{
		ftime(&tend);
		i=(long)tend.millitm-(long)tstart.millitm;
		ret=((double)(tend.time-tstart.time))+((double)i)/1000.0;
		return((ret < 0.001)?0.001:ret);
		}
#endif
	}

#define NUM_SIZES	5
static int sizes[NUM_SIZES]={128,256,512,1024,2048};
/*static int sizes[NUM_SIZES]={59,179,299,419,539}; */

void do_mul(BIGNUM *r,BIGNUM *a,BIGNUM *b,BN_CTX *ctx); 

int main(int argc, char **argv)
	{
	BN_CTX *ctx;
	BIGNUM a,b,c;

	ctx=BN_CTX_new();
	BN_init(&a);
	BN_init(&b);
	BN_init(&c);

	do_mul(&a,&b,&c,ctx);
	}

void do_mul(BIGNUM *r, BIGNUM *a, BIGNUM *b, BN_CTX *ctx)
	{
	int i,j,k;
	double tm;
	long num;

	for (i=0; i<NUM_SIZES; i++)
		{
		num=BASENUM;
		if (i) num/=(i*3);
		BN_rand(a,sizes[i],1,0);
		for (j=i; j<NUM_SIZES; j++)
			{
			BN_rand(b,sizes[j],1,0);
			Time_F(START);
			for (k=0; k<num; k++)
				BN_mul(r,b,a,ctx);
			tm=Time_F(STOP);
			printf("mul %4d x %4d -> %8.3fms\n",sizes[i],sizes[j],tm*1000.0/num);
			}
		}

	for (i=0; i<NUM_SIZES; i++)
		{
		num=BASENUM;
		if (i) num/=(i*3);
		BN_rand(a,sizes[i],1,0);
		Time_F(START);
		for (k=0; k<num; k++)
			BN_sqr(r,a,ctx);
		tm=Time_F(STOP);
		printf("sqr %4d x %4d -> %8.3fms\n",sizes[i],sizes[i],tm*1000.0/num);
		}

	for (i=0; i<NUM_SIZES; i++)
		{
		num=BASENUM/10;
		if (i) num/=(i*3);
		BN_rand(a,sizes[i]-1,1,0);
		for (j=i; j<NUM_SIZES; j++)
			{
			BN_rand(b,sizes[j],1,0);
			Time_F(START);
			for (k=0; k<100000; k++)
				BN_div(r, NULL, b, a,ctx);
			tm=Time_F(STOP);
			printf("div %4d / %4d -> %8.3fms\n",sizes[j],sizes[i]-1,tm*1000.0/num);
			}
		}
	}

