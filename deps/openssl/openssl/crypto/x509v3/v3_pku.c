/* v3_pku.c */
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
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>

static int i2r_PKEY_USAGE_PERIOD(X509V3_EXT_METHOD *method, PKEY_USAGE_PERIOD *usage, BIO *out, int indent);
/*
static PKEY_USAGE_PERIOD *v2i_PKEY_USAGE_PERIOD(X509V3_EXT_METHOD *method, X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *values);
*/
const X509V3_EXT_METHOD v3_pkey_usage_period = {
NID_private_key_usage_period, 0, ASN1_ITEM_ref(PKEY_USAGE_PERIOD),
0,0,0,0,
0,0,0,0,
(X509V3_EXT_I2R)i2r_PKEY_USAGE_PERIOD, NULL,
NULL
};

ASN1_SEQUENCE(PKEY_USAGE_PERIOD) = {
	ASN1_IMP_OPT(PKEY_USAGE_PERIOD, notBefore, ASN1_GENERALIZEDTIME, 0),
	ASN1_IMP_OPT(PKEY_USAGE_PERIOD, notAfter, ASN1_GENERALIZEDTIME, 1)
} ASN1_SEQUENCE_END(PKEY_USAGE_PERIOD)

IMPLEMENT_ASN1_FUNCTIONS(PKEY_USAGE_PERIOD)

static int i2r_PKEY_USAGE_PERIOD(X509V3_EXT_METHOD *method,
	     PKEY_USAGE_PERIOD *usage, BIO *out, int indent)
{
	BIO_printf(out, "%*s", indent, "");
	if(usage->notBefore) {
		BIO_write(out, "Not Before: ", 12);
		ASN1_GENERALIZEDTIME_print(out, usage->notBefore);
		if(usage->notAfter) BIO_write(out, ", ", 2);
	}
	if(usage->notAfter) {
		BIO_write(out, "Not After: ", 11);
		ASN1_GENERALIZEDTIME_print(out, usage->notAfter);
	}
	return 1;
}

/*
static PKEY_USAGE_PERIOD *v2i_PKEY_USAGE_PERIOD(method, ctx, values)
X509V3_EXT_METHOD *method;
X509V3_CTX *ctx;
STACK_OF(CONF_VALUE) *values;
{
return NULL;
}
*/
