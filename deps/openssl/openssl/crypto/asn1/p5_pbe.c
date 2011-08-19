/* p5_pbe.c */
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
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

/* PKCS#5 password based encryption structure */

ASN1_SEQUENCE(PBEPARAM) = {
	ASN1_SIMPLE(PBEPARAM, salt, ASN1_OCTET_STRING),
	ASN1_SIMPLE(PBEPARAM, iter, ASN1_INTEGER)
} ASN1_SEQUENCE_END(PBEPARAM)

IMPLEMENT_ASN1_FUNCTIONS(PBEPARAM)

/* Return an algorithm identifier for a PKCS#5 PBE algorithm */

X509_ALGOR *PKCS5_pbe_set(int alg, int iter, unsigned char *salt,
	     int saltlen)
{
	PBEPARAM *pbe=NULL;
	ASN1_OBJECT *al;
	X509_ALGOR *algor;
	ASN1_TYPE *astype=NULL;

	if (!(pbe = PBEPARAM_new ())) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if(iter <= 0) iter = PKCS5_DEFAULT_ITER;
	if (!ASN1_INTEGER_set(pbe->iter, iter)) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!saltlen) saltlen = PKCS5_SALT_LEN;
	if (!(pbe->salt->data = OPENSSL_malloc (saltlen))) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	pbe->salt->length = saltlen;
	if (salt) memcpy (pbe->salt->data, salt, saltlen);
	else if (RAND_pseudo_bytes (pbe->salt->data, saltlen) < 0)
		goto err;

	if (!(astype = ASN1_TYPE_new())) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}

	astype->type = V_ASN1_SEQUENCE;
	if(!ASN1_pack_string_of(PBEPARAM, pbe, i2d_PBEPARAM,
				&astype->value.sequence)) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	PBEPARAM_free (pbe);
	pbe = NULL;
	
	al = OBJ_nid2obj(alg); /* never need to free al */
	if (!(algor = X509_ALGOR_new())) {
		ASN1err(ASN1_F_PKCS5_PBE_SET,ERR_R_MALLOC_FAILURE);
		goto err;
	}
	ASN1_OBJECT_free(algor->algorithm);
	algor->algorithm = al;
	algor->parameter = astype;

	return (algor);
err:
	if (pbe != NULL) PBEPARAM_free(pbe);
	if (astype != NULL) ASN1_TYPE_free(astype);
	return NULL;
}
