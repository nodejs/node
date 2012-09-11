/* nseq.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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

#undef PROG
#define PROG nseq_main

int MAIN(int, char **);

int MAIN(int argc, char **argv)
{
	char **args, *infile = NULL, *outfile = NULL;
	BIO *in = NULL, *out = NULL;
	int toseq = 0;
	X509 *x509 = NULL;
	NETSCAPE_CERT_SEQUENCE *seq = NULL;
	int i, ret = 1;
	int badarg = 0;
	if (bio_err == NULL) bio_err = BIO_new_fp (stderr, BIO_NOCLOSE);
	ERR_load_crypto_strings();
	args = argv + 1;
	while (!badarg && *args && *args[0] == '-') {
		if (!strcmp (*args, "-toseq")) toseq = 1;
		else if (!strcmp (*args, "-in")) {
			if (args[1]) {
				args++;
				infile = *args;
			} else badarg = 1;
		} else if (!strcmp (*args, "-out")) {
			if (args[1]) {
				args++;
				outfile = *args;
			} else badarg = 1;
		} else badarg = 1;
		args++;
	}

	if (badarg) {
		BIO_printf (bio_err, "Netscape certificate sequence utility\n");
		BIO_printf (bio_err, "Usage nseq [options]\n");
		BIO_printf (bio_err, "where options are\n");
		BIO_printf (bio_err, "-in file  input file\n");
		BIO_printf (bio_err, "-out file output file\n");
		BIO_printf (bio_err, "-toseq    output NS Sequence file\n");
		OPENSSL_EXIT(1);
	}

	if (infile) {
		if (!(in = BIO_new_file (infile, "r"))) {
			BIO_printf (bio_err,
				 "Can't open input file %s\n", infile);
			goto end;
		}
	} else in = BIO_new_fp(stdin, BIO_NOCLOSE);

	if (outfile) {
		if (!(out = BIO_new_file (outfile, "w"))) {
			BIO_printf (bio_err,
				 "Can't open output file %s\n", outfile);
			goto end;
		}
	} else {
		out = BIO_new_fp(stdout, BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
		{
		BIO *tmpbio = BIO_new(BIO_f_linebuffer());
		out = BIO_push(tmpbio, out);
		}
#endif
	}
	if (toseq) {
		seq = NETSCAPE_CERT_SEQUENCE_new();
		seq->certs = sk_X509_new_null();
		while((x509 = PEM_read_bio_X509(in, NULL, NULL, NULL))) 
		    sk_X509_push(seq->certs,x509);

		if(!sk_X509_num(seq->certs))
		{
			BIO_printf (bio_err, "Error reading certs file %s\n", infile);
			ERR_print_errors(bio_err);
			goto end;
		}
		PEM_write_bio_NETSCAPE_CERT_SEQUENCE(out, seq);
		ret = 0;
		goto end;
	}

	if (!(seq = PEM_read_bio_NETSCAPE_CERT_SEQUENCE(in, NULL, NULL, NULL))) {
		BIO_printf (bio_err, "Error reading sequence file %s\n", infile);
		ERR_print_errors(bio_err);
		goto end;
	}

	for(i = 0; i < sk_X509_num(seq->certs); i++) {
		x509 = sk_X509_value(seq->certs, i);
		dump_cert_text(out, x509);
		PEM_write_bio_X509(out, x509);
	}
	ret = 0;
end:
	BIO_free(in);
	BIO_free_all(out);
	NETSCAPE_CERT_SEQUENCE_free(seq);

	OPENSSL_EXIT(ret);
}

