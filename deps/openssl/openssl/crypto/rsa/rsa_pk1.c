/* crypto/rsa/rsa_pk1.c */
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

#include "constant_time_locl.h"

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>

int RSA_padding_add_PKCS1_type_1(unsigned char *to, int tlen,
	     const unsigned char *from, int flen)
	{
	int j;
	unsigned char *p;

	if (flen > (tlen-RSA_PKCS1_PADDING_SIZE))
		{
		RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_1,RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
		return(0);
		}
	
	p=(unsigned char *)to;

	*(p++)=0;
	*(p++)=1; /* Private Key BT (Block Type) */

	/* pad out with 0xff data */
	j=tlen-3-flen;
	memset(p,0xff,j);
	p+=j;
	*(p++)='\0';
	memcpy(p,from,(unsigned int)flen);
	return(1);
	}

int RSA_padding_check_PKCS1_type_1(unsigned char *to, int tlen,
	     const unsigned char *from, int flen, int num)
	{
	int i,j;
	const unsigned char *p;

	p=from;
	if ((num != (flen+1)) || (*(p++) != 01))
		{
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BLOCK_TYPE_IS_NOT_01);
		return(-1);
		}

	/* scan over padding data */
	j=flen-1; /* one for type. */
	for (i=0; i<j; i++)
		{
		if (*p != 0xff) /* should decrypt to 0xff */
			{
			if (*p == 0)
				{ p++; break; }
			else	{
				RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BAD_FIXED_HEADER_DECRYPT);
				return(-1);
				}
			}
		p++;
		}

	if (i == j)
		{
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_NULL_BEFORE_BLOCK_MISSING);
		return(-1);
		}

	if (i < 8)
		{
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BAD_PAD_BYTE_COUNT);
		return(-1);
		}
	i++; /* Skip over the '\0' */
	j-=i;
	if (j > tlen)
		{
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_DATA_TOO_LARGE);
		return(-1);
		}
	memcpy(to,p,(unsigned int)j);

	return(j);
	}

int RSA_padding_add_PKCS1_type_2(unsigned char *to, int tlen,
	     const unsigned char *from, int flen)
	{
	int i,j;
	unsigned char *p;
	
	if (flen > (tlen-11))
		{
		RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_2,RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
		return(0);
		}
	
	p=(unsigned char *)to;

	*(p++)=0;
	*(p++)=2; /* Public Key BT (Block Type) */

	/* pad out with non-zero random data */
	j=tlen-3-flen;

	if (RAND_bytes(p,j) <= 0)
		return(0);
	for (i=0; i<j; i++)
		{
		if (*p == '\0')
			do	{
				if (RAND_bytes(p,1) <= 0)
					return(0);
				} while (*p == '\0');
		p++;
		}

	*(p++)='\0';

	memcpy(p,from,(unsigned int)flen);
	return(1);
	}

int RSA_padding_check_PKCS1_type_2(unsigned char *to, int tlen,
	     const unsigned char *from, int flen, int num)
	{
	int i;
	/* |em| is the encoded message, zero-padded to exactly |num| bytes */
	unsigned char *em = NULL;
	unsigned int good, found_zero_byte;
	int zero_index = 0, msg_index, mlen = -1;

        if (tlen < 0 || flen < 0)
		return -1;

	/* PKCS#1 v1.5 decryption. See "PKCS #1 v2.2: RSA Cryptography
	 * Standard", section 7.2.2. */

	if (flen > num)
		goto err;

	if (num < 11)
		goto err;

	em = OPENSSL_malloc(num);
	if (em == NULL)
		{
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2, ERR_R_MALLOC_FAILURE);
		return -1;
		}
	memset(em, 0, num);
	/*
	 * Always do this zero-padding copy (even when num == flen) to avoid
	 * leaking that information. The copy still leaks some side-channel
	 * information, but it's impossible to have a fixed  memory access
	 * pattern since we can't read out of the bounds of |from|.
	 *
	 * TODO(emilia): Consider porting BN_bn2bin_padded from BoringSSL.
	 */
	memcpy(em + num - flen, from, flen);

	good = constant_time_is_zero(em[0]);
	good &= constant_time_eq(em[1], 2);

	found_zero_byte = 0;
	for (i = 2; i < num; i++)
		{
		unsigned int equals0 = constant_time_is_zero(em[i]);
		zero_index = constant_time_select_int(~found_zero_byte & equals0, i, zero_index);
		found_zero_byte |= equals0;
		}

	/*
	 * PS must be at least 8 bytes long, and it starts two bytes into |em|.
         * If we never found a 0-byte, then |zero_index| is 0 and the check
	 * also fails.
	 */
	good &= constant_time_ge((unsigned int)(zero_index), 2 + 8);

	/* Skip the zero byte. This is incorrect if we never found a zero-byte
	 * but in this case we also do not copy the message out. */
	msg_index = zero_index + 1;
	mlen = num - msg_index;

	/* For good measure, do this check in constant time as well; it could
	 * leak something if |tlen| was assuming valid padding. */
	good &= constant_time_ge((unsigned int)(tlen), (unsigned int)(mlen));

	/*
	 * We can't continue in constant-time because we need to copy the result
	 * and we cannot fake its length. This unavoidably leaks timing
	 * information at the API boundary.
	 * TODO(emilia): this could be addressed at the call site,
	 * see BoringSSL commit 0aa0767340baf925bda4804882aab0cb974b2d26.
	 */
	if (!good)
		{
		mlen = -1;
		goto err;
		}

	memcpy(to, em + msg_index, mlen);

err:
	if (em != NULL)
		OPENSSL_free(em);
	if (mlen == -1)
		RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2, RSA_R_PKCS_DECODING_ERROR);
	return mlen;
	}
