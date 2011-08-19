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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "e_os.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif


#if defined(OPENSSL_NO_DSA) || !defined(OPENSSL_FIPS)
int main(int argc, char *argv[])
{
    printf("No FIPS DSA support\n");
    return(0);
}
#else
#include <openssl/dsa.h>
#include <openssl/fips.h>
#include <openssl/fips_rand.h>
#include <openssl/dsa.h>

#ifdef OPENSSL_SYS_WIN16
#define MS_CALLBACK     _far _loadds
#else
#define MS_CALLBACK
#endif

#include "fips_utl.h"

static int MS_CALLBACK dsa_cb(int p, int n, BN_GENCB *cb);

/* seed, out_p, out_q, out_g are taken from the earlier validation test
 * vectors.
 */

static unsigned char seed[20] = {
	0x1c, 0xfb, 0xa9, 0x6c, 0xf7, 0x95, 0xb3, 0x2e, 0x01, 0x01, 0x3c, 0x8d,
	0x7f, 0x6e, 0xf4, 0x59, 0xcc, 0x2f, 0x19, 0x59
  	};

static unsigned char out_p[] = {
	0xc2, 0x3c, 0x48, 0x31, 0x7e, 0x3b, 0x4e, 0x5d, 0x3c, 0x93, 0x78, 0x60,
	0x5c, 0xf2, 0x60, 0xbb, 0x5a, 0xfa, 0x7f, 0x17, 0xf9, 0x26, 0x69, 0x46,
	0xe7, 0x07, 0xbb, 0x3b, 0x2e, 0xc4, 0xb5, 0x66, 0xf7, 0x4d, 0xae, 0x9b,
	0x8f, 0xf0, 0x42, 0xea, 0xb3, 0xa0, 0x7e, 0x81, 0x85, 0x89, 0xe6, 0xb0,
	0x29, 0x03, 0x6b, 0xcc, 0xfb, 0x8e, 0x46, 0x15, 0x4d, 0xc1, 0x69, 0xd8,
	0x2f, 0xef, 0x5c, 0x8b, 0x29, 0x32, 0x41, 0xbd, 0x13, 0x72, 0x3d, 0xac,
	0x81, 0xcc, 0x86, 0x6c, 0x06, 0x5d, 0x51, 0xa1, 0xa5, 0x07, 0x0c, 0x3e,
	0xbe, 0xdd, 0xf4, 0x6e, 0xa8, 0xed, 0xb4, 0x2f, 0xbd, 0x3e, 0x64, 0xea,
	0xee, 0x92, 0xec, 0x51, 0xe1, 0x0d, 0xab, 0x25, 0x45, 0xae, 0x55, 0x21,
	0x4d, 0xd6, 0x96, 0x6f, 0xe6, 0xaa, 0xd3, 0xca, 0x87, 0x92, 0xb1, 0x1c,
	0x3c, 0xaf, 0x29, 0x09, 0x8b, 0xc6, 0xed, 0xe1
	};

static unsigned char out_q[] = {
	0xae, 0x0a, 0x8c, 0xfb, 0x80, 0xe1, 0xc6, 0xd1, 0x09, 0x0f, 0x26, 0xde,
	0x91, 0x53, 0xc2, 0x8b, 0x2b, 0x0f, 0xde, 0x7f
	};

static unsigned char out_g[] = {
	0x0d, 0x7d, 0x92, 0x74, 0x10, 0xf6, 0xa4, 0x43, 0x86, 0x9a, 0xd1, 0xd9,
	0x56, 0x00, 0xbc, 0x18, 0x97, 0x99, 0x4e, 0x9a, 0x93, 0xfb, 0x00, 0x3d,
	0x6c, 0xa0, 0x1b, 0x95, 0x6b, 0xbd, 0xf7, 0x7a, 0xbc, 0x36, 0x3f, 0x3d,
	0xb9, 0xbf, 0xf9, 0x91, 0x37, 0x68, 0xd1, 0xb9, 0x1e, 0xfe, 0x7f, 0x10,
	0xc0, 0x6a, 0xcd, 0x5f, 0xc1, 0x65, 0x1a, 0xb8, 0xe7, 0xab, 0xb5, 0xc6,
	0x8d, 0xb7, 0x86, 0xad, 0x3a, 0xbf, 0x6b, 0x7b, 0x0a, 0x66, 0xbe, 0xd5,
	0x58, 0x23, 0x16, 0x48, 0x83, 0x29, 0xb6, 0xa7, 0x64, 0xc7, 0x08, 0xbe,
	0x55, 0x4c, 0x6f, 0xcb, 0x34, 0xc1, 0x73, 0xb0, 0x39, 0x68, 0x52, 0xdf,
	0x27, 0x7f, 0x32, 0xbc, 0x2b, 0x0d, 0x63, 0xed, 0x75, 0x3e, 0xb5, 0x54,
	0xac, 0xc8, 0x20, 0x2a, 0x73, 0xe8, 0x29, 0x51, 0x03, 0x77, 0xe8, 0xc9,
	0x61, 0x32, 0x25, 0xaf, 0x21, 0x5b, 0x6e, 0xda
	};


