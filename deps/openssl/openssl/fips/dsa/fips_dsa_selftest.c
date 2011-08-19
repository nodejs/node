/* crypto/dsa/dsatest.c */
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

#include <string.h>
#include <openssl/crypto.h>
#include <openssl/dsa.h>
#include <openssl/fips.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#ifdef OPENSSL_FIPS

/* seed, out_p, out_q, out_g are taken the NIST test vectors */

static unsigned char seed[20] = {
	0x77, 0x8f, 0x40, 0x74, 0x6f, 0x66, 0xbe, 0x33, 0xce, 0xbe, 0x99, 0x34,
	0x4c, 0xfc, 0xf3, 0x28, 0xaa, 0x70, 0x2d, 0x3a
  	};

static unsigned char out_p[] = {
	0xf7, 0x7c, 0x1b, 0x83, 0xd8, 0xe8, 0x5c, 0x7f, 0x85, 0x30, 0x17, 0x57,
	0x21, 0x95, 0xfe, 0x26, 0x04, 0xeb, 0x47, 0x4c, 0x3a, 0x4a, 0x81, 0x4b,
	0x71, 0x2e, 0xed, 0x6e, 0x4f, 0x3d, 0x11, 0x0f, 0x7c, 0xfe, 0x36, 0x43,
	0x51, 0xd9, 0x81, 0x39, 0x17, 0xdf, 0x62, 0xf6, 0x9c, 0x01, 0xa8, 0x69,
	0x71, 0xdd, 0x29, 0x7f, 0x47, 0xe6, 0x65, 0xa6, 0x22, 0xe8, 0x6a, 0x12,
	0x2b, 0xc2, 0x81, 0xff, 0x32, 0x70, 0x2f, 0x9e, 0xca, 0x53, 0x26, 0x47,
	0x0f, 0x59, 0xd7, 0x9e, 0x2c, 0xa5, 0x07, 0xc4, 0x49, 0x52, 0xa3, 0xe4,
	0x6b, 0x04, 0x00, 0x25, 0x49, 0xe2, 0xe6, 0x7f, 0x28, 0x78, 0x97, 0xb8,
	0x3a, 0x32, 0x14, 0x38, 0xa2, 0x51, 0x33, 0x22, 0x44, 0x7e, 0xd7, 0xef,
	0x45, 0xdb, 0x06, 0x4a, 0xd2, 0x82, 0x4a, 0x82, 0x2c, 0xb1, 0xd7, 0xd8,
	0xb6, 0x73, 0x00, 0x4d, 0x94, 0x77, 0x94, 0xef
	};

static unsigned char out_q[] = {
	0xd4, 0x0a, 0xac, 0x9f, 0xbd, 0x8c, 0x80, 0xc2, 0x38, 0x7e, 0x2e, 0x0c,
	0x52, 0x5c, 0xea, 0x34, 0xa1, 0x83, 0x32, 0xf3
	};

static unsigned char out_g[] = {
	0x34, 0x73, 0x8b, 0x57, 0x84, 0x8e, 0x55, 0xbf, 0x57, 0xcc, 0x41, 0xbb,
	0x5e, 0x2b, 0xd5, 0x42, 0xdd, 0x24, 0x22, 0x2a, 0x09, 0xea, 0x26, 0x1e,
	0x17, 0x65, 0xcb, 0x1a, 0xb3, 0x12, 0x44, 0xa3, 0x9e, 0x99, 0xe9, 0x63,
	0xeb, 0x30, 0xb1, 0x78, 0x7b, 0x09, 0x40, 0x30, 0xfa, 0x83, 0xc2, 0x35,
	0xe1, 0xc4, 0x2d, 0x74, 0x1a, 0xb1, 0x83, 0x54, 0xd8, 0x29, 0xf4, 0xcf,
	0x7f, 0x6f, 0x67, 0x1c, 0x36, 0x49, 0xee, 0x6c, 0xa2, 0x3c, 0x2d, 0x6a,
	0xe9, 0xd3, 0x9a, 0xf6, 0x57, 0x78, 0x6f, 0xfd, 0x33, 0xcd, 0x3c, 0xed,
	0xfd, 0xd4, 0x41, 0xe6, 0x5c, 0x8b, 0xe0, 0x68, 0x31, 0x47, 0x47, 0xaf,
	0x12, 0xa7, 0xf9, 0x32, 0x0d, 0x94, 0x15, 0x48, 0xd0, 0x54, 0x85, 0xb2,
	0x04, 0xb5, 0x4d, 0xd4, 0x9d, 0x05, 0x22, 0x25, 0xd9, 0xfd, 0x6c, 0x36,
	0xef, 0xbe, 0x69, 0x6c, 0x55, 0xf4, 0xee, 0xec
	};

static const unsigned char str1[]="12345678901234567890";

void FIPS_corrupt_dsa()
    {
    ++seed[0];
    }

int FIPS_selftest_dsa()
    {
    DSA *dsa=NULL;
    int counter,i,j, ret = 0;
    unsigned int slen;
    unsigned char buf[256];
    unsigned long h;
    EVP_MD_CTX mctx;
    EVP_PKEY pk;

    EVP_MD_CTX_init(&mctx);

    dsa = FIPS_dsa_new();

    if(dsa == NULL)
	goto err;
    if(!DSA_generate_parameters_ex(dsa, 1024,seed,20,&counter,&h,NULL))
	goto err;
    if (counter != 378) 
	goto err;
    if (h != 2)
	goto err;
    i=BN_bn2bin(dsa->q,buf);
    j=sizeof(out_q);
    if (i != j || memcmp(buf,out_q,i) != 0)
	goto err;

    i=BN_bn2bin(dsa->p,buf);
    j=sizeof(out_p);
    if (i != j || memcmp(buf,out_p,i) != 0)
	goto err;

    i=BN_bn2bin(dsa->g,buf);
    j=sizeof(out_g);
    if (i != j || memcmp(buf,out_g,i) != 0)
	goto err;
    DSA_generate_key(dsa);
    pk.type = EVP_PKEY_DSA;
    pk.pkey.dsa = dsa;

    if (!EVP_SignInit_ex(&mctx, EVP_dss1(), NULL))
	goto err;
    if (!EVP_SignUpdate(&mctx, str1, 20))
	goto err;
    if (!EVP_SignFinal(&mctx, buf, &slen, &pk))
	goto err;

    if (!EVP_VerifyInit_ex(&mctx, EVP_dss1(), NULL))
	goto err;
    if (!EVP_VerifyUpdate(&mctx, str1, 20))
	goto err;
    if (EVP_VerifyFinal(&mctx, buf, slen, &pk) != 1)
	goto err;

    ret = 1;

    err:
    EVP_MD_CTX_cleanup(&mctx);
    if (dsa)
	FIPS_dsa_free(dsa);
    if (ret == 0)
	    FIPSerr(FIPS_F_FIPS_SELFTEST_DSA,FIPS_R_SELFTEST_FAILED);
    return ret;
    }
#endif
