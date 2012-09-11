/* apps/gendsa.c */
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "apps.h"
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#define DEFBITS	512
#undef PROG
#define PROG gendsa_main

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
	DSA *dsa=NULL;
	int ret=1;
	char *outfile=NULL;
	char *inrand=NULL,*dsaparams=NULL;
	char *passargout = NULL, *passout = NULL;
	BIO *out=NULL,*in=NULL;
	const EVP_CIPHER *enc=NULL;
#ifndef OPENSSL_NO_ENGINE
	char *engine=NULL;
#endif

	apps_startup();

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

	argv++;
	argc--;
	for (;;)
		{
		if (argc <= 0) break;
		if (strcmp(*argv,"-out") == 0)
			{
			if (--argc < 1) goto bad;
			outfile= *(++argv);
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
		else if (strcmp(*argv,"-rand") == 0)
			{
			if (--argc < 1) goto bad;
			inrand= *(++argv);
			}
		else if (strcmp(*argv,"-") == 0)
			goto bad;
#ifndef OPENSSL_NO_DES
		else if (strcmp(*argv,"-des") == 0)
			enc=EVP_des_cbc();
		else if (strcmp(*argv,"-des3") == 0)
			enc=EVP_des_ede3_cbc();
#endif
#ifndef OPENSSL_NO_IDEA
		else if (strcmp(*argv,"-idea") == 0)
			enc=EVP_idea_cbc();
#endif
#ifndef OPENSSL_NO_SEED
		else if (strcmp(*argv,"-seed") == 0)
			enc=EVP_seed_cbc();
#endif
#ifndef OPENSSL_NO_AES
		else if (strcmp(*argv,"-aes128") == 0)
			enc=EVP_aes_128_cbc();
		else if (strcmp(*argv,"-aes192") == 0)
			enc=EVP_aes_192_cbc();
		else if (strcmp(*argv,"-aes256") == 0)
			enc=EVP_aes_256_cbc();
#endif
#ifndef OPENSSL_NO_CAMELLIA
		else if (strcmp(*argv,"-camellia128") == 0)
			enc=EVP_camellia_128_cbc();
		else if (strcmp(*argv,"-camellia192") == 0)
			enc=EVP_camellia_192_cbc();
		else if (strcmp(*argv,"-camellia256") == 0)
			enc=EVP_camellia_256_cbc();
#endif
		else if (**argv != '-' && dsaparams == NULL)
			{
			dsaparams = *argv;
			}
		else
			goto bad;
		argv++;
		argc--;
		}

	if (dsaparams == NULL)
		{
bad:
		BIO_printf(bio_err,"usage: gendsa [args] dsaparam-file\n");
		BIO_printf(bio_err," -out file - output the key to 'file'\n");
#ifndef OPENSSL_NO_DES
		BIO_printf(bio_err," -des      - encrypt the generated key with DES in cbc mode\n");
		BIO_printf(bio_err," -des3     - encrypt the generated key with DES in ede cbc mode (168 bit key)\n");
#endif
#ifndef OPENSSL_NO_IDEA
		BIO_printf(bio_err," -idea     - encrypt the generated key with IDEA in cbc mode\n");
#endif
#ifndef OPENSSL_NO_SEED
		BIO_printf(bio_err," -seed\n");
		BIO_printf(bio_err,"                 encrypt PEM output with cbc seed\n");
#endif
#ifndef OPENSSL_NO_AES
		BIO_printf(bio_err," -aes128, -aes192, -aes256\n");
		BIO_printf(bio_err,"                 encrypt PEM output with cbc aes\n");
#endif
#ifndef OPENSSL_NO_CAMELLIA
		BIO_printf(bio_err," -camellia128, -camellia192, -camellia256\n");
		BIO_printf(bio_err,"                 encrypt PEM output with cbc camellia\n");
#endif
#ifndef OPENSSL_NO_ENGINE
		BIO_printf(bio_err," -engine e - use engine e, possibly a hardware device.\n");
#endif
		BIO_printf(bio_err," -rand file%cfile%c...\n", LIST_SEPARATOR_CHAR, LIST_SEPARATOR_CHAR);
		BIO_printf(bio_err,"           - load the file (or the files in the directory) into\n");
		BIO_printf(bio_err,"             the random number generator\n");
		BIO_printf(bio_err," dsaparam-file\n");
		BIO_printf(bio_err,"           - a DSA parameter file as generated by the dsaparam command\n");
		goto end;
		}

#ifndef OPENSSL_NO_ENGINE
        setup_engine(bio_err, engine, 0);
#endif

	if(!app_passwd(bio_err, NULL, passargout, NULL, &passout)) {
		BIO_printf(bio_err, "Error getting password\n");
		goto end;
	}


	in=BIO_new(BIO_s_file());
	if (!(BIO_read_filename(in,dsaparams)))
		{
		perror(dsaparams);
		goto end;
		}

	if ((dsa=PEM_read_bio_DSAparams(in,NULL,NULL,NULL)) == NULL)
		{
		BIO_printf(bio_err,"unable to load DSA parameter file\n");
		goto end;
		}
	BIO_free(in);
	in = NULL;
		
	out=BIO_new(BIO_s_file());
	if (out == NULL) goto end;

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

	if (!app_RAND_load_file(NULL, bio_err, 1) && inrand == NULL)
		{
		BIO_printf(bio_err,"warning, not much extra random data, consider using the -rand option\n");
		}
	if (inrand != NULL)
		BIO_printf(bio_err,"%ld semi-random bytes loaded\n",
			app_RAND_load_files(inrand));

	BIO_printf(bio_err,"Generating DSA key, %d bits\n",
							BN_num_bits(dsa->p));
	if (!DSA_generate_key(dsa)) goto end;

	app_RAND_write_file(NULL, bio_err);

	if (!PEM_write_bio_DSAPrivateKey(out,dsa,enc,NULL,0,NULL, passout))
		goto end;
	ret=0;
end:
	if (ret != 0)
		ERR_print_errors(bio_err);
	if (in != NULL) BIO_free(in);
	if (out != NULL) BIO_free_all(out);
	if (dsa != NULL) DSA_free(dsa);
	if(passout) OPENSSL_free(passout);
	apps_shutdown();
	OPENSSL_EXIT(ret);
	}
#else /* !OPENSSL_NO_DSA */

# if PEDANTIC
static void *dummy=&dummy;
# endif

#endif
