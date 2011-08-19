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
#include <openssl/opensslconf.h>

#ifdef OPENSSL_FIPS

static struct
    {
    unsigned char key[16];
    unsigned char plaintext[8];
    unsigned char ciphertext[8];
    } tests2[]=
	{
	{
	{ 0x7c,0x4f,0x6e,0xf7,0xa2,0x04,0x16,0xec,
	  0x0b,0x6b,0x7c,0x9e,0x5e,0x19,0xa7,0xc4 },
	{ 0x06,0xa7,0xd8,0x79,0xaa,0xce,0x69,0xef },
	{ 0x4c,0x11,0x17,0x55,0xbf,0xc4,0x4e,0xfd }
	},
	{
	{ 0x5d,0x9e,0x01,0xd3,0x25,0xc7,0x3e,0x34,
	  0x01,0x16,0x7c,0x85,0x23,0xdf,0xe0,0x68 },
	{ 0x9c,0x50,0x09,0x0f,0x5e,0x7d,0x69,0x7e },
	{ 0xd2,0x0b,0x18,0xdf,0xd9,0x0d,0x9e,0xff },
	}
	};

static struct
    {
    unsigned char key[24];
    unsigned char plaintext[8];
    unsigned char ciphertext[8];
    } tests3[]=
	{
	{
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	  0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,
	  0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0 },
	{ 0x8f,0x8f,0xbf,0x9b,0x5d,0x48,0xb4,0x1c },
	{ 0x59,0x8c,0xe5,0xd3,0x6c,0xa2,0xea,0x1b },
	},
	{
	{ 0xDC,0xBA,0x98,0x76,0x54,0x32,0x10,0xFE,
	  0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
	  0xED,0x39,0xD9,0x50,0xFA,0x74,0xBC,0xC4 },
	{ 0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF },
	{ 0x11,0x25,0xb0,0x35,0xbe,0xa0,0x82,0x86 },
	},
	};

void FIPS_corrupt_des()
    {
    tests2[0].plaintext[0]++;
    }

int FIPS_selftest_des()
    {
    int n, ret = 0;
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);
    /* Encrypt/decrypt with 2-key 3DES and compare to known answers */
    for(n=0 ; n < 2 ; ++n)
	{
	if (!fips_cipher_test(&ctx, EVP_des_ede_ecb(),
				tests2[n].key, NULL,
				tests2[n].plaintext, tests2[n].ciphertext, 8))
		goto err;
	}

    /* Encrypt/decrypt with 3DES and compare to known answers */
    for(n=0 ; n < 2 ; ++n)
	{
	if (!fips_cipher_test(&ctx, EVP_des_ede3_ecb(),
				tests3[n].key, NULL,
				tests3[n].plaintext, tests3[n].ciphertext, 8))
		goto err;
	}
    ret = 1;
    err:
    EVP_CIPHER_CTX_cleanup(&ctx);
    if (ret == 0)
	    FIPSerr(FIPS_F_FIPS_SELFTEST_DES,FIPS_R_SELFTEST_FAILED);

    return ret;
    }
#endif
