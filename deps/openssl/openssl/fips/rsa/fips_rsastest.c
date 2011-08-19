/* fips_rsastest.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
#include <ctype.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>

#ifndef OPENSSL_FIPS

int main(int argc, char *argv[])
{
    printf("No FIPS RSA support\n");
    return(0);
}

#else

#include <openssl/rsa.h>
#include "fips_utl.h"

static int rsa_stest(FILE *out, FILE *in, int Saltlen);
static int rsa_printsig(FILE *out, RSA *rsa, const EVP_MD *dgst,
		unsigned char *Msg, long Msglen, int Saltlen);

int main(int argc, char **argv)
	{
	FILE *in = NULL, *out = NULL;

	int ret = 1, Saltlen = -1;

	if(!FIPS_mode_set(1))
		{
		do_print_errors();
		goto end;
		}

	if ((argc > 2) && !strcmp("-saltlen", argv[1]))
		{
		Saltlen = atoi(argv[2]);
		if (Saltlen < 0)
			{
			fprintf(stderr, "FATAL: Invalid salt length\n");
			goto end;
			}
		argc -= 2;
		argv += 2;
		}
	else if ((argc > 1) && !strcmp("-x931", argv[1]))
		{
		Saltlen = -2;
		argc--;
		argv++;
		}

	if (argc == 1)
		in = stdin;
	else
		in = fopen(argv[1], "r");

	if (argc < 2)
		out = stdout;
	else
		out = fopen(argv[2], "w");

	if (!in)
		{
		fprintf(stderr, "FATAL input initialization error\n");
		goto end;
		}

	if (!out)
		{
		fprintf(stderr, "FATAL output initialization error\n");
		goto end;
		}

	if (!rsa_stest(out, in, Saltlen))
		{
		fprintf(stderr, "FATAL RSASTEST file processing error\n");
		goto end;
		}
	else
		ret = 0;

	end:

	if (ret)
		do_print_errors();

	if (in && (in != stdin))
		fclose(in);
	if (out && (out != stdout))
		fclose(out);

	return ret;

	}

#define RSA_TEST_MAXLINELEN	10240

int rsa_stest(FILE *out, FILE *in, int Saltlen)
	{
	char *linebuf, *olinebuf, *p, *q;
	char *keyword, *value;
	RSA *rsa = NULL;
	const EVP_MD *dgst = NULL;
	unsigned char *Msg = NULL;
	long Msglen = -1;
	int keylen = -1, current_keylen = -1;
	int ret = 0;
	int lnum = 0;

	olinebuf = OPENSSL_malloc(RSA_TEST_MAXLINELEN);
	linebuf = OPENSSL_malloc(RSA_TEST_MAXLINELEN);

	if (!linebuf || !olinebuf)
		goto error;

	while (fgets(olinebuf, RSA_TEST_MAXLINELEN, in))
		{
		lnum++;
		strcpy(linebuf, olinebuf);
		keyword = linebuf;
		/* Skip leading space */
		while (isspace((unsigned char)*keyword))
			keyword++;

		/* Look for = sign */
		p = strchr(linebuf, '=');

		/* If no = just copy */
		if (!p)
			{
			if (fputs(olinebuf, out) < 0)
				goto error;
			continue;
			}

		q = p - 1;

		/* Remove trailing space */
		while (isspace((unsigned char)*q))
			*q-- = 0;

		*p = 0;
		value = p + 1;

		/* Remove leading space from value */
		while (isspace((unsigned char)*value))
			value++;

		/* Remove trailing space from value */
		p = value + strlen(value) - 1;

		while (*p == '\n' || isspace((unsigned char)*p))
			*p-- = 0;

		/* Look for [mod = XXX] for key length */

		if (!strcmp(keyword, "[mod"))
			{
			p = value + strlen(value) - 1;
			if (*p != ']')
				goto parse_error;
			*p = 0;
			keylen = atoi(value);
			if (keylen < 0)
				goto parse_error;
			}
		else if (!strcmp(keyword, "SHAAlg"))
			{
			if (!strcmp(value, "SHA1"))
				dgst = EVP_sha1();
			else if (!strcmp(value, "SHA224"))
				dgst = EVP_sha224();
			else if (!strcmp(value, "SHA256"))
				dgst = EVP_sha256();
			else if (!strcmp(value, "SHA384"))
				dgst = EVP_sha384();
			else if (!strcmp(value, "SHA512"))
				dgst = EVP_sha512();
			else
				{
				fprintf(stderr,
					"FATAL: unsupported algorithm \"%s\"\n",
								value);
				goto parse_error;
				}
			}
		else if (!strcmp(keyword, "Msg"))
			{
			if (Msg)
				goto parse_error;
			if (strlen(value) & 1)
				*(--value) = '0';
			Msg = hex2bin_m(value, &Msglen);
			if (!Msg)
				goto parse_error;
			}

		fputs(olinebuf, out);

		/* If key length has changed, generate and output public
		 * key components of new RSA private key.
		 */

		if (keylen != current_keylen)
			{
			BIGNUM *bn_e;
			if (rsa)
				FIPS_rsa_free(rsa);
			rsa = FIPS_rsa_new();
			if (!rsa)
				goto error;
			bn_e = BN_new();
			if (!bn_e || !BN_set_word(bn_e, 0x1001))
				goto error;
			if (!RSA_X931_generate_key_ex(rsa, keylen, bn_e, NULL))
				goto error;
			BN_free(bn_e);
			fputs("n = ", out);
			do_bn_print(out, rsa->n);
			fputs("\ne = ", out);
			do_bn_print(out, rsa->e);
			fputs("\n", out);
			current_keylen = keylen;
			}

		if (Msg && dgst)
			{
			if (!rsa_printsig(out, rsa, dgst, Msg, Msglen,
								Saltlen))
				goto error;
			OPENSSL_free(Msg);
			Msg = NULL;
			}

		}

	ret = 1;

	error:

	if (olinebuf)
		OPENSSL_free(olinebuf);
	if (linebuf)
		OPENSSL_free(linebuf);
	if (rsa)
		FIPS_rsa_free(rsa);

	return ret;

	parse_error:

	fprintf(stderr, "FATAL parse error processing line %d\n", lnum);

	goto error;

	}

