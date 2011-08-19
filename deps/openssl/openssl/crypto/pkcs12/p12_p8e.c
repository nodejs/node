/* p12_p8e.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
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
#include <openssl/pkcs12.h>

X509_SIG *PKCS8_encrypt(int pbe_nid, const EVP_CIPHER *cipher,
			 const char *pass, int passlen,
			 unsigned char *salt, int saltlen, int iter,
						PKCS8_PRIV_KEY_INFO *p8inf)
{
	X509_SIG *p8 = NULL;
	X509_ALGOR *pbe;

	if (!(p8 = X509_SIG_new())) {
		PKCS12err(PKCS12_F_PKCS8_ENCRYPT, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if(pbe_nid == -1) pbe = PKCS5_pbe2_set(cipher, iter, salt, saltlen);
	else pbe = PKCS5_pbe_set(pbe_nid, iter, salt, saltlen);
	if(!pbe) {
		PKCS12err(PKCS12_F_PKCS8_ENCRYPT, ERR_R_ASN1_LIB);
		goto err;
	}
	X509_ALGOR_free(p8->algor);
	p8->algor = pbe;
	M_ASN1_OCTET_STRING_free(p8->digest);
	p8->digest = PKCS12_item_i2d_encrypt(pbe, ASN1_ITEM_rptr(PKCS8_PRIV_KEY_INFO),
					pass, passlen, p8inf, 1);
	if(!p8->digest) {
		PKCS12err(PKCS12_F_PKCS8_ENCRYPT, PKCS12_R_ENCRYPT_ERROR);
		goto err;
	}

	return p8;

	err:
	X509_SIG_free(p8);
	return NULL;
}
