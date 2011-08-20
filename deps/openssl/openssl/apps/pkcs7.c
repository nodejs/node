/* apps/pkcs7.c */
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
#include <time.h>
#include "apps.h"
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pem.h>

#undef PROG
#define PROG	pkcs7_main

/* -inform arg	- input format - default PEM (DER or PEM)
 * -outform arg - output format - default PEM
 * -in arg	- input file - default stdin
 * -out arg	- output file - default stdout
 * -print_certs
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
	PKCS7 *p7=NULL;
	int i,badops=0;
	BIO *in=NULL,*out=NULL;
	int informat,outformat;
	char *infile,*outfile,*prog;
	int print_certs=0,text=0,noout=0;
	int ret=1;
#ifndef OPENSSL_NO_ENGINE
	char *engine=NULL;
#endif

	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

	infile=NULL;
	outfile=NULL;
	informat=FORMAT_PEM;
	outformat=FORMAT_PEM;

	prog=argv[0];
	argc--;
	argv++;
	while (argc >= 1)
		{
		if 	(strcmp(*argv,"-inform") == 0)
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
		else if (strcmp(*argv,"-noout") == 0)
			noout=1;
		else if (strcmp(*argv,"-text") == 0)
			text=1;
		else if (strcmp(*argv,"-print_certs") == 0)
			print_certs=1;
#ifndef OPENSSL_NO_ENGINE
		else if (strcmp(*argv,"-engine") == 0)
			{
			if (--argc < 1) goto bad;
			engine= *(++argv);
			}
#endif
		else
			{
			BIO_printf(bio_err,"unknown option %s\n",*argv);
			badops=1;
			break;
			}
		argc--;
		argv++;
		}

	if (badops)
		{
bad:
		BIO_printf(bio_err,"%s [options] <infile >outfile\n",prog);
		BIO_printf(bio_err,"where options are\n");
		BIO_printf(bio_err," -inform arg   input format - DER or PEM\n");
		BIO_printf(bio_err," -outform arg  output format - DER or PEM\n");
		BIO_printf(bio_err," -in arg       input file\n");
		BIO_printf(bio_err," -out arg      output file\n");
		BIO_printf(bio_err," -print_certs  print any certs or crl in the input\n");
		BIO_printf(bio_err," -text         print full details of certificates\n");
		BIO_printf(bio_err," -noout        don't output encoded data\n");
#ifndef OPENSSL_NO_ENGINE
		BIO_printf(bio_err," -engine e     use engine e, possibly a hardware device.\n");
#endif
		ret = 1;
		goto end;
		}

	ERR_load_crypto_strings();

#ifndef OPENSSL_NO_ENGINE
        setup_engine(bio_err, engine, 0);
#endif

	in=BIO_new(BIO_s_file());
	out=BIO_new(BIO_s_file());
	if ((in == NULL) || (out == NULL))
		{
		ERR_print_errors(bio_err);
                goto end;
                }

	if (infile == NULL)
		BIO_set_fp(in,stdin,BIO_NOCLOSE);
	else
		{
		if (BIO_read_filename(in,infile) <= 0)
		if (in == NULL)
			{
			perror(infile);
			goto end;
			}
		}

	if	(informat == FORMAT_ASN1)
		p7=d2i_PKCS7_bio(in,NULL);
	else if (informat == FORMAT_PEM)
		p7=PEM_read_bio_PKCS7(in,NULL,NULL,NULL);
	else
		{
		BIO_printf(bio_err,"bad input format specified for pkcs7 object\n");
		goto end;
		}
	if (p7 == NULL)
		{
		BIO_printf(bio_err,"unable to load PKCS7 object\n");
		ERR_print_errors(bio_err);
		goto end;
		}

	if (outfile == NULL)
		{
		BIO_set_fp(out,stdout,BIO_NOCLOSE);
#ifdef OPENSSL_SYS_VMS
		{
		BIO *tmpbio = BIO_new(BIO_f_linebuffer());
		out = BIO_push(tmpbio, out);
		}
#endif
		}
	else
		{
		if (BIO_write_filename(out,outfile) <= 0)
			{
			perror(outfile);
			goto end;
			}
		}

	if (print_certs)
		{
		STACK_OF(X509) *certs=NULL;
		STACK_OF(X509_CRL) *crls=NULL;

		i=OBJ_obj2nid(p7->type);
		switch (i)
			{
		case NID_pkcs7_signed:
			certs=p7->d.sign->cert;
			crls=p7->d.sign->crl;
			break;
		case NID_pkcs7_signedAndEnveloped:
			certs=p7->d.signed_and_enveloped->cert;
			crls=p7->d.signed_and_enveloped->crl;
			break;
		default:
			break;
			}

		if (certs != NULL)
			{
			X509 *x;

			for (i=0; i<sk_X509_num(certs); i++)
				{
				x=sk_X509_value(certs,i);
				if(text) X509_print(out, x);
				else dump_cert_text(out, x);

				if(!noout) PEM_write_bio_X509(out,x);
				BIO_puts(out,"\n");
				}
			}
		if (crls != NULL)
			{
			X509_CRL *crl;

			for (i=0; i<sk_X509_CRL_num(crls); i++)
				{
				crl=sk_X509_CRL_value(crls,i);

				X509_CRL_print(out, crl);

				if(!noout)PEM_write_bio_X509_CRL(out,crl);
				BIO_puts(out,"\n");
				}
			}

		ret=0;
		goto end;
		}

	if(!noout) {
		if 	(outformat == FORMAT_ASN1)
			i=i2d_PKCS7_bio(out,p7);
		else if (outformat == FORMAT_PEM)
			i=PEM_write_bio_PKCS7(out,p7);
		else	{
			BIO_printf(bio_err,"bad output format specified for outfile\n");
			goto end;
			}

		if (!i)
			{
			BIO_printf(bio_err,"unable to write pkcs7 object\n");
			ERR_print_errors(bio_err);
			goto end;
			}
	}
	ret=0;
end:
	if (p7 != NULL) PKCS7_free(p7);
	if (in != NULL) BIO_free(in);
	if (out != NULL) BIO_free_all(out);
	apps_shutdown();
	OPENSSL_EXIT(ret);
	}
