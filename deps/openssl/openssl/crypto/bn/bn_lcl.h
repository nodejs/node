/* crypto/bn/bn_lcl.h */
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
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef HEADER_BN_LCL_H
#define HEADER_BN_LCL_H

#include <openssl/bn.h>

#ifdef  __cplusplus
extern "C" {
#endif


/*
 * BN_window_bits_for_exponent_size -- macro for sliding window mod_exp functions
 *
 *
 * For window size 'w' (w >= 2) and a random 'b' bits exponent,
 * the number of multiplications is a constant plus on average
 *
 *    2^(w-1) + (b-w)/(w+1);
 *
 * here  2^(w-1)  is for precomputing the table (we actually need
 * entries only for windows that have the lowest bit set), and
 * (b-w)/(w+1)  is an approximation for the expected number of
 * w-bit windows, not counting the first one.
 *
 * Thus we should use
 *
 *    w >= 6  if        b > 671
 *     w = 5  if  671 > b > 239
 *     w = 4  if  239 > b >  79
 *     w = 3  if   79 > b >  23
 *    w <= 2  if   23 > b
 *
 * (with draws in between).  Very small exponents are often selected
 * with low Hamming weight, so we use  w = 1  for b <= 23.
 */
#if 1
#define BN_window_bits_for_exponent_size(b) \
		((b) > 671 ? 6 : \
		 (b) > 239 ? 5 : \
		 (b) >  79 ? 4 : \
		 (b) >  23 ? 3 : 1)
#else
/* Old SSLeay/OpenSSL table.
 * Maximum window size was 5, so this table differs for b==1024;
 * but it coincides for other interesting values (b==160, b==512).
 */
#define BN_window_bits_for_exponent_size(b) \
		((b) > 255 ? 5 : \
		 (b) > 127 ? 4 : \
		 (b) >  17 ? 3 : 1)
#endif	 



/* BN_mod_exp_mont_conttime is based on the assumption that the
 * L1 data cache line width of the target processor is at least
 * the following value.
 */
#define MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH	( 64 )
#define MOD_EXP_CTIME_MIN_CACHE_LINE_MASK	(MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH - 1)

/* Window sizes optimized for fixed window size modular exponentiation
 * algorithm (BN_mod_exp_mont_consttime).
 *
 * To achieve the security goals of BN_mode_exp_mont_consttime, the
 * maximum size of the window must not exceed
 * log_2(MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH). 
 *
 * Window size thresholds are defined for cache line sizes of 32 and 64,
 * cache line sizes where log_2(32)=5 and log_2(64)=6 respectively. A
 * window size of 7 should only be used on processors that have a 128
 * byte or greater cache line size.
 */
#if MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH == 64

#  define BN_window_bits_for_ctime_exponent_size(b) \
		((b) > 937 ? 6 : \
		 (b) > 306 ? 5 : \
		 (b) >  89 ? 4 : \
		 (b) >  22 ? 3 : 1)
#  define BN_MAX_WINDOW_BITS_FOR_CTIME_EXPONENT_SIZE	(6)

#elif MOD_EXP_CTIME_MIN_CACHE_LINE_WIDTH == 32

#  define BN_window_bits_for_ctime_exponent_size(b) \
		((b) > 306 ? 5 : \
		 (b) >  89 ? 4 : \
		 (b) >  22 ? 3 : 1)
#  define BN_MAX_WINDOW_BITS_FOR_CTIME_EXPONENT_SIZE	(5)

#endif


/* Pentium pro 16,16,16,32,64 */
/* Alpha       16,16,16,16.64 */
#define BN_MULL_SIZE_NORMAL			(16) /* 32 */
#define BN_MUL_RECURSIVE_SIZE_NORMAL		(16) /* 32 less than */
#define BN_SQR_RECURSIVE_SIZE_NORMAL		(16) /* 32 */
#define BN_MUL_LOW_RECURSIVE_SIZE_NORMAL	(32) /* 32 */
#define BN_MONT_CTX_SET_SIZE_WORD		(64) /* 32 */

#if !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM) && !defined(PEDANTIC)
/*
 * BN_UMULT_HIGH section.
 *
 * No, I'm not trying to overwhelm you when stating that the
 * product of N-bit numbers is 2*N bits wide:-) No, I don't expect
 * you to be impressed when I say that if the compiler doesn't
 * support 2*N integer type, then you have to replace every N*N
 * multiplication with 4 (N/2)*(N/2) accompanied by some shifts
 * and additions which unavoidably results in severe performance
 * penalties. Of course provided that the hardware is capable of
 * producing 2*N result... That's when you normally start
 * considering assembler implementation. However! It should be
 * pointed out that some CPUs (most notably Alpha, PowerPC and
 * upcoming IA-64 family:-) provide *separate* instruction
 * calculating the upper half of the product placing the result
 * into a general purpose register. Now *if* the compiler supports
 * inline assembler, then it's not impossible to implement the
 * "bignum" routines (and have the compiler optimize 'em)
 * exhibiting "native" performance in C. That's what BN_UMULT_HIGH
 * macro is about:-)
 *
 *					<appro@fy.chalmers.se>
 */
# if defined(__alpha) && (defined(SIXTY_FOUR_BIT_LONG) || defined(SIXTY_FOUR_BIT))
#  if defined(__DECC)
#   include <c_asm.h>
#   define BN_UMULT_HIGH(a,b)	(BN_ULONG)asm("umulh %a0,%a1,%v0",(a),(b))
#  elif defined(__GNUC__) && __GNUC__>=2
#   define BN_UMULT_HIGH(a,b)	({	\
	register BN_ULONG ret;		\
	asm ("umulh	%1,%2,%0"	\
	     : "=r"(ret)		\
	     : "r"(a), "r"(b));		\
	ret;			})
