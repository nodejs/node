/* ====================================================================
 * Copyright (c) 2003 The OpenSSL Project.  All rights reserved.
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
 *
 */

#include <string.h>
#include <openssl/err.h>
#include <openssl/fips.h>
#include <openssl/evp.h>

#ifdef OPENSSL_FIPS
static struct
    {
    unsigned char key[16];
    unsigned char plaintext[16];
    unsigned char ciphertext[16];
    } tests[]=
	{
	{
	{ 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	  0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F },
	{ 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	  0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF },
	{ 0x69,0xC4,0xE0,0xD8,0x6A,0x7B,0x04,0x30,
	  0xD8,0xCD,0xB7,0x80,0x70,0xB4,0xC5,0x5A },
	},
	};

void FIPS_corrupt_aes()
    {
    tests[0].key[0]++;
    }

int FIPS_selftest_aes()
    {
    int n;
    int ret = 0;
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);

    for(n=0 ; n < 1 ; ++n)
	{
	if (fips_cipher_test(&ctx, EVP_aes_128_ecb(),
				tests[n].key, NULL,
				tests[n].plaintext,
				tests[n].ciphertext,
				16) <= 0)
		goto err;
	}
    ret = 1;
    err:
    EVP_CIPHER_CTX_cleanup(&ctx);
    if (ret == 0)
	    FIPSerr(FIPS_F_FIPS_SELFTEST_AES,FIPS_R_SELFTEST_FAILED);
    return ret;
    }
#endif
