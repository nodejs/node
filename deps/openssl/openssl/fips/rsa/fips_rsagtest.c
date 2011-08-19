/* fips_rsagtest.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005,2007 The OpenSSL Project.  All rights reserved.
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

int rsa_test(FILE *out, FILE *in);
static int rsa_printkey1(FILE *out, RSA *rsa,
		BIGNUM *Xp1, BIGNUM *Xp2, BIGNUM *Xp,
		BIGNUM *e);
static int rsa_printkey2(FILE *out, RSA *rsa,
		BIGNUM *Xq1, BIGNUM *Xq2, BIGNUM *Xq);

int main(int argc, char **argv)
	{
	FILE *in = NULL, *out = NULL;

	int ret = 1;

	if(!FIPS_mode_set(1))
		{
		do_print_errors();
		goto end;
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

	if (!rsa_test(out, in))
		{
		fprintf(stderr, "FATAL RSAGTEST file processing error\n");
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

int rsa_test(FILE *out, FILE *in)
	{
	char *linebuf, *olinebuf, *p, *q;
	char *keyword, *value;
	RSA *rsa = NULL;
	BIGNUM *Xp1 = NULL, *Xp2 = NULL, *Xp = NULL;
	BIGNUM *Xq1 = NULL, *Xq2 = NULL, *Xq = NULL;
	BIGNUM *e = NULL;
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

		/* If no = or starts with [ (for [foo = bar] line) just copy */
		if (!p || *keyword=='[')
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

		if (!strcmp(keyword, "xp1"))
			{
			if (Xp1 || !do_hex2bn(&Xp1,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "xp2"))
			{
			if (Xp2 || !do_hex2bn(&Xp2,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "Xp"))
			{
			if (Xp || !do_hex2bn(&Xp,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "xq1"))
			{
			if (Xq1 || !do_hex2bn(&Xq1,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "xq2"))
			{
			if (Xq2 || !do_hex2bn(&Xq2,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "Xq"))
			{
			if (Xq || !do_hex2bn(&Xq,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "e"))
			{
			if (e || !do_hex2bn(&e,value))
				goto parse_error;
			}
		else if (!strcmp(keyword, "p1"))
			continue;
		else if (!strcmp(keyword, "p2"))
			continue;
		else if (!strcmp(keyword, "p"))
			continue;
		else if (!strcmp(keyword, "q1"))
			continue;
		else if (!strcmp(keyword, "q2"))
			continue;
		else if (!strcmp(keyword, "q"))
			continue;
		else if (!strcmp(keyword, "n"))
			continue;
		else if (!strcmp(keyword, "d"))
			continue;
		else
			goto parse_error;

		fputs(olinebuf, out);

		if (e && Xp1 && Xp2 && Xp)
			{
			rsa = FIPS_rsa_new();
			if (!rsa)
				goto error;
			if (!rsa_printkey1(out, rsa, Xp1, Xp2, Xp, e))
				goto error;
			BN_free(Xp1);
			Xp1 = NULL;
			BN_free(Xp2);
			Xp2 = NULL;
			BN_free(Xp);
			Xp = NULL;
			BN_free(e);
			e = NULL;
			}

		if (rsa && Xq1 && Xq2 && Xq)
			{
			if (!rsa_printkey2(out, rsa, Xq1, Xq2, Xq))
				goto error;
			BN_free(Xq1);
			Xq1 = NULL;
			BN_free(Xq2);
			Xq2 = NULL;
			BN_free(Xq);
			Xq = NULL;
			FIPS_rsa_free(rsa);
			rsa = NULL;
			}
		}

	ret = 1;

	error:

	if (olinebuf)
		OPENSSL_free(olinebuf);
	if (linebuf)
		OPENSSL_free(linebuf);

	if (Xp1)
		BN_free(Xp1);
	if (Xp2)
		BN_free(Xp2);
	if (Xp)
		BN_free(Xp);
	if (Xq1)
		BN_free(Xq1);
	if (Xq1)
		BN_free(Xq1);
	if (Xq2)
		BN_free(Xq2);
	if (Xq)
		BN_free(Xq);
	if (e)
		BN_free(e);
	if (rsa)
		FIPS_rsa_free(rsa);

	return ret;

	parse_error:

	fprintf(stderr, "FATAL parse error processing line %d\n", lnum);

	goto error;

	}

static int rsa_printkey1(FILE *out, RSA *rsa,
		BIGNUM *Xp1, BIGNUM *Xp2, BIGNUM *Xp,
		BIGNUM *e)
	{
	int ret = 0;
	BIGNUM *p1 = NULL, *p2 = NULL;
	p1 = BN_new();
	p2 = BN_new();
	if (!p1 || !p2)
		goto error;

	if (!RSA_X931_derive_ex(rsa, p1, p2, NULL, NULL, Xp1, Xp2, Xp,
						NULL, NULL, NULL, e, NULL))
		goto error;

	do_bn_print_name(out, "p1", p1);
	do_bn_print_name(out, "p2", p2);
	do_bn_print_name(out, "p", rsa->p);

	ret = 1;

	error:
	if (p1)
		BN_free(p1);
	if (p2)
		BN_free(p2);

	return ret;
	}

static int rsa_printkey2(FILE *out, RSA *rsa,
		BIGNUM *Xq1, BIGNUM *Xq2, BIGNUM *Xq)
	{
	int ret = 0;
	BIGNUM *q1 = NULL, *q2 = NULL;
	q1 = BN_new();
	q2 = BN_new();
	if (!q1 || !q2)
		goto error;

	if (!RSA_X931_derive_ex(rsa, NULL, NULL, q1, q2, NULL, NULL, NULL,
						Xq1, Xq2, Xq, NULL, NULL))
		goto error;

	do_bn_print_name(out, "q1", q1);
	do_bn_print_name(out, "q2", q2);
	do_bn_print_name(out, "q", rsa->q);
	do_bn_print_name(out, "n", rsa->n);
	do_bn_print_name(out, "d", rsa->d);

	ret = 1;

	error:
	if (q1)
		BN_free(q1);
	if (q2)
		BN_free(q2);

	return ret;
	}

#endif