#  endif	/* compiler */
# elif defined(_ARCH_PPC) && defined(__64BIT__) && defined(SIXTY_FOUR_BIT_LONG)
#  if defined(__GNUC__) && __GNUC__>=2
#   define BN_UMULT_HIGH(a,b)	({	\
	register BN_ULONG ret;		\
	asm ("mulhdu	%0,%1,%2"	\
	     : "=r"(ret)		\
	     : "r"(a), "r"(b));		\
	ret;			})
#  endif	/* compiler */
# elif (defined(__x86_64) || defined(__x86_64__)) && \
       (defined(SIXTY_FOUR_BIT_LONG) || defined(SIXTY_FOUR_BIT))
#  if defined(__GNUC__) && __GNUC__>=2
#   define BN_UMULT_HIGH(a,b)	({	\
	register BN_ULONG ret,discard;	\
	asm ("mulq	%3"		\
	     : "=a"(discard),"=d"(ret)	\
	     : "a"(a), "g"(b)		\
	     : "cc");			\
	ret;			})
#   define BN_UMULT_LOHI(low,high,a,b)	\
	asm ("mulq	%3"		\
		: "=a"(low),"=d"(high)	\
		: "a"(a),"g"(b)		\
		: "cc");
#  endif
# elif (defined(_M_AMD64) || defined(_M_X64)) && defined(SIXTY_FOUR_BIT)
#  if defined(_MSC_VER) && _MSC_VER>=1400
    unsigned __int64 __umulh	(unsigned __int64 a,unsigned __int64 b);
    unsigned __int64 _umul128	(unsigned __int64 a,unsigned __int64 b,
				 unsigned __int64 *h);
#   pragma intrinsic(__umulh,_umul128)
#   define BN_UMULT_HIGH(a,b)		__umulh((a),(b))
#   define BN_UMULT_LOHI(low,high,a,b)	((low)=_umul128((a),(b),&(high)))
#  endif
# elif defined(__mips) && (defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG))
#  if defined(__GNUC__) && __GNUC__>=2
#   if __GNUC__>=4 && __GNUC_MINOR__>=4 /* "h" constraint is no more since 4.4 */
#     define BN_UMULT_HIGH(a,b)		 (((__uint128_t)(a)*(b))>>64)
#     define BN_UMULT_LOHI(low,high,a,b) ({	\
	__uint128_t ret=(__uint128_t)(a)*(b);	\
	(high)=ret>>64; (low)=ret;	 })