static int rsa_printsig(FILE *out, RSA *rsa, const EVP_MD *dgst,
		unsigned char *Msg, long Msglen, int Saltlen)
	{
	int ret = 0;
	unsigned char *sigbuf = NULL;
	int i, siglen;
	/* EVP_PKEY structure */
	EVP_PKEY pk;
	EVP_MD_CTX ctx;
	pk.type = EVP_PKEY_RSA;
	pk.pkey.rsa = rsa;

	siglen = RSA_size(rsa);
	sigbuf = OPENSSL_malloc(siglen);
	if (!sigbuf)
		goto error;

	EVP_MD_CTX_init(&ctx);

	if (Saltlen >= 0)
		{
		M_EVP_MD_CTX_set_flags(&ctx,
			EVP_MD_CTX_FLAG_PAD_PSS | (Saltlen << 16));
		}
	else if (Saltlen == -2)
		M_EVP_MD_CTX_set_flags(&ctx, EVP_MD_CTX_FLAG_PAD_X931);
	if (!EVP_SignInit_ex(&ctx, dgst, NULL))
		goto error;
	if (!EVP_SignUpdate(&ctx, Msg, Msglen))
		goto error;
	if (!EVP_SignFinal(&ctx, sigbuf, (unsigned int *)&siglen, &pk))
		goto error;

	EVP_MD_CTX_cleanup(&ctx);

	fputs("S = ", out);

	for (i = 0; i < siglen; i++)
		fprintf(out, "%02X", sigbuf[i]);

	fputs("\n", out);

	ret = 1;

	error:

	return ret;
	}
#endif
