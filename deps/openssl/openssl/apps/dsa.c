/* apps/dsa.c */
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

#include <openssl/opensslconf.h>	/* for OPENSSL_NO_DSA */
#ifndef OPENSSL_NO_DSA
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bn.h>

#undef PROG
#define PROG	dsa_main

/* -inform arg	- input format - default PEM (one of DER, NET or PEM)
 * -outform arg - output format - default PEM
 * -in arg	- input file - default stdin
 * -out arg	- output file - default stdout
 * -des		- encrypt output if PEM format with DES in cbc mode
 * -des3	- encrypt output if PEM format
 * -idea	- encrypt output if PEM format
 * -aes128	- encrypt output if PEM format
 * -aes192	- encrypt output if PEM format
 * -aes256	- encrypt output if PEM format
 * -camellia128 - encrypt output if PEM format
 * -camellia192 - encrypt output if PEM format
 * -camellia256 - encrypt output if PEM format
 * -seed        - encrypt output if PEM format
 * -text	- print a text version
 * -modulus	- print the DSA public key
 */

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
	ENGINE *e = NULL;
	int ret=1;
	DSA *dsa=NULL;
	int i,badops=0;
	const EVP_CIPHER *enc=NULL;
	BIO *in=NULL,*out=NULL;
	int informat,outformat,text=0,noout=0;
	int pubin = 0, pubout = 0;
	char *infile,*outfile,*prog;
#ifndef OPENSSL_NO_ENGINE
	char *engine;
#endif
	char *passargin = NULL, *passargout = NULL;
	char *passin = NULL, *passout = NULL;
	int modulus=0;

	int pvk_encr = 2;

	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

#ifndef OPENSSL_NO_ENGINE
	engine=NULL;
#endif
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
#ifndef OPENSSL_NO_ENGINE
		else if (strcmp(*argv,"-engine") == 0)
			{
			if (--argc < 1) goto bad;
			engine= *(++argv);
			}
#endif
		else if (strcmp(*argv,"-pvk-strong") == 0)
			pvk_encr=2;
		else if (strcmp(*argv,"-pvk-weak") == 0)
			pvk_encr=1;
		else if (strcmp(*argv,"-pvk-none") == 0)
			pvk_encr=0;
		else if (strcmp(*argv,"-noout") == 0)
			noout=1;
		else if (strcmp(*argv,"-text") == 0)
			text=1;
		else if (strcmp(*argv,"-modulus") == 0)
			modulus=1;
		else if (strcmp(*argv,"-pubin") == 0)
			pubin=1;
		else if (strcmp(*argv,"-pubout") == 0)
			pubout=1;
		else if ((enc=EVP_get_cipherbyname(&(argv[0][1]))) == NULL)
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
		BIO_printf(bio_err," -inform arg     input format - DER or PEM\n");
		BIO_printf(bio_err," -outform arg    output format - DER or PEM\n");
		BIO_printf(bio_err," -in arg         input file\n");
		BIO_printf(bio_err," -passin arg     input file pass phrase source\n");
		BIO_printf(bio_err," -out arg        output file\n");
		BIO_printf(bio_err," -passout arg    output file pass phrase source\n");
#ifndef OPENSSL_NO_ENGINE
		BIO_printf(bio_err," -engine e       use engine e, possibly a hardware device.\n");
#endif
		BIO_printf(bio_err," -des            encrypt PEM output with cbc des\n");
		BIO_printf(bio_err," -des3           encrypt PEM output with ede cbc des using 168 bit key\n");
#ifndef OPENSSL_NO_IDEA
		BIO_printf(bio_err," -idea           encrypt PEM output with cbc idea\n");
#endif
#ifndef OPENSSL_NO_AES
		BIO_printf(bio_err," -aes128, -aes192, -aes256\n");
		BIO_printf(bio_err,"                 encrypt PEM output with cbc aes\n");
#endif
#ifndef OPENSSL_NO_CAMELLIA
		BIO_printf(bio_err," -camellia128, -camellia192, -camellia256\n");
		BIO_printf(bio_err,"                 encrypt PEM output with cbc camellia\n");
#endif
#ifndef OPENSSL_NO_SEED
		BIO_printf(bio_err," -seed           encrypt PEM output with cbc seed\n");
#endif
		BIO_printf(bio_err," -text           print the key in text\n");
		BIO_printf(bio_err," -noout          don't print key out\n");
		BIO_printf(bio_err," -modulus        print the DSA public value\n");
		goto end;
		}

	ERR_load_crypto_strings();

#ifndef OPENSSL_NO_ENGINE
        e = setup_engine(bio_err, engine, 0);
#endif

	if(!app_passwd(bio_err, passargin, passargout, &passin, &passout)) {
		BIO_printf(bio_err, "Error getting passwords\n");
		goto end;
	}

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
			{
			perror(infile);
			goto end;
			}
		}

	BIO_printf(bio_err,"read DSA key\n");

		{
		EVP_PKEY	*pkey;

		if (pubin)
			pkey = load_pubkey(bio_err, infile, informat, 1,
				passin, e, "Public Key");
		else
			pkey = load_key(bio_err, infile, informat, 1,
				passin, e, "Private Key");

		if (pkey)
			{
			dsa = EVP_PKEY_get1_DSA(pkey);
			EVP_PKEY_free(pkey);
			}
		}
	if (dsa == NULL)
		{
		BIO_printf(bio_err,"unable to load Key\n");
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

	if (text) 
		if (!DSA_print(out,dsa,0))
			{
			perror(outfile);
			ERR_print_errors(bio_err);
			goto end;
			}

	if (modulus)
		{
		fprintf(stdout,"Public Key=");
		BN_print(out,dsa->pub_key);
		fprintf(stdout,"\n");
		}

	if (noout) goto end;
	BIO_printf(bio_err,"writing DSA key\n");
	if 	(outformat == FORMAT_ASN1) {
		if(pubin || pubout) i=i2d_DSA_PUBKEY_bio(out,dsa);
		else i=i2d_DSAPrivateKey_bio(out,dsa);
	} else if (outformat == FORMAT_PEM) {
		if(pubin || pubout)
			i=PEM_write_bio_DSA_PUBKEY(out,dsa);
		else i=PEM_write_bio_DSAPrivateKey(out,dsa,enc,
							NULL,0,NULL, passout);
#if !defined(OPENSSL_NO_RSA) && !defined(OPENSSL_NO_RC4)
	} else if (outformat == FORMAT_MSBLOB || outformat == FORMAT_PVK) {
		EVP_PKEY *pk;
		pk = EVP_PKEY_new();
		EVP_PKEY_set1_DSA(pk, dsa);
		if (outformat == FORMAT_PVK)
			i = i2b_PVK_bio(out, pk, pvk_encr, 0, passout);
		else if (pubin || pubout)
			i = i2b_PublicKey_bio(out, pk);
		else
			i = i2b_PrivateKey_bio(out, pk);
		EVP_PKEY_free(pk);
#endif
	} else {
		BIO_printf(bio_err,"bad output format specified for outfile\n");
		goto end;
		}
	if (i <= 0)
		{
		BIO_printf(bio_err,"unable to write private key\n");
		ERR_print_errors(bio_err);
		}
	else
		ret=0;
end:
	if(in != NULL) BIO_free(in);
	if(out != NULL) BIO_free_all(out);
	if(dsa != NULL) DSA_free(dsa);
	if(passin) OPENSSL_free(passin);
	if(passout) OPENSSL_free(passout);
	apps_shutdown();
	OPENSSL_EXIT(ret);
	}
#else /* !OPENSSL_NO_DSA */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
