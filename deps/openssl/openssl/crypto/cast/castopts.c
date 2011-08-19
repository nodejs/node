/* crypto/cast/castopts.c */
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

/* define PART1, PART2, PART3 or PART4 to build only with a few of the options.
 * This is for machines with 64k code segment size restrictions. */

#if !defined(OPENSSL_SYS_MSDOS) && (!defined(OPENSSL_SYS_VMS) || defined(__DECC))
#define TIMES
#endif

#include <stdio.h>

#include <openssl/e_os2.h>
#include OPENSSL_UNISTD_IO
OPENSSL_DECLARE_EXIT

#ifndef OPENSSL_SYS_NETWARE
#include <signal.h>
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

#include <openssl/cast.h>

#define CAST_DEFAULT_OPTIONS

#undef E_CAST
#define CAST_encrypt  CAST_encrypt_normal
#define CAST_decrypt  CAST_decrypt_normal
#define CAST_cbc_encrypt  CAST_cbc_encrypt_normal
#undef HEADER_CAST_LOCL_H
#include "c_enc.c"

#define CAST_PTR
#undef CAST_PTR2
#undef E_CAST
#undef CAST_encrypt
#undef CAST_decrypt
#undef CAST_cbc_encrypt
#define CAST_encrypt  CAST_encrypt_ptr
#define CAST_decrypt  CAST_decrypt_ptr
#define CAST_cbc_encrypt  CAST_cbc_encrypt_ptr
#undef HEADER_CAST_LOCL_H
#include "c_enc.c"

#undef CAST_PTR
#define CAST_PTR2
#undef E_CAST
#undef CAST_encrypt
#undef CAST_decrypt
#undef CAST_cbc_encrypt
#define CAST_encrypt  CAST_encrypt_ptr2
#define CAST_decrypt  CAST_decrypt_ptr2
#define CAST_cbc_encrypt  CAST_cbc_encrypt_ptr2
#undef HEADER_CAST_LOCL_H
#include "c_enc.c"

/* The following if from times(3) man page.  It may need to be changed */
#ifndef HZ
# ifndef CLK_TCK
#  ifndef _BSD_CLK_TCK_ /* FreeBSD fix */
#   define HZ	100.0
#  else /* _BSD_CLK_TCK_ */
#   define HZ ((double)_BSD_CLK_TCK_)
#  endif
# else /* CLK_TCK */
#  define HZ ((double)CLK_TCK)
# endif
#endif

#define BUFSIZE	((long)1024)
long run=0;

double Time_F(int s);
#ifdef SIGALRM
#if defined(__STDC__) || defined(sgi)
#define SIGRETTYPE void
#else
#define SIGRETTYPE int
#endif

SIGRETTYPE sig_done(int sig);
SIGRETTYPE sig_done(int sig)
	{
	signal(SIGALRM,sig_done);
	run=0;
#ifdef LINT
	sig=sig;
#endif
	}
#endif

#define START	0
#define STOP	1

double Time_F(int s)
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
		return((ret == 0.0)?1e-6:ret);
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
		return((ret == 0.0)?1e-6:ret);
		}
#endif
	}

#ifdef SIGALRM
#define print_name(name) fprintf(stderr,"Doing %s's for 10 seconds\n",name); alarm(10);
#else
#define print_name(name) fprintf(stderr,"Doing %s %ld times\n",name,cb);
#endif
	
#define time_it(func,name,index) \
	print_name(name); \
	Time_F(START); \
	for (count=0,run=1; COND(cb); count+=4) \
		{ \
		unsigned long d[2]; \
		func(d,&sch); \
		func(d,&sch); \
		func(d,&sch); \
		func(d,&sch); \
		} \
	tm[index]=Time_F(STOP); \
	fprintf(stderr,"%ld %s's in %.2f second\n",count,name,tm[index]); \
	tm[index]=((double)COUNT(cb))/tm[index];

#define print_it(name,index) \
	fprintf(stderr,"%s bytes per sec = %12.2f (%5.1fuS)\n",name, \
		tm[index]*8,1.0e6/tm[index]);

int main(int argc, char **argv)
	{
	long count;
	static unsigned char buf[BUFSIZE];
	static char key[16]={	0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
				0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0};
	CAST_KEY sch;
	double d,tm[16],max=0;
	int rank[16];
	char *str[16];
	int max_idx=0,i,num=0,j;
#ifndef SIGALARM
	long ca,cb,cc,cd,ce;
#endif

	for (i=0; i<12; i++)
		{
		tm[i]=0.0;
		rank[i]=0;
		}

#ifndef TIMES
	fprintf(stderr,"To get the most accurate results, try to run this\n");
	fprintf(stderr,"program when this computer is idle.\n");
#endif

	CAST_set_key(&sch,16,key);

#ifndef SIGALRM
	fprintf(stderr,"First we calculate the approximate speed ...\n");
	count=10;
	do	{
		long i;
		unsigned long data[2];

		count*=2;
		Time_F(START);
		for (i=count; i; i--)
			CAST_encrypt(data,&sch);
		d=Time_F(STOP);
		} while (d < 3.0);
	ca=count;
	cb=count*3;
	cc=count*3*8/BUFSIZE+1;
	cd=count*8/BUFSIZE+1;

	ce=count/20+1;
#define COND(d) (count != (d))
#define COUNT(d) (d)
#else
#define COND(c) (run)
#define COUNT(d) (count)
        signal(SIGALRM,sig_done);
        alarm(10);
#endif

	time_it(CAST_encrypt_normal,	"CAST_encrypt_normal ", 0);
	time_it(CAST_encrypt_ptr,	"CAST_encrypt_ptr    ", 1);
	time_it(CAST_encrypt_ptr2,	"CAST_encrypt_ptr2   ", 2);
	num+=3;

	str[0]="<nothing>";
	print_it("CAST_encrypt_normal ",0);
	max=tm[0];
	max_idx=0;
	str[1]="ptr      ";
	print_it("CAST_encrypt_ptr ",1);
	if (max < tm[1]) { max=tm[1]; max_idx=1; }
	str[2]="ptr2     ";
	print_it("CAST_encrypt_ptr2 ",2);
	if (max < tm[2]) { max=tm[2]; max_idx=2; }

	printf("options    CAST ecb/s\n");
	printf("%s %12.2f 100.0%%\n",str[max_idx],tm[max_idx]);
	d=tm[max_idx];
	tm[max_idx]= -2.0;
	max= -1.0;
	for (;;)
		{
		for (i=0; i<3; i++)
			{
			if (max < tm[i]) { max=tm[i]; j=i; }
			}
		if (max < 0.0) break;
		printf("%s %12.2f  %4.1f%%\n",str[j],tm[j],tm[j]/d*100.0);
		tm[j]= -2.0;
		max= -1.0;
		}

	switch (max_idx)
		{
	case 0:
		printf("-DCAST_DEFAULT_OPTIONS\n");
		break;
	case 1:
		printf("-DCAST_PTR\n");
		break;
	case 2:
		printf("-DCAST_PTR2\n");
		break;
		}
	exit(0);
#if defined(LINT) || defined(OPENSSL_SYS_MSDOS)
	return(0);
#endif
	}

