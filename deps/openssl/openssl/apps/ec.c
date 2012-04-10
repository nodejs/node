/* apps/ec.c */
/*
 * Written by Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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

#include <openssl/opensslconf.h>
#ifndef OPENSSL_NO_EC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#undef PROG
#define PROG	ec_main

/* -inform arg    - input format - default PEM (one of DER, NET or PEM)
 * -outform arg   - output format - default PEM
 * -in arg        - input file - default stdin
 * -out arg       - output file - default stdout
 * -des           - encrypt output if PEM format with DES in cbc mode
 * -text          - print a text version
 * -param_out     - print the elliptic curve parameters
 * -conv_form arg - specifies the point encoding form
 * -param_enc arg - specifies the parameter encoding
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
	int 	ret = 1;
	EC_KEY 	*eckey = NULL;
	const EC_GROUP *group;
	int 	i, badops = 0;
	const EVP_CIPHER *enc = NULL;
	BIO 	*in = NULL, *out = NULL;
	int 	informat, outformat, text=0, noout=0;
	int  	pubin = 0, pubout = 0, param_out = 0;
	char 	*infile, *outfile, *prog, *engine;
	char 	*passargin = NULL, *passargout = NULL;
	char 	*passin = NULL, *passout = NULL;
	point_conversion_form_t form = POINT_CONVERSION_UNCOMPRESSED;
	int	new_form = 0;
	int	asn1_flag = OPENSSL_EC_NAMED_CURVE;
	int 	new_asn1_flag = 0;

	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err, stderr, BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

	engine = NULL;
	infile = NULL;
	outfile = NULL;
	informat = FORMAT_PEM;
	outformat = FORMAT_PEM;

	prog = argv[0];
	argc--;
	argv++;
	while (argc >= 1)
		{
		if (strcmp(*argv,"-inform") == 0)
			{
			if (--argc < 1) goto bad;
			informat=str2fmt(*(++argv));
			}
		else if (strcmp(*argv,"-outform") == 0)
			{
			if (--argc < 1) goto bad;
			outformat=str2fmt(*(++argv));
			}
		else if (strcmp(*argv,"-in") == 0)
			{
			if (--argc < 1) goto bad;
			infile= *(++argv);
			}
		else if (strcmp(*argv,"-out") == 0)
			{
			if (--argc < 1) goto bad;
			outfile= *(++argv);
			}
		else if (strcmp(*argv,"-passin") == 0)
			{
			if (--argc < 1) goto bad;
			passargin= *(++argv);
			}
		else if (strcmp(*argv,"-passout") == 0)
			{
			if (--argc < 1) goto bad;
			passargout= *(++argv);
			}
		else if (strcmp(*argv, "-engine") == 0)
			{
			if (--argc < 1) goto bad;
			engine= *(++argv);
			}
		else if (strcmp(*argv, "-noout") == 0)
			noout = 1;
		else if (strcmp(*argv, "-text") == 0)
			text = 1;
		else if (strcmp(*argv, "-conv_form") == 0)
			{
			if (--argc < 1)
				goto bad;
			++argv;
			new_form = 1;
			if (strcmp(*argv, "compressed") == 0)
				form = POINT_CONVERSION_COMPRESSED;
			else if (strcmp(*argv, "uncompressed") == 0)
				form = POINT_CONVERSION_UNCOMPRESSED;
			else if (strcmp(*argv, "hybrid") == 0)
				form = POINT_CONVERSION_HYBRID;
			else
				goto bad;
			}
		else if (strcmp(*argv, "-param_enc") == 0)
			{
			if (--argc < 1)
				goto bad;
			++argv;
			new_asn1_flag = 1;
			if (strcmp(*argv, "named_curve") == 0)
				asn1_flag = OPENSSL_EC_NAMED_CURVE;
			else if (strcmp(*argv, "explicit") == 0)
				asn1_flag = 0;
			else
				goto bad;
			}
		else if (strcmp(*argv, "-param_out") == 0)
			param_out = 1;
		else if (strcmp(*argv, "-pubin") == 0)
			pubin=1;
		else if (strcmp(*argv, "-pubout") == 0)
			pubout=1;
		else if ((enc=EVP_get_cipherbyname(&(argv[0][1]))) == NULL)
			{
			BIO_printf(bio_err, "unknown option %s\n", *argv);
			badops=1;
			break;
			}
		argc--;
		argv++;
		}

	if (badops)
		{
bad:
		BIO_printf(bio_err, "%s [options] <infile >outfile\n", prog);
		BIO_printf(bio_err, "where options are\n");
		BIO_printf(bio_err, " -inform arg     input format - "
				"DER or PEM\n");
		BIO_printf(bio_err, " -outform arg    output format - "
				"DER or PEM\n");
		BIO_printf(bio_err, " -in arg         input file\n");
		BIO_printf(bio_err, " -passin arg     input file pass "
				"phrase source\n");
		BIO_printf(bio_err, " -out arg        output file\n");
		BIO_printf(bio_err, " -passout arg    output file pass "
				"phrase source\n");
		BIO_printf(bio_err, " -engine e       use engine e, "
				"possibly a hardware device.\n");
		BIO_printf(bio_err, " -des            encrypt PEM output, "
				"instead of 'des' every other \n"
				"                 cipher "
				"supported by OpenSSL can be used\n");
		BIO_printf(bio_err, " -text           print the key\n");
		BIO_printf(bio_err, " -noout          don't print key out\n");
		BIO_printf(bio_err, " -param_out      print the elliptic "
				"curve parameters\n");
		BIO_printf(bio_err, " -conv_form arg  specifies the "
				"point conversion form \n");
		BIO_printf(bio_err, "                 possible values:"
				" compressed\n");
		BIO_printf(bio_err, "                                 "
				" uncompressed (default)\n");
		BIO_printf(bio_err, "                                  "
				" hybrid\n");
		BIO_printf(bio_err, " -param_enc arg  specifies the way"
				" the ec parameters are encoded\n");
		BIO_printf(bio_err, "                 in the asn1 der "
				"encoding\n");
		BIO_printf(bio_err, "                 possible values:"
				" named_curve (default)\n");
		BIO_printf(bio_err,"                                  "
				"explicit\n");
		goto end;
		}

	ERR_load_crypto_strings();

#ifndef OPENSSL_NO_ENGINE
        setup_engine(bio_err, engine, 0);
#endif

	if(!app_passwd(bio_err, passargin, passargout, &passin, &passout)) 
		{
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
		}

	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL))
		{
		ERR_print_errors(bio_err);
		goto end;
		}

	if (infile == NULL)
		BIO_set_fp(in, stdin, BIO_NOCLOSE);
	else
		{
		if (BIO_read_filename(in, infile) <= 0)
			{
			perror(infile);
			goto end;
			}
		}

	BIO_printf(bio_err, "read EC key\n");
	if (informat == FORMAT_ASN1) 
		{
		if (pubin) 
			eckey = d2i_EC_PUBKEY_bio(in, NULL);
		else 
			eckey = d2i_ECPrivateKey_bio(in, NULL);
		} 
	else if (informat == FORMAT_PEM) 
		{
		if (pubin) 
			eckey = PEM_read_bio_EC_PUBKEY(in, NULL, NULL, 
				NULL);
		else 
			eckey = PEM_read_bio_ECPrivateKey(in, NULL, NULL,
				passin);
		} 
	else
		{
		BIO_printf(bio_err, "bad input format specified for key\n");
		goto end;
		}
	if (eckey == NULL)
		{
		BIO_printf(bio_err,"unable to load Key\n");
		ERR_print_errors(bio_err);
		goto end;
		}

	if (outfile == NULL)
		{
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
			{
			BIO *tmpbio = BIO_new(BIO_f_linebuffer());
			out = BIO_push(tmpbio, out);
			}
#endif
		}
	else
		{
		if (BIO_write_filename(out, outfile) <= 0)
			{
			perror(outfile);
			goto end;
			}
		}

	group = EC_KEY_get0_group(eckey);

	if (new_form)
		EC_KEY_set_conv_form(eckey, form);

	if (new_asn1_flag)
		EC_KEY_set_asn1_flag(eckey, asn1_flag);

	if (text) 
		if (!EC_KEY_print(out, eckey, 0))
			{
			perror(outfile);
			ERR_print_errors(bio_err);
			goto end;
			}

	if (noout) 
		{
		ret = 0;
		goto end;
		}

	BIO_printf(bio_err, "writing EC key\n");
	if (outformat == FORMAT_ASN1) 
		{
		if (param_out)
			i = i2d_ECPKParameters_bio(out, group);
		else if (pubin || pubout) 
			i = i2d_EC_PUBKEY_bio(out, eckey);
		else 
			i = i2d_ECPrivateKey_bio(out, eckey);
		} 
	else if (outformat == FORMAT_PEM) 
		{
		if (param_out)
			i = PEM_write_bio_ECPKParameters(out, group);
		else if (pubin || pubout)
			i = PEM_write_bio_EC_PUBKEY(out, eckey);
		else 
			i = PEM_write_bio_ECPrivateKey(out, eckey, enc,
						NULL, 0, NULL, passout);
		} 
	else 
		{
		BIO_printf(bio_err, "bad output format specified for "
			"outfile\n");
		goto end;
		}

	if (!i)
		{
		BIO_printf(bio_err, "unable to write private key\n");
		ERR_print_errors(bio_err);
		}
	else
		ret=0;
end:
	if (in)
		BIO_free(in);
	if (out)
		BIO_free_all(out);
	if (eckey)
		EC_KEY_free(eckey);
	if (passin)
		OPENSSL_free(passin);
	if (passout)
		OPENSSL_free(passout);
	apps_shutdown();
	OPENSSL_EXIT(ret);
}
#else /* !OPENSSL_NO_EC */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
