/* crypto/ripemd/rmd_locl.h */
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

#include <stdlib.h>
#include <string.h>
#include <openssl/opensslconf.h>
#include <openssl/ripemd.h>

#ifndef RIPEMD160_LONG_LOG2
#define RIPEMD160_LONG_LOG2 2 /* default to 32 bits */
#endif

/*
 * DO EXAMINE COMMENTS IN crypto/md5/md5_locl.h & crypto/md5/md5_dgst.c
 * FOR EXPLANATIONS ON FOLLOWING "CODE."
 *					<appro@fy.chalmers.se>
 */
#ifdef RMD160_ASM
# if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__INTEL__)
#  define ripemd160_block_host_order ripemd160_block_asm_data_order
# endif
#endif

void ripemd160_block_data_order (RIPEMD160_CTX *c, const void *p,size_t num);

#define DATA_ORDER_IS_LITTLE_ENDIAN

#define HASH_LONG               RIPEMD160_LONG
#define HASH_CTX                RIPEMD160_CTX
#define HASH_CBLOCK             RIPEMD160_CBLOCK
#define HASH_UPDATE             RIPEMD160_Update
#define HASH_TRANSFORM          RIPEMD160_Transform
#define HASH_FINAL              RIPEMD160_Final
#define	HASH_MAKE_STRING(c,s)	do {	\
	unsigned long ll;		\
	ll=(c)->A; HOST_l2c(ll,(s));	\
	ll=(c)->B; HOST_l2c(ll,(s));	\
	ll=(c)->C; HOST_l2c(ll,(s));	\
	ll=(c)->D; HOST_l2c(ll,(s));	\
	ll=(c)->E; HOST_l2c(ll,(s));	\
	} while (0)
#define HASH_BLOCK_DATA_ORDER   ripemd160_block_data_order

#include "md32_common.h"

#if 0
#define F1(x,y,z)	 ((x)^(y)^(z))
#define F2(x,y,z)	(((x)&(y))|((~x)&z))
#define F3(x,y,z)	(((x)|(~y))^(z))
#define F4(x,y,z)	(((x)&(z))|((y)&(~(z))))
#define F5(x,y,z)	 ((x)^((y)|(~(z))))
#else
/*
 * Transformed F2 and F4 are courtesy of Wei Dai <weidai@eskimo.com>
 */
#define F1(x,y,z)	((x) ^ (y) ^ (z))
#define F2(x,y,z)	((((y) ^ (z)) & (x)) ^ (z))
#define F3(x,y,z)	(((~(y)) | (x)) ^ (z))
#define F4(x,y,z)	((((x) ^ (y)) & (z)) ^ (y))
#define F5(x,y,z)	(((~(z)) | (y)) ^ (x))
#endif

#define RIPEMD160_A	0x67452301L
#define RIPEMD160_B	0xEFCDAB89L
#define RIPEMD160_C	0x98BADCFEL
#define RIPEMD160_D	0x10325476L
#define RIPEMD160_E	0xC3D2E1F0L

#include "rmdconst.h"

#define RIP1(a,b,c,d,e,w,s) { \
	a+=F1(b,c,d)+X(w); \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP2(a,b,c,d,e,w,s,K) { \
	a+=F2(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP3(a,b,c,d,e,w,s,K) { \
	a+=F3(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP4(a,b,c,d,e,w,s,K) { \
	a+=F4(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP5(a,b,c,d,e,w,s,K) { \
	a+=F5(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

