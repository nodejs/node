/* crypto/pkcs7/enc.c */
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
#include <string.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>

int main(argc,argv)
int argc;
char *argv[];
	{
	X509 *x509;
	PKCS7 *p7;
	BIO *in;
	BIO *data,*p7bio;
	char buf[1024*4];
	int i;
	int nodetach=1;
	char *keyfile = NULL;
	const EVP_CIPHER *cipher=NULL;
	STACK_OF(X509) *recips=NULL;

	OpenSSL_add_all_algorithms();

	data=BIO_new(BIO_s_file());
	while(argc > 1)
		{
		if (strcmp(argv[1],"-nd") == 0)
			{
			nodetach=1;
			argv++; argc--;
			}
		else if ((strcmp(argv[1],"-c") == 0) && (argc >= 2)) {
			if(!(cipher = EVP_get_cipherbyname(argv[2]))) {
				fprintf(stderr, "Unknown cipher %s\n", argv[2]);
				goto err;
			}
			argc-=2;
			argv+=2;
		} else if ((strcmp(argv[1],"-k") == 0) && (argc >= 2)) {
			keyfile = argv[2];
			argc-=2;
			argv+=2;
			if (!(in=BIO_new_file(keyfile,"r"))) goto err;
			if (!(x509=PEM_read_bio_X509(in,NULL,NULL,NULL)))
				goto err;
			if(!recips) recips = sk_X509_new_null();
			sk_X509_push(recips, x509);
			BIO_free(in);
		} else break;
	}

	if(!recips) {
		fprintf(stderr, "No recipients\n");
		goto err;
	}

	if (!BIO_read_filename(data,argv[1])) goto err;

	p7=PKCS7_new();
#if 0
	BIO_reset(in);
	if ((pkey=PEM_read_bio_PrivateKey(in,NULL,NULL)) == NULL) goto err;
	BIO_free(in);
	PKCS7_set_type(p7,NID_pkcs7_signedAndEnveloped);
	 
	if (PKCS7_add_signature(p7,x509,pkey,EVP_sha1()) == NULL) goto err;
	/* we may want to add more */
	PKCS7_add_certificate(p7,x509);
#else
	PKCS7_set_type(p7,NID_pkcs7_enveloped);
#endif
	if(!cipher)	{
#ifndef OPENSSL_NO_DES
		cipher = EVP_des_ede3_cbc();
#else
		fprintf(stderr, "No cipher selected\n");
		goto err;
#endif
	}

	if (!PKCS7_set_cipher(p7,cipher)) goto err;
	for(i = 0; i < sk_X509_num(recips); i++) {
		if (!PKCS7_add_recipient(p7,sk_X509_value(recips, i))) goto err;
	}
	sk_X509_pop_free(recips, X509_free);

	/* Set the content of the signed to 'data' */
	/* PKCS7_content_new(p7,NID_pkcs7_data); not used in envelope */

	/* could be used, but not in this version :-)
	if (!nodetach) PKCS7_set_detached(p7,1);
	*/

	if ((p7bio=PKCS7_dataInit(p7,NULL)) == NULL) goto err;

	for (;;)
		{
		i=BIO_read(data,buf,sizeof(buf));
		if (i <= 0) break;
		BIO_write(p7bio,buf,i);
		}
	BIO_flush(p7bio);

	if (!PKCS7_dataFinal(p7,p7bio)) goto err;
	BIO_free(p7bio);

	PEM_write_PKCS7(stdout,p7);
	PKCS7_free(p7);

	exit(0);
err:
	ERR_load_crypto_strings();
	ERR_print_errors_fp(stderr);
	exit(1);
	}

