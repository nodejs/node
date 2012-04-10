/* apps/pkeyparam.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
#include <string.h>
#include "apps.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#define PROG pkeyparam_main

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
	char **args, *infile = NULL, *outfile = NULL;
	BIO *in = NULL, *out = NULL;
	int text = 0, noout = 0;
	EVP_PKEY *pkey=NULL;
	int badarg = 0;
#ifndef OPENSSL_NO_ENGINE
	char *engine=NULL;
#endif
	int ret = 1;

	if (bio_err == NULL)
		bio_err = BIO_new_fp (stderr, BIO_NOCLOSE);

	if (!load_config(bio_err, NULL))
		goto end;

	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	args = argv + 1;
	while (!badarg && *args && *args[0] == '-')
		{
		if (!strcmp (*args, "-in"))
			{
			if (args[1])
				{
				args++;
				infile = *args;
				}
			else badarg = 1;
			}
		else if (!strcmp (*args, "-out"))
			{
			if (args[1])
				{
				args++;
				outfile = *args;
				}
			else badarg = 1;
			}
#ifndef OPENSSL_NO_ENGINE
		else if (strcmp(*args,"-engine") == 0)
			{
			if (!args[1]) goto bad;
			engine= *(++args);
			}
#endif

		else if (strcmp(*args,"-text") == 0)
			text=1;
		else if (strcmp(*args,"-noout") == 0)
			noout=1;
		args++;
		}

	if (badarg)
		{
#ifndef OPENSSL_NO_ENGINE
		bad:
#endif
		BIO_printf(bio_err, "Usage pkeyparam [options]\n");
		BIO_printf(bio_err, "where options are\n");
		BIO_printf(bio_err, "-in file        input file\n");
		BIO_printf(bio_err, "-out file       output file\n");
		BIO_printf(bio_err, "-text           print parameters as text\n");
		BIO_printf(bio_err, "-noout          don't output encoded parameters\n");
#ifndef OPENSSL_NO_ENGINE
		BIO_printf(bio_err, "-engine e       use engine e, possibly a hardware device.\n");
#endif
		return 1;
		}

#ifndef OPENSSL_NO_ENGINE
        setup_engine(bio_err, engine, 0);
#endif

	if (infile)
		{
		if (!(in = BIO_new_file (infile, "r")))
			{
			BIO_printf(bio_err,
				 "Can't open input file %s\n", infile);
			goto end;
			}
		}
	else
		in = BIO_new_fp (stdin, BIO_NOCLOSE);

	if (outfile)
		{
		if (!(out = BIO_new_file (outfile, "w")))
			{
			BIO_printf(bio_err,
				 "Can't open output file %s\n", outfile);
			goto end;
			}
		}
	else
		{
		out = BIO_new_fp (stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
			{
			BIO *tmpbio = BIO_new(BIO_f_linebuffer());
			out = BIO_push(tmpbio, out);
			}
#endif
		}

	pkey = PEM_read_bio_Parameters(in, NULL);
	if (!pkey)
		{
		BIO_printf(bio_err, "Error reading parameters\n");
		ERR_print_errors(bio_err);
		goto end;
		}

	if (!noout)
		PEM_write_bio_Parameters(out,pkey);

	if (text)
		EVP_PKEY_print_params(out, pkey, 0, NULL);

	ret = 0;

	end:
	EVP_PKEY_free(pkey);
	BIO_free_all(out);
	BIO_free(in);

	return ret;
	}
