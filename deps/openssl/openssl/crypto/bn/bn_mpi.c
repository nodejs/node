/* crypto/bn/bn_mpi.c */
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
#include "cryptlib.h"
#include "bn_lcl.h"

int BN_bn2mpi(const BIGNUM *a, unsigned char *d)
	{
	int bits;
	int num=0;
	int ext=0;
	long l;

	bits=BN_num_bits(a);
	num=(bits+7)/8;
	if (bits > 0)
		{
		ext=((bits & 0x07) == 0);
		}
	if (d == NULL)
		return(num+4+ext);

	l=num+ext;
	d[0]=(unsigned char)(l>>24)&0xff;
	d[1]=(unsigned char)(l>>16)&0xff;
	d[2]=(unsigned char)(l>> 8)&0xff;
	d[3]=(unsigned char)(l    )&0xff;
	if (ext) d[4]=0;
	num=BN_bn2bin(a,&(d[4+ext]));
	if (a->neg)
		d[4]|=0x80;
	return(num+4+ext);
	}

BIGNUM *BN_mpi2bn(const unsigned char *d, int n, BIGNUM *a)
	{
	long len;
	int neg=0;

	if (n < 4)
		{
		BNerr(BN_F_BN_MPI2BN,BN_R_INVALID_LENGTH);
		return(NULL);
		}
	len=((long)d[0]<<24)|((long)d[1]<<16)|((int)d[2]<<8)|(int)d[3];
	if ((len+4) != n)
		{
		BNerr(BN_F_BN_MPI2BN,BN_R_ENCODING_ERROR);
		return(NULL);
		}

	if (a == NULL) a=BN_new();
	if (a == NULL) return(NULL);

	if (len == 0)
		{
		a->neg=0;
		a->top=0;
		return(a);
		}
	d+=4;
	if ((*d) & 0x80)
		neg=1;
	if (BN_bin2bn(d,(int)len,a) == NULL)
		return(NULL);
	a->neg=neg;
	if (neg)
		{
		BN_clear_bit(a,BN_num_bits(a)-1);
		}
	bn_check_top(a);
	return(a);
	}

