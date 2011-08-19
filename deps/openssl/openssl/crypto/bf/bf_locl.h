/* crypto/bf/bf_locl.h */
/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
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

#ifndef HEADER_BF_LOCL_H
#define HEADER_BF_LOCL_H
#include <openssl/opensslconf.h> /* BF_PTR, BF_PTR2 */

#undef c2l
#define c2l(c,l)	(l =((unsigned long)(*((c)++)))    , \
			 l|=((unsigned long)(*((c)++)))<< 8L, \
			 l|=((unsigned long)(*((c)++)))<<16L, \
			 l|=((unsigned long)(*((c)++)))<<24L)

/* NOTE - c is not incremented as per c2l */
#undef c2ln
#define c2ln(c,l1,l2,n)	{ \
			c+=n; \
			l1=l2=0; \
			switch (n) { \
			case 8: l2 =((unsigned long)(*(--(c))))<<24L; \
			case 7: l2|=((unsigned long)(*(--(c))))<<16L; \
			case 6: l2|=((unsigned long)(*(--(c))))<< 8L; \
			case 5: l2|=((unsigned long)(*(--(c))));     \
			case 4: l1 =((unsigned long)(*(--(c))))<<24L; \
			case 3: l1|=((unsigned long)(*(--(c))))<<16L; \
			case 2: l1|=((unsigned long)(*(--(c))))<< 8L; \
			case 1: l1|=((unsigned long)(*(--(c))));     \
				} \
			}

#undef l2c
#define l2c(l,c)	(*((c)++)=(unsigned char)(((l)     )&0xff), \
			 *((c)++)=(unsigned char)(((l)>> 8L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>16L)&0xff), \
			 *((c)++)=(unsigned char)(((l)>>24L)&0xff))

/* NOTE - c is not incremented as per l2c */
#undef l2cn
#define l2cn(l1,l2,c,n)	{ \
			c+=n; \
			switch (n) { \
			case 8: *(--(c))=(unsigned char)(((l2)>>24L)&0xff); \
			case 7: *(--(c))=(unsigned char)(((l2)>>16L)&0xff); \
			case 6: *(--(c))=(unsigned char)(((l2)>> 8L)&0xff); \
			case 5: *(--(c))=(unsigned char)(((l2)     )&0xff); \
			case 4: *(--(c))=(unsigned char)(((l1)>>24L)&0xff); \
			case 3: *(--(c))=(unsigned char)(((l1)>>16L)&0xff); \
			case 2: *(--(c))=(unsigned char)(((l1)>> 8L)&0xff); \
			case 1: *(--(c))=(unsigned char)(((l1)     )&0xff); \
				} \
			}

/* NOTE - c is not incremented as per n2l */
#define n2ln(c,l1,l2,n)	{ \
			c+=n; \
			l1=l2=0; \
			switch (n) { \
			case 8: l2 =((unsigned long)(*(--(c))))    ; \
			case 7: l2|=((unsigned long)(*(--(c))))<< 8; \
			case 6: l2|=((unsigned long)(*(--(c))))<<16; \
			case 5: l2|=((unsigned long)(*(--(c))))<<24; \
			case 4: l1 =((unsigned long)(*(--(c))))    ; \
			case 3: l1|=((unsigned long)(*(--(c))))<< 8; \
			case 2: l1|=((unsigned long)(*(--(c))))<<16; \
			case 1: l1|=((unsigned long)(*(--(c))))<<24; \
				} \
			}

/* NOTE - c is not incremented as per l2n */
#define l2nn(l1,l2,c,n)	{ \
			c+=n; \
			switch (n) { \
			case 8: *(--(c))=(unsigned char)(((l2)    )&0xff); \
			case 7: *(--(c))=(unsigned char)(((l2)>> 8)&0xff); \
			case 6: *(--(c))=(unsigned char)(((l2)>>16)&0xff); \
			case 5: *(--(c))=(unsigned char)(((l2)>>24)&0xff); \
			case 4: *(--(c))=(unsigned char)(((l1)    )&0xff); \
			case 3: *(--(c))=(unsigned char)(((l1)>> 8)&0xff); \
			case 2: *(--(c))=(unsigned char)(((l1)>>16)&0xff); \
			case 1: *(--(c))=(unsigned char)(((l1)>>24)&0xff); \
				} \
			}