#   else
#     define BN_UMULT_HIGH(a,b)	({	\
	register BN_ULONG ret;		\
	asm ("dmultu	%1,%2"		\
	     : "=h"(ret)		\
	     : "r"(a), "r"(b) : "l");	\
	ret;			})
#     define BN_UMULT_LOHI(low,high,a,b)\
	asm ("dmultu	%2,%3"		\
	     : "=l"(low),"=h"(high)	\
	     : "r"(a), "r"(b));
#    endif
#  endif
# endif		/* cpu */
#endif		/* OPENSSL_NO_ASM */

/*************************************************************
 * Using the long long type
 */
#define Lw(t)    (((BN_ULONG)(t))&BN_MASK2)
#define Hw(t)    (((BN_ULONG)((t)>>BN_BITS2))&BN_MASK2)

#ifdef BN_DEBUG_RAND
#define bn_clear_top2max(a) \
	{ \
	int      ind = (a)->dmax - (a)->top; \
	BN_ULONG *ftl = &(a)->d[(a)->top-1]; \
	for (; ind != 0; ind--) \
		*(++ftl) = 0x0; \
	}
#else
#define bn_clear_top2max(a)
#endif

#ifdef BN_LLONG
#define mul_add(r,a,w,c) { \
	BN_ULLONG t; \
	t=(BN_ULLONG)w * (a) + (r) + (c); \
	(r)= Lw(t); \
	(c)= Hw(t); \
	}

#define mul(r,a,w,c) { \
	BN_ULLONG t; \
	t=(BN_ULLONG)w * (a) + (c); \
	(r)= Lw(t); \
	(c)= Hw(t); \
	}

#define sqr(r0,r1,a) { \
	BN_ULLONG t; \
	t=(BN_ULLONG)(a)*(a); \
	(r0)=Lw(t); \
	(r1)=Hw(t); \
	}

#elif defined(BN_UMULT_LOHI)
#define mul_add(r,a,w,c) {		\
	BN_ULONG high,low,ret,tmp=(a);	\
	ret =  (r);			\
	BN_UMULT_LOHI(low,high,w,tmp);	\
	ret += (c);			\
	(c) =  (ret<(c))?1:0;		\
	(c) += high;			\
	ret += low;			\
	(c) += (ret<low)?1:0;		\
	(r) =  ret;			\
	}

#define mul(r,a,w,c)	{		\
	BN_ULONG high,low,ret,ta=(a);	\
	BN_UMULT_LOHI(low,high,w,ta);	\
	ret =  low + (c);		\
	(c) =  high;			\
	(c) += (ret<low)?1:0;		\
	(r) =  ret;			\
	}

#define sqr(r0,r1,a)	{		\
	BN_ULONG tmp=(a);		\
	BN_UMULT_LOHI(r0,r1,tmp,tmp);	\
	}

#elif defined(BN_UMULT_HIGH)
#define mul_add(r,a,w,c) {		\
	BN_ULONG high,low,ret,tmp=(a);	\
	ret =  (r);			\
	high=  BN_UMULT_HIGH(w,tmp);	\
	ret += (c);			\
	low =  (w) * tmp;		\
	(c) =  (ret<(c))?1:0;		\
	(c) += high;			\
	ret += low;			\
	(c) += (ret<low)?1:0;		\
	(r) =  ret;			\
	}

#define mul(r,a,w,c)	{		\
	BN_ULONG high,low,ret,ta=(a);	\
	low =  (w) * ta;		\
	high=  BN_UMULT_HIGH(w,ta);	\
	ret =  low + (c);		\
	(c) =  high;			\
	(c) += (ret<low)?1:0;		\
	(r) =  ret;			\
	}

#define sqr(r0,r1,a)	{		\
	BN_ULONG tmp=(a);		\
	(r0) = tmp * tmp;		\
	(r1) = BN_UMULT_HIGH(tmp,tmp);	\
	}

#else
/*************************************************************
 * No long long type
 */

#define LBITS(a)	((a)&BN_MASK2l)
#define HBITS(a)	(((a)>>BN_BITS4)&BN_MASK2l)
#define	L2HBITS(a)	(((a)<<BN_BITS4)&BN_MASK2)

