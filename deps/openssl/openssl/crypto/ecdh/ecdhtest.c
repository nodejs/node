/* crypto/ecdh/ecdhtest.c */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The ECDH software is originally written by Douglas Stebila of
 * Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../e_os.h"

#include <openssl/opensslconf.h>	/* for OPENSSL_NO_ECDH */
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#ifdef OPENSSL_NO_ECDH
int main(int argc, char *argv[])
{
    printf("No ECDH support\n");
    return(0);
}
#else
#include <openssl/ec.h>
#include <openssl/ecdh.h>

#ifdef OPENSSL_SYS_WIN16
#define MS_CALLBACK	_far _loadds
#else
#define MS_CALLBACK
#endif

#if 0
static void MS_CALLBACK cb(int p, int n, void *arg);
#endif

static const char rnd_seed[] = "string to make the random number generator think it has entropy";


static const int KDF1_SHA1_len = 20;
static void *KDF1_SHA1(const void *in, size_t inlen, void *out, size_t *outlen)
	{
#ifndef OPENSSL_NO_SHA
	if (*outlen < SHA_DIGEST_LENGTH)
		return NULL;
	else
		*outlen = SHA_DIGEST_LENGTH;
	return SHA1(in, inlen, out);
#else
	return NULL;
#endif
	}


static int test_ecdh_curve(int nid, const char *text, BN_CTX *ctx, BIO *out)
	{
	EC_KEY *a=NULL;
	EC_KEY *b=NULL;
	BIGNUM *x_a=NULL, *y_a=NULL,
	       *x_b=NULL, *y_b=NULL;
	char buf[12];
	unsigned char *abuf=NULL,*bbuf=NULL;
	int i,alen,blen,aout,bout,ret=0;
	const EC_GROUP *group;

	a = EC_KEY_new_by_curve_name(nid);
	b = EC_KEY_new_by_curve_name(nid);
	if (a == NULL || b == NULL)
		goto err;

	group = EC_KEY_get0_group(a);

	if ((x_a=BN_new()) == NULL) goto err;
	if ((y_a=BN_new()) == NULL) goto err;
	if ((x_b=BN_new()) == NULL) goto err;
	if ((y_b=BN_new()) == NULL) goto err;

	BIO_puts(out,"Testing key generation with ");
	BIO_puts(out,text);
#ifdef NOISY
	BIO_puts(out,"\n");
#else
	(void)BIO_flush(out);
#endif

	if (!EC_KEY_generate_key(a)) goto err;
	
	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field) 
		{
		if (!EC_POINT_get_affine_coordinates_GFp(group,
			EC_KEY_get0_public_key(a), x_a, y_a, ctx)) goto err;
		}
	else
		{
		if (!EC_POINT_get_affine_coordinates_GF2m(group,
			EC_KEY_get0_public_key(a), x_a, y_a, ctx)) goto err;
		}
#ifdef NOISY
	BIO_puts(out,"  pri 1=");
	BN_print(out,a->priv_key);
	BIO_puts(out,"\n  pub 1=");
	BN_print(out,x_a);
	BIO_puts(out,",");
	BN_print(out,y_a);
	BIO_puts(out,"\n");
#else
	BIO_printf(out," .");
	(void)BIO_flush(out);
#endif

	if (!EC_KEY_generate_key(b)) goto err;

	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field) 
		{
		if (!EC_POINT_get_affine_coordinates_GFp(group, 
			EC_KEY_get0_public_key(b), x_b, y_b, ctx)) goto err;
		}
	else
		{
		if (!EC_POINT_get_affine_coordinates_GF2m(group, 
			EC_KEY_get0_public_key(b), x_b, y_b, ctx)) goto err;
		}

#ifdef NOISY
	BIO_puts(out,"  pri 2=");
	BN_print(out,b->priv_key);
	BIO_puts(out,"\n  pub 2=");
	BN_print(out,x_b);
	BIO_puts(out,",");
	BN_print(out,y_b);
	BIO_puts(out,"\n");
#else
	BIO_printf(out,".");
	(void)BIO_flush(out);
#endif

	alen=KDF1_SHA1_len;
	abuf=(unsigned char *)OPENSSL_malloc(alen);
	aout=ECDH_compute_key(abuf,alen,EC_KEY_get0_public_key(b),a,KDF1_SHA1);

#ifdef NOISY
	BIO_puts(out,"  key1 =");
	for (i=0; i<aout; i++)
		{
		sprintf(buf,"%02X",abuf[i]);
		BIO_puts(out,buf);
		}
	BIO_puts(out,"\n");
#else
	BIO_printf(out,".");
	(void)BIO_flush(out);
#endif

	blen=KDF1_SHA1_len;
	bbuf=(unsigned char *)OPENSSL_malloc(blen);
	bout=ECDH_compute_key(bbuf,blen,EC_KEY_get0_public_key(a),b,KDF1_SHA1);

