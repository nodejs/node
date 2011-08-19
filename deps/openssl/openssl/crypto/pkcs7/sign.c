/* crypto/pkcs7/sign.c */
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
	EVP_PKEY *pkey;
	PKCS7 *p7;
	PKCS7_SIGNER_INFO *si;
	BIO *in;
	BIO *data,*p7bio;
	char buf[1024*4];
	int i;
	int nodetach=0;

#ifndef OPENSSL_NO_MD2
	EVP_add_digest(EVP_md2());
#endif
#ifndef OPENSSL_NO_MD5
	EVP_add_digest(EVP_md5());
#endif
#ifndef OPENSSL_NO_SHA1
	EVP_add_digest(EVP_sha1());
#endif
#ifndef OPENSSL_NO_MDC2
	EVP_add_digest(EVP_mdc2());
#endif

	data=BIO_new(BIO_s_file());
again:
	if (argc > 1)
		{
		if (strcmp(argv[1],"-nd") == 0)
			{
			nodetach=1;
			argv++; argc--;
			goto again;
			}
		if (!BIO_read_filename(data,argv[1]))
			goto err;
		}
	else
		BIO_set_fp(data,stdin,BIO_NOCLOSE);

	if ((in=BIO_new_file("server.pem","r")) == NULL) goto err;
	if ((x509=PEM_read_bio_X509(in,NULL,NULL,NULL)) == NULL) goto err;
	BIO_reset(in);
	if ((pkey=PEM_read_bio_PrivateKey(in,NULL,NULL,NULL)) == NULL) goto err;
	BIO_free(in);

	p7=PKCS7_new();
	PKCS7_set_type(p7,NID_pkcs7_signed);
	 
	si=PKCS7_add_signature(p7,x509,pkey,EVP_sha1());
	if (si == NULL) goto err;

	/* If you do this then you get signing time automatically added */
	PKCS7_add_signed_attribute(si, NID_pkcs9_contentType, V_ASN1_OBJECT,
						OBJ_nid2obj(NID_pkcs7_data));

	/* we may want to add more */
	PKCS7_add_certificate(p7,x509);

	/* Set the content of the signed to 'data' */
	PKCS7_content_new(p7,NID_pkcs7_data);

	if (!nodetach)
		PKCS7_set_detached(p7,1);

	if ((p7bio=PKCS7_dataInit(p7,NULL)) == NULL) goto err;

	for (;;)
		{
		i=BIO_read(data,buf,sizeof(buf));
		if (i <= 0) break;
		BIO_write(p7bio,buf,i);
		}

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

