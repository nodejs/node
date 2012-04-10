/* p8_pkey.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
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
static int pkey_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it,
							void *exarg)
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

int PKCS8_pkey_set0(PKCS8_PRIV_KEY_INFO *priv, ASN1_OBJECT *aobj,
					int version,
					int ptype, void *pval,
					unsigned char *penc, int penclen)
	{
	unsigned char **ppenc = NULL;
	if (version >= 0)
		{
		if (!ASN1_INTEGER_set(priv->version, version))
			return 0;
		}
	if (penc)
		{
		int pmtype;
		ASN1_OCTET_STRING *oct;
		oct = ASN1_OCTET_STRING_new();
		if (!oct)
			return 0;
		oct->data = penc;
		ppenc = &oct->data;
		oct->length = penclen;
		if (priv->broken == PKCS8_NO_OCTET)
			pmtype = V_ASN1_SEQUENCE;
		else
			pmtype = V_ASN1_OCTET_STRING;
		ASN1_TYPE_set(priv->pkey, pmtype, oct);
		}
	if (!X509_ALGOR_set0(priv->pkeyalg, aobj, ptype, pval))
		{
		/* If call fails do not swallow 'enc' */
		if (ppenc)
			*ppenc = NULL;
		return 0;
		}
	return 1;
	}

int PKCS8_pkey_get0(ASN1_OBJECT **ppkalg,
		const unsigned char **pk, int *ppklen,
		X509_ALGOR **pa,
		PKCS8_PRIV_KEY_INFO *p8)
	{
	if (ppkalg)
		*ppkalg = p8->pkeyalg->algorithm;
	if(p8->pkey->type == V_ASN1_OCTET_STRING)
		{
		p8->broken = PKCS8_OK;
		if (pk)
			{
			*pk = p8->pkey->value.octet_string->data;
			*ppklen = p8->pkey->value.octet_string->length;
			}
		}
	else if (p8->pkey->type == V_ASN1_SEQUENCE)
		{
		p8->broken = PKCS8_NO_OCTET;
		if (pk)
			{
			*pk = p8->pkey->value.sequence->data;
			*ppklen = p8->pkey->value.sequence->length;
			}
		}
	else
		return 0;
	if (pa)
		*pa = p8->pkeyalg;
	return 1;
	}

