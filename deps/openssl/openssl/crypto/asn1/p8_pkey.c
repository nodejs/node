/* p8_pkey.c */
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

/* Minor tweak to operation: zero private key data */
static int pkey_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	/* Since the structure must still be valid use ASN1_OP_FREE_PRE */
	if(operation == ASN1_OP_FREE_PRE) {
		PKCS8_PRIV_KEY_INFO *key = (PKCS8_PRIV_KEY_INFO *)*pval;
		if (key->pkey->value.octet_string)
		OPENSSL_cleanse(key->pkey->value.octet_string->data,
			key->pkey->value.octet_string->length);
	}
	return 1;
}

ASN1_SEQUENCE_cb(PKCS8_PRIV_KEY_INFO, pkey_cb) = {
	ASN1_SIMPLE(PKCS8_PRIV_KEY_INFO, version, ASN1_INTEGER),
	ASN1_SIMPLE(PKCS8_PRIV_KEY_INFO, pkeyalg, X509_ALGOR),
	ASN1_SIMPLE(PKCS8_PRIV_KEY_INFO, pkey, ASN1_ANY),
	ASN1_IMP_SET_OF_OPT(PKCS8_PRIV_KEY_INFO, attributes, X509_ATTRIBUTE, 0)
} ASN1_SEQUENCE_END_cb(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO)

IMPLEMENT_ASN1_FUNCTIONS(PKCS8_PRIV_KEY_INFO)
