/* crypto/bn/exptest.c */
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

#include "../e_os.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define NUM_BITS	(BN_BITS*2)

static const char rnd_seed[] = "string to make the random number generator think it has entropy";

/* test_exp_mod_zero tests that x**0 mod 1 == 0. It returns zero on success. */
static int test_exp_mod_zero() {
	BIGNUM a, p, m;
	BIGNUM r;
	BN_CTX *ctx = BN_CTX_new();
	int ret = 1;

	BN_init(&m);
	BN_one(&m);

	BN_init(&a);
	BN_one(&a);

	BN_init(&p);
	BN_zero(&p);

	BN_init(&r);
	BN_mod_exp(&r, &a, &p, &m, ctx);
	BN_CTX_free(ctx);

	if (BN_is_zero(&r))
		ret = 0;
	else
		{
		printf("1**0 mod 1 = ");
		BN_print_fp(stdout, &r);
		printf(", should be 0\n");
		}

	BN_free(&r);
	BN_free(&a);
	BN_free(&p);
	BN_free(&m);

	return ret;
}

int main(int argc, char *argv[])
	{
	BN_CTX *ctx;
	BIO *out=NULL;
	int i,ret;
	unsigned char c;
	BIGNUM *r_mont,*r_mont_const,*r_recp,*r_simple,*a,*b,*m;

	RAND_seed(rnd_seed, sizeof rnd_seed); /* or BN_rand may fail, and we don't
	                                       * even check its return value
	                                       * (which we should) */

	ERR_load_BN_strings();

	ctx=BN_CTX_new();
	if (ctx == NULL) EXIT(1);
	r_mont=BN_new();
	r_mont_const=BN_new();
	r_recp=BN_new();
	r_simple=BN_new();
	a=BN_new();
	b=BN_new();
	m=BN_new();
	if (	(r_mont == NULL) || (r_recp == NULL) ||
		(a == NULL) || (b == NULL))
		goto err;

	out=BIO_new(BIO_s_file());

	if (out == NULL) EXIT(1);
	BIO_set_fp(out,stdout,BIO_NOCLOSE);

	for (i=0; i<200; i++)
		{
		RAND_bytes(&c,1);
		c=(c%BN_BITS)-BN_BITS2;
		BN_rand(a,NUM_BITS+c,0,0);

		RAND_bytes(&c,1);
		c=(c%BN_BITS)-BN_BITS2;
		BN_rand(b,NUM_BITS+c,0,0);

		RAND_bytes(&c,1);
		c=(c%BN_BITS)-BN_BITS2;
		BN_rand(m,NUM_BITS+c,0,1);

		BN_mod(a,a,m,ctx);
		BN_mod(b,b,m,ctx);

		ret=BN_mod_exp_mont(r_mont,a,b,m,ctx,NULL);
		if (ret <= 0)
			{
			printf("BN_mod_exp_mont() problems\n");
			ERR_print_errors(out);
			EXIT(1);
			}

		ret=BN_mod_exp_recp(r_recp,a,b,m,ctx);
		if (ret <= 0)
			{
			printf("BN_mod_exp_recp() problems\n");
			ERR_print_errors(out);
			EXIT(1);
			}

		ret=BN_mod_exp_simple(r_simple,a,b,m,ctx);
		if (ret <= 0)
			{
			printf("BN_mod_exp_simple() problems\n");
			ERR_print_errors(out);
			EXIT(1);
			}

		ret=BN_mod_exp_mont_consttime(r_mont_const,a,b,m,ctx,NULL);
		if (ret <= 0)
			{
			printf("BN_mod_exp_mont_consttime() problems\n");
			ERR_print_errors(out);
			EXIT(1);
			}

		if (BN_cmp(r_simple, r_mont) == 0
		    && BN_cmp(r_simple,r_recp) == 0
			&& BN_cmp(r_simple,r_mont_const) == 0)
			{
			printf(".");
			fflush(stdout);
			}
		else
		  	{
			if (BN_cmp(r_simple,r_mont) != 0)
				printf("\nsimple and mont results differ\n");
			if (BN_cmp(r_simple,r_mont_const) != 0)
				printf("\nsimple and mont const time results differ\n");
			if (BN_cmp(r_simple,r_recp) != 0)
				printf("\nsimple and recp results differ\n");

			printf("a (%3d) = ",BN_num_bits(a));   BN_print(out,a);
			printf("\nb (%3d) = ",BN_num_bits(b)); BN_print(out,b);
			printf("\nm (%3d) = ",BN_num_bits(m)); BN_print(out,m);
			printf("\nsimple   =");	BN_print(out,r_simple);
			printf("\nrecp     =");	BN_print(out,r_recp);
			printf("\nmont     ="); BN_print(out,r_mont);
			printf("\nmont_ct  ="); BN_print(out,r_mont_const);
			printf("\n");
			EXIT(1);
			}
		}
	BN_free(r_mont);
	BN_free(r_mont_const);
	BN_free(r_recp);
	BN_free(r_simple);
	BN_free(a);
	BN_free(b);
	BN_free(m);
	BN_CTX_free(ctx);
	ERR_remove_thread_state(NULL);
	CRYPTO_mem_leaks(out);
	BIO_free(out);
	printf("\n");

	if (test_exp_mod_zero() != 0)
		goto err;

	printf("done\n");

	EXIT(0);
err:
	ERR_load_crypto_strings();
	ERR_print_errors(out);
#ifdef OPENSSL_SYS_NETWARE
    printf("ERROR\n");
#endif
	EXIT(1);
	return(1);
	}

