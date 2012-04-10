/* crypto/rc5/rc5.h */
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

#ifndef HEADER_RC5_H
#define HEADER_RC5_H

#include <openssl/opensslconf.h> /* OPENSSL_NO_RC5 */

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef OPENSSL_NO_RC5
#error RC5 is disabled.
#endif

#define RC5_ENCRYPT	1
#define RC5_DECRYPT	0

/* 32 bit.  For Alpha, things may get weird */
#define RC5_32_INT unsigned long

#define RC5_32_BLOCK		8
#define RC5_32_KEY_LENGTH	16 /* This is a default, max is 255 */

/* This are the only values supported.  Tweak the code if you want more
 * The most supported modes will be
 * RC5-32/12/16
 * RC5-32/16/8
 */
#define RC5_8_ROUNDS	8
#define RC5_12_ROUNDS	12
#define RC5_16_ROUNDS	16

typedef struct rc5_key_st
	{
	/* Number of rounds */
	int rounds;
	RC5_32_INT data[2*(RC5_16_ROUNDS+1)];
	} RC5_32_KEY;

 
void RC5_32_set_key(RC5_32_KEY *key, int len, const unsigned char *data,
	int rounds);
void RC5_32_ecb_encrypt(const unsigned char *in,unsigned char *out,RC5_32_KEY *key,
	int enc);
void RC5_32_encrypt(unsigned long *data,RC5_32_KEY *key);
void RC5_32_decrypt(unsigned long *data,RC5_32_KEY *key);
void RC5_32_cbc_encrypt(const unsigned char *in, unsigned char *out,
			long length, RC5_32_KEY *ks, unsigned char *iv,
			int enc);
void RC5_32_cfb64_encrypt(const unsigned char *in, unsigned char *out,
			  long length, RC5_32_KEY *schedule,
			  unsigned char *ivec, int *num, int enc);
void RC5_32_ofb64_encrypt(const unsigned char *in, unsigned char *out,
			  long length, RC5_32_KEY *schedule,
			  unsigned char *ivec, int *num);

#ifdef  __cplusplus
}
#endif

#endif