#ifdef NOISY
	BIO_puts(out,"  key2 =");
	for (i=0; i<bout; i++)
		{
		sprintf(buf,"%02X",bbuf[i]);
		BIO_puts(out,buf);
		}
	BIO_puts(out,"\n");
#else
	BIO_printf(out,".");
	(void)BIO_flush(out);
#endif

	if ((aout < 4) || (bout != aout) || (memcmp(abuf,bbuf,aout) != 0))
		{
#ifndef NOISY
		BIO_printf(out, " failed\n\n");
		BIO_printf(out, "key a:\n");
		BIO_printf(out, "private key: ");
		BN_print(out, EC_KEY_get0_private_key(a));
		BIO_printf(out, "\n");
		BIO_printf(out, "public key (x,y): ");
		BN_print(out, x_a);
		BIO_printf(out, ",");
		BN_print(out, y_a);
		BIO_printf(out, "\nkey b:\n");
		BIO_printf(out, "private key: ");
		BN_print(out, EC_KEY_get0_private_key(b));
		BIO_printf(out, "\n");
		BIO_printf(out, "public key (x,y): ");
		BN_print(out, x_b);
		BIO_printf(out, ",");
		BN_print(out, y_b);
		BIO_printf(out, "\n");
		BIO_printf(out, "generated key a: ");
		for (i=0; i<bout; i++)
			{
			sprintf(buf, "%02X", bbuf[i]);
			BIO_puts(out, buf);
			}
		BIO_printf(out, "\n");
		BIO_printf(out, "generated key b: ");
		for (i=0; i<aout; i++)
			{
			sprintf(buf, "%02X", abuf[i]);
			BIO_puts(out,buf);
			}
		BIO_printf(out, "\n");
#endif
		fprintf(stderr,"Error in ECDH routines\n");
		ret=0;
		}
	else
		{
#ifndef NOISY
		BIO_printf(out, " ok\n");
#endif
		ret=1;
		}
err:
	ERR_print_errors_fp(stderr);

	if (abuf != NULL) OPENSSL_free(abuf);
	if (bbuf != NULL) OPENSSL_free(bbuf);
	if (x_a) BN_free(x_a);
	if (y_a) BN_free(y_a);
	if (x_b) BN_free(x_b);
	if (y_b) BN_free(y_b);
	if (b) EC_KEY_free(b);
	if (a) EC_KEY_free(a);
	return(ret);
	}

int main(int argc, char *argv[])
	{
	BN_CTX *ctx=NULL;
	int ret=1;
	BIO *out;

	CRYPTO_malloc_debug_init();
	CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

#ifdef OPENSSL_SYS_WIN32
	CRYPTO_malloc_init();
#endif

	RAND_seed(rnd_seed, sizeof rnd_seed);

	out=BIO_new(BIO_s_file());
	if (out == NULL) EXIT(1);
	BIO_set_fp(out,stdout,BIO_NOCLOSE);

	if ((ctx=BN_CTX_new()) == NULL) goto err;

	/* NIST PRIME CURVES TESTS */
	if (!test_ecdh_curve(NID_X9_62_prime192v1, "NIST Prime-Curve P-192", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_secp224r1, "NIST Prime-Curve P-224", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_X9_62_prime256v1, "NIST Prime-Curve P-256", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_secp384r1, "NIST Prime-Curve P-384", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_secp521r1, "NIST Prime-Curve P-521", ctx, out)) goto err;
	/* NIST BINARY CURVES TESTS */
	if (!test_ecdh_curve(NID_sect163k1, "NIST Binary-Curve K-163", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect163r2, "NIST Binary-Curve B-163", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect233k1, "NIST Binary-Curve K-233", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect233r1, "NIST Binary-Curve B-233", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect283k1, "NIST Binary-Curve K-283", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect283r1, "NIST Binary-Curve B-283", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect409k1, "NIST Binary-Curve K-409", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect409r1, "NIST Binary-Curve B-409", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect571k1, "NIST Binary-Curve K-571", ctx, out)) goto err;
	if (!test_ecdh_curve(NID_sect571r1, "NIST Binary-Curve B-571", ctx, out)) goto err;

	ret = 0;

err:
	ERR_print_errors_fp(stderr);
	if (ctx) BN_CTX_free(ctx);
	BIO_free(out);
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_thread_state(NULL);
	CRYPTO_mem_leaks_fp(stderr);
	EXIT(ret);
	return(ret);
	}

#if 0
static void MS_CALLBACK cb(int p, int n, void *arg)
	{
	char c='*';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	BIO_write((BIO *)arg,&c,1);
	(void)BIO_flush((BIO *)arg);
#ifdef LINT
	p=n;
#endif
	}
#endif
#endif
