/* crypto/evp/e_rc5.c */
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

#ifndef OPENSSL_NO_RC5

#include <openssl/evp.h>
#include <openssl/objects.h>
#include "evp_locl.h"
#include <openssl/rc5.h>

static int r_32_12_16_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			       const unsigned char *iv,int enc);
static int rc5_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr);

typedef struct
	{
	int rounds;	/* number of rounds */
	RC5_32_KEY ks;	/* key schedule */
	} EVP_RC5_KEY;

#define data(ctx)	EVP_C_DATA(EVP_RC5_KEY,ctx)

IMPLEMENT_BLOCK_CIPHER(rc5_32_12_16, ks, RC5_32, EVP_RC5_KEY, NID_rc5,
		       8, RC5_32_KEY_LENGTH, 8, 64,
		       EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
		       r_32_12_16_init_key, NULL,
		       NULL, NULL, rc5_ctrl)

static int rc5_ctrl(EVP_CIPHER_CTX *c, int type, int arg, void *ptr)
	{
	switch(type)
		{
	case EVP_CTRL_INIT:
		data(c)->rounds = RC5_12_ROUNDS;
		return 1;

	case EVP_CTRL_GET_RC5_ROUNDS:
		*(int *)ptr = data(c)->rounds;
		return 1;
			
	case EVP_CTRL_SET_RC5_ROUNDS:
		switch(arg)
			{
		case RC5_8_ROUNDS:
		case RC5_12_ROUNDS:
		case RC5_16_ROUNDS:
			data(c)->rounds = arg;
			return 1;

		default:
			EVPerr(EVP_F_RC5_CTRL, EVP_R_UNSUPORTED_NUMBER_OF_ROUNDS);
			return 0;
			}

	default:
		return -1;
		}
	}

static int r_32_12_16_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
			       const unsigned char *iv, int enc)
	{
	RC5_32_set_key(&data(ctx)->ks,EVP_CIPHER_CTX_key_length(ctx),
		       key,data(ctx)->rounds);
	return 1;
	}

#endif