#define LLBITS(a)	((a)&BN_MASKl)
#define LHBITS(a)	(((a)>>BN_BITS2)&BN_MASKl)
#define	LL2HBITS(a)	((BN_ULLONG)((a)&BN_MASKl)<<BN_BITS2)

#define mul64(l,h,bl,bh) \
	{ \
	BN_ULONG m,m1,lt,ht; \
 \
	lt=l; \
	ht=h; \
	m =(bh)*(lt); \
	lt=(bl)*(lt); \
	m1=(bl)*(ht); \
	ht =(bh)*(ht); \
	m=(m+m1)&BN_MASK2; if (m < m1) ht+=L2HBITS((BN_ULONG)1); \
	ht+=HBITS(m); \
	m1=L2HBITS(m); \
	lt=(lt+m1)&BN_MASK2; if (lt < m1) ht++; \
	(l)=lt; \
	(h)=ht; \
	}

#define sqr64(lo,ho,in) \
	{ \
	BN_ULONG l,h,m; \
 \
	h=(in); \
	l=LBITS(h); \
	h=HBITS(h); \
	m =(l)*(h); \
	l*=l; \
	h*=h; \
	h+=(m&BN_MASK2h1)>>(BN_BITS4-1); \
	m =(m&BN_MASK2l)<<(BN_BITS4+1); \
	l=(l+m)&BN_MASK2; if (l < m) h++; \
	(lo)=l; \
	(ho)=h; \
	}

#define mul_add(r,a,bl,bh,c) { \
	BN_ULONG l,h; \
 \
	h= (a); \
	l=LBITS(h); \
	h=HBITS(h); \
	mul64(l,h,(bl),(bh)); \
 \
	/* non-multiply part */ \
	l=(l+(c))&BN_MASK2; if (l < (c)) h++; \
	(c)=(r); \
	l=(l+(c))&BN_MASK2; if (l < (c)) h++; \
	(c)=h&BN_MASK2; \
	(r)=l; \
	}

#define mul(r,a,bl,bh,c) { \
	BN_ULONG l,h; \
 \
	h= (a); \
	l=LBITS(h); \
	h=HBITS(h); \
	mul64(l,h,(bl),(bh)); \
 \
	/* non-multiply part */ \
	l+=(c); if ((l&BN_MASK2) < (c)) h++; \
	(c)=h&BN_MASK2; \
	(r)=l&BN_MASK2; \
	}
#endif /* !BN_LLONG */

#if defined(OPENSSL_DOING_MAKEDEPEND) && defined(OPENSSL_FIPS)
#undef bn_div_words
#endif

void bn_mul_normal(BN_ULONG *r,BN_ULONG *a,int na,BN_ULONG *b,int nb);
void bn_mul_comba8(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b);
void bn_mul_comba4(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b);
void bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, int n, BN_ULONG *tmp);
void bn_sqr_comba8(BN_ULONG *r,const BN_ULONG *a);
void bn_sqr_comba4(BN_ULONG *r,const BN_ULONG *a);
int bn_cmp_words(const BN_ULONG *a,const BN_ULONG *b,int n);
int bn_cmp_part_words(const BN_ULONG *a, const BN_ULONG *b,
	int cl, int dl);
void bn_mul_recursive(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b,int n2,
	int dna,int dnb,BN_ULONG *t);
void bn_mul_part_recursive(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b,
	int n,int tna,int tnb,BN_ULONG *t);
void bn_sqr_recursive(BN_ULONG *r,const BN_ULONG *a, int n2, BN_ULONG *t);
void bn_mul_low_normal(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b, int n);
void bn_mul_low_recursive(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b,int n2,
	BN_ULONG *t);
void bn_mul_high(BN_ULONG *r,BN_ULONG *a,BN_ULONG *b,BN_ULONG *l,int n2,
	BN_ULONG *t);
BN_ULONG bn_add_part_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
	int cl, int dl);
BN_ULONG bn_sub_part_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
	int cl, int dl);
int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np,const BN_ULONG *n0, int num);

#ifdef  __cplusplus
}
#endif

#endif