#undef n2l
#define n2l(c,l)        (l =((unsigned long)(*((c)++)))<<24L, \
                         l|=((unsigned long)(*((c)++)))<<16L, \
                         l|=((unsigned long)(*((c)++)))<< 8L, \
                         l|=((unsigned long)(*((c)++))))

#undef l2n
#define l2n(l,c)        (*((c)++)=(unsigned char)(((l)>>24L)&0xff), \
                         *((c)++)=(unsigned char)(((l)>>16L)&0xff), \
                         *((c)++)=(unsigned char)(((l)>> 8L)&0xff), \
                         *((c)++)=(unsigned char)(((l)     )&0xff))

/* This is actually a big endian algorithm, the most significant byte
 * is used to lookup array 0 */

#if defined(BF_PTR2)

/*
 * This is basically a special Intel version. Point is that Intel
 * doesn't have many registers, but offers a reach choice of addressing
 * modes. So we spare some registers by directly traversing BF_KEY
 * structure and hiring the most decorated addressing mode. The code
 * generated by EGCS is *perfectly* competitive with assembler
 * implementation!
 */
#define BF_ENC(LL,R,KEY,Pi) (\
	LL^=KEY[Pi], \
	t=  KEY[BF_ROUNDS+2 +   0 + ((R>>24)&0xFF)], \
	t+= KEY[BF_ROUNDS+2 + 256 + ((R>>16)&0xFF)], \
	t^= KEY[BF_ROUNDS+2 + 512 + ((R>>8 )&0xFF)], \
	t+= KEY[BF_ROUNDS+2 + 768 + ((R    )&0xFF)], \
	LL^=t \
	)

#elif defined(BF_PTR)

#ifndef BF_LONG_LOG2
#define BF_LONG_LOG2  2       /* default to BF_LONG being 32 bits */
#endif
#define BF_M  (0xFF<<BF_LONG_LOG2)
#define BF_0  (24-BF_LONG_LOG2)
#define BF_1  (16-BF_LONG_LOG2)
#define BF_2  ( 8-BF_LONG_LOG2)
#define BF_3  BF_LONG_LOG2 /* left shift */

/*
 * This is normally very good on RISC platforms where normally you
 * have to explicitly "multiply" array index by sizeof(BF_LONG)
 * in order to calculate the effective address. This implementation
 * excuses CPU from this extra work. Power[PC] uses should have most
 * fun as (R>>BF_i)&BF_M gets folded into a single instruction, namely
 * rlwinm. So let'em double-check if their compiler does it.
 */

#define BF_ENC(LL,R,S,P) ( \
	LL^=P, \
	LL^= (((*(BF_LONG *)((unsigned char *)&(S[  0])+((R>>BF_0)&BF_M))+ \
		*(BF_LONG *)((unsigned char *)&(S[256])+((R>>BF_1)&BF_M)))^ \
		*(BF_LONG *)((unsigned char *)&(S[512])+((R>>BF_2)&BF_M)))+ \
		*(BF_LONG *)((unsigned char *)&(S[768])+((R<<BF_3)&BF_M))) \
	)
#else

/*
 * This is a *generic* version. Seem to perform best on platforms that
 * offer explicit support for extraction of 8-bit nibbles preferably
 * complemented with "multiplying" of array index by sizeof(BF_LONG).
 * For the moment of this writing the list comprises Alpha CPU featuring
 * extbl and s[48]addq instructions.
 */

#define BF_ENC(LL,R,S,P) ( \
	LL^=P, \
	LL^=(((	S[       ((int)(R>>24)&0xff)] + \
		S[0x0100+((int)(R>>16)&0xff)])^ \
		S[0x0200+((int)(R>> 8)&0xff)])+ \
		S[0x0300+((int)(R    )&0xff)])&0xffffffffL \
	)
#endif

#endif
