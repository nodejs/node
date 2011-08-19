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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/rand.h>
#include <openssl/fips_rand.h>
#include <openssl/err.h>
#include <openssl/bn.h>

#include "e_os.h"

#ifndef OPENSSL_FIPS
int main(int argc, char *argv[])
{
    printf("No FIPS RAND support\n");
    return(0);
}

#else

#include "fips_utl.h"

typedef struct
	{
	unsigned char DT[16];
	unsigned char V[16];
	unsigned char R[16];
	} AES_PRNG_MCT;

static unsigned char aes_128_mct_key[16] =
	{0x9f,0x5b,0x51,0x20,0x0b,0xf3,0x34,0xb5,
	 0xd8,0x2b,0xe8,0xc3,0x72,0x55,0xc8,0x48};

static AES_PRNG_MCT aes_128_mct_tv = {
			/* DT */
	{0x63,0x76,0xbb,0xe5,0x29,0x02,0xba,0x3b,
	 0x67,0xc9,0x25,0xfa,0x70,0x1f,0x11,0xac},
			/* V */
	{0x57,0x2c,0x8e,0x76,0x87,0x26,0x47,0x97,
	 0x7e,0x74,0xfb,0xdd,0xc4,0x95,0x01,0xd1},
			/* R */
	{0x48,0xe9,0xbd,0x0d,0x06,0xee,0x18,0xfb,
	 0xe4,0x57,0x90,0xd5,0xc3,0xfc,0x9b,0x73}
};

static unsigned char aes_192_mct_key[24] =
	{0xb7,0x6c,0x34,0xd1,0x09,0x67,0xab,0x73,
	 0x4d,0x5a,0xd5,0x34,0x98,0x16,0x0b,0x91,
	 0xbc,0x35,0x51,0x16,0x6b,0xae,0x93,0x8a};

static AES_PRNG_MCT aes_192_mct_tv = {
			/* DT */
	{0x84,0xce,0x22,0x7d,0x91,0x5a,0xa3,0xc9,
	 0x84,0x3c,0x0a,0xb3,0xa9,0x63,0x15,0x52},
			/* V */
	{0xb6,0xaf,0xe6,0x8f,0x99,0x9e,0x90,0x64,
	 0xdd,0xc7,0x7a,0xc1,0xbb,0x90,0x3a,0x6d},
			/* R */
	{0xfc,0x85,0x60,0x9a,0x29,0x6f,0xef,0x21,
	 0xdd,0x86,0x20,0x32,0x8a,0x29,0x6f,0x47}
};

static unsigned char aes_256_mct_key[32] =
	{0x9b,0x05,0xc8,0x68,0xff,0x47,0xf8,0x3a,
	 0xa6,0x3a,0xa8,0xcb,0x4e,0x71,0xb2,0xe0,
	 0xb8,0x7e,0xf1,0x37,0xb6,0xb4,0xf6,0x6d,
	 0x86,0x32,0xfc,0x1f,0x5e,0x1d,0x1e,0x50};

static AES_PRNG_MCT aes_256_mct_tv = {
			/* DT */
	{0x31,0x6e,0x35,0x9a,0xb1,0x44,0xf0,0xee,
	 0x62,0x6d,0x04,0x46,0xe0,0xa3,0x92,0x4c},
			/* V */
	{0x4f,0xcd,0xc1,0x87,0x82,0x1f,0x4d,0xa1,
	 0x3e,0x0e,0x56,0x44,0x59,0xe8,0x83,0xca},
			/* R */
	{0xc8,0x87,0xc2,0x61,0x5b,0xd0,0xb9,0xe1,
	 0xe7,0xf3,0x8b,0xd7,0x5b,0xd5,0xf1,0x8d}
};

static void dump(const unsigned char *b,int n)
    {
    while(n-- > 0)
	{
	printf(" %02x",*b++);
	}
    }

static void compare(const unsigned char *result,const unsigned char *expected,
		    int n)
    {
    int i;

    for(i=0 ; i < n ; ++i)
	if(result[i] != expected[i])
	    {
	    puts("Random test failed, got:");
	    dump(result,n);
	    puts("\n               expected:");
	    dump(expected,n);
	    putchar('\n');
	    EXIT(1);
	    }
    }


static void run_test(unsigned char *key, int keylen, AES_PRNG_MCT *tv)
    {
    unsigned char buf[16], dt[16];
    int i, j;
    FIPS_rand_reset();
    FIPS_rand_test_mode();
    FIPS_rand_set_key(key, keylen);
    FIPS_rand_seed(tv->V, 16);
    memcpy(dt, tv->DT, 16);
    for (i = 0; i < 10000; i++)
	{
    	FIPS_rand_set_dt(dt);
	FIPS_rand_bytes(buf, 16);
	/* Increment DT */
	for (j = 15; j >= 0; j--)
		{
		dt[j]++;
		if (dt[j])
			break;
		}
	}

    compare(buf,tv->R, 16);
    }

int main()
	{
	run_test(aes_128_mct_key, 16, &aes_128_mct_tv);
	printf("FIPS PRNG test 1 done\n");
	run_test(aes_192_mct_key, 24, &aes_192_mct_tv);
	printf("FIPS PRNG test 2 done\n");
	run_test(aes_256_mct_key, 32, &aes_256_mct_tv);
	printf("FIPS PRNG test 3 done\n");
	return 0;
	}

#endif
