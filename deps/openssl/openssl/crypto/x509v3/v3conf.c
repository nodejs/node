/* v3conf.c */
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
#include "cryptlib.h"
#include <openssl/asn1.h>
#include <openssl/conf.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

/* Test application to add extensions from a config file */

int main(int argc, char **argv)
{
	LHASH *conf;
	X509 *cert;
	FILE *inf;
	char *conf_file;
	int i;
	int count;
	X509_EXTENSION *ext;
	X509V3_add_standard_extensions();
	ERR_load_crypto_strings();
	if(!argv[1]) {
		fprintf(stderr, "Usage: v3conf cert.pem [file.cnf]\n");
		exit(1);
	}
	conf_file = argv[2];
	if(!conf_file) conf_file = "test.cnf";
	conf = CONF_load(NULL, "test.cnf", NULL);
	if(!conf) {
		fprintf(stderr, "Error opening Config file %s\n", conf_file);
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	inf = fopen(argv[1], "r");
	if(!inf) {
		fprintf(stderr, "Can't open certificate file %s\n", argv[1]);
		exit(1);
	}
	cert = PEM_read_X509(inf, NULL, NULL);
	if(!cert) {
		fprintf(stderr, "Error reading certificate file %s\n", argv[1]);
		exit(1);
	}
	fclose(inf);

	sk_pop_free(cert->cert_info->extensions, X509_EXTENSION_free);
	cert->cert_info->extensions = NULL;

	if(!X509V3_EXT_add_conf(conf, NULL, "test_section", cert)) {
		fprintf(stderr, "Error adding extensions\n");
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	count = X509_get_ext_count(cert);
	printf("%d extensions\n", count);
	for(i = 0; i < count; i++) {
		ext = X509_get_ext(cert, i);
		printf("%s", OBJ_nid2ln(OBJ_obj2nid(ext->object)));
		if(ext->critical) printf(",critical:\n");
		else printf(":\n");
		X509V3_EXT_print_fp(stdout, ext, 0, 0);
		printf("\n");
		
	}
	return 0;
}