static const unsigned char str1[]="12345678901234567890";

static const char rnd_seed[] = "string to make the random number generator think it has entropy";

int main(int argc, char **argv)
	{
	DSA *dsa=NULL;
	EVP_PKEY pk;
	int counter,ret=0,i,j;
	unsigned int slen;
	unsigned char buf[256];
	unsigned long h;
	BN_GENCB cb;
	EVP_MD_CTX mctx;
	BN_GENCB_set(&cb, dsa_cb, stderr);
	EVP_MD_CTX_init(&mctx);

	if(!FIPS_mode_set(1))
	    {
	    do_print_errors();
	    EXIT(1);
	    }

	fprintf(stderr,"test generation of DSA parameters\n");

	dsa = FIPS_dsa_new();
	DSA_generate_parameters_ex(dsa, 1024,seed,20,&counter,&h,&cb);

	fprintf(stderr,"seed\n");
	for (i=0; i<20; i+=4)
		{
		fprintf(stderr,"%02X%02X%02X%02X ",
			seed[i],seed[i+1],seed[i+2],seed[i+3]);
		}
	fprintf(stderr,"\ncounter=%d h=%ld\n",counter,h);

	if (dsa == NULL) goto end;
	if (counter != 16) 
		{
		fprintf(stderr,"counter should be 105\n");
		goto end;
		}
	if (h != 2)
		{
		fprintf(stderr,"h should be 2\n");
		goto end;
		}

	i=BN_bn2bin(dsa->q,buf);
	j=sizeof(out_q);
	if ((i != j) || (memcmp(buf,out_q,i) != 0))
		{
		fprintf(stderr,"q value is wrong\n");
		goto end;
		}

	i=BN_bn2bin(dsa->p,buf);
	j=sizeof(out_p);
	if ((i != j) || (memcmp(buf,out_p,i) != 0))
		{
		fprintf(stderr,"p value is wrong\n");
		goto end;
		}

	i=BN_bn2bin(dsa->g,buf);
	j=sizeof(out_g);
	if ((i != j) || (memcmp(buf,out_g,i) != 0))
		{
		fprintf(stderr,"g value is wrong\n");
		goto end;
		}
	DSA_generate_key(dsa);
	pk.type = EVP_PKEY_DSA;
	pk.pkey.dsa = dsa;

	if (!EVP_SignInit_ex(&mctx, EVP_dss1(), NULL))
		goto end;
	if (!EVP_SignUpdate(&mctx, str1, 20))
		goto end;
	if (!EVP_SignFinal(&mctx, buf, &slen, &pk))
		goto end;

	if (!EVP_VerifyInit_ex(&mctx, EVP_dss1(), NULL))
		goto end;
	if (!EVP_VerifyUpdate(&mctx, str1, 20))
		goto end;
	if (EVP_VerifyFinal(&mctx, buf, slen, &pk) != 1)
		goto end;

	ret = 1;

end:
	if (!ret)
		do_print_errors();
	if (dsa != NULL) FIPS_dsa_free(dsa);
	EVP_MD_CTX_cleanup(&mctx);
#if 0
	CRYPTO_mem_leaks(bio_err);
#endif
	EXIT(!ret);
	return(!ret);
	}

static int cb_exit(int ec)
	{
	EXIT(ec);
	return(0);		/* To keep some compilers quiet */
	}

static int MS_CALLBACK dsa_cb(int p, int n, BN_GENCB *cb)
	{
	char c='*';
	static int ok=0,num=0;

	if (p == 0) { c='.'; num++; };
	if (p == 1) c='+';
	if (p == 2) { c='*'; ok++; }
	if (p == 3) c='\n';
	fwrite(&c,1, 1, cb->arg);
	fflush(cb->arg);

	if (!ok && (p == 0) && (num > 1))
		{
		fprintf(cb->arg,"error in dsatest\n");
		cb_exit(1);
		}
	return 1;
	}
#endif
