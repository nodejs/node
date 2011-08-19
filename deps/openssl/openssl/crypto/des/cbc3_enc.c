/* crypto/des/cbc3_enc.c */
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

/* HAS BUGS! DON'T USE - this is only present for use in des.c */
void DES_3cbc_encrypt(DES_cblock *input, DES_cblock *output, long length,
	     DES_key_schedule ks1, DES_key_schedule ks2, DES_cblock *iv1,
	     DES_cblock *iv2, int enc)
	{
	int off=((int)length-1)/8;
	long l8=((length+7)/8)*8;
	DES_cblock niv1,niv2;

	if (enc == DES_ENCRYPT)
		{
		DES_cbc_encrypt((unsigned char*)input,
				(unsigned char*)output,length,&ks1,iv1,enc);
		if (length >= sizeof(DES_cblock))
			memcpy(niv1,output[off],sizeof(DES_cblock));
		DES_cbc_encrypt((unsigned char*)output,
				(unsigned char*)output,l8,&ks2,iv1,!enc);
		DES_cbc_encrypt((unsigned char*)output,
				(unsigned char*)output,l8,&ks1,iv2,enc);
		if (length >= sizeof(DES_cblock))
			memcpy(niv2,output[off],sizeof(DES_cblock));
		}
	else
		{
		if (length >= sizeof(DES_cblock))
			memcpy(niv2,input[off],sizeof(DES_cblock));
		DES_cbc_encrypt((unsigned char*)input,
				(unsigned char*)output,l8,&ks1,iv2,enc);
		DES_cbc_encrypt((unsigned char*)output,
				(unsigned char*)output,l8,&ks2,iv1,!enc);
		if (length >= sizeof(DES_cblock))
			memcpy(niv1,output[off],sizeof(DES_cblock));
		DES_cbc_encrypt((unsigned char*)output,
				(unsigned char*)output,length,&ks1,iv1,enc);
		}
	memcpy(*iv1,niv1,sizeof(DES_cblock));
	memcpy(*iv2,niv2,sizeof(DES_cblock));
	}

