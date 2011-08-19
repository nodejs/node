/* v3_crld.c */
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
#include <openssl/conf.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>

static STACK_OF(CONF_VALUE) *i2v_crld(X509V3_EXT_METHOD *method,
		STACK_OF(DIST_POINT) *crld, STACK_OF(CONF_VALUE) *extlist);
static STACK_OF(DIST_POINT) *v2i_crld(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval);

const X509V3_EXT_METHOD v3_crld = {
NID_crl_distribution_points, X509V3_EXT_MULTILINE, ASN1_ITEM_ref(CRL_DIST_POINTS),
0,0,0,0,
0,0,
(X509V3_EXT_I2V)i2v_crld,
(X509V3_EXT_V2I)v2i_crld,
0,0,
NULL
};

static STACK_OF(CONF_VALUE) *i2v_crld(X509V3_EXT_METHOD *method,
			STACK_OF(DIST_POINT) *crld, STACK_OF(CONF_VALUE) *exts)
{
	DIST_POINT *point;
	int i;
	for(i = 0; i < sk_DIST_POINT_num(crld); i++) {
		point = sk_DIST_POINT_value(crld, i);
		if(point->distpoint) {
			if(point->distpoint->type == 0)
				exts = i2v_GENERAL_NAMES(NULL,
					 point->distpoint->name.fullname, exts);
		        else X509V3_add_value("RelativeName","<UNSUPPORTED>", &exts);
		}
		if(point->reasons) 
			X509V3_add_value("reasons","<UNSUPPORTED>", &exts);
		if(point->CRLissuer)
			X509V3_add_value("CRLissuer","<UNSUPPORTED>", &exts);
	}
	return exts;
}

static STACK_OF(DIST_POINT) *v2i_crld(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval)
{
	STACK_OF(DIST_POINT) *crld = NULL;
	GENERAL_NAMES *gens = NULL;
	GENERAL_NAME *gen = NULL;
	CONF_VALUE *cnf;
	int i;
	if(!(crld = sk_DIST_POINT_new_null())) goto merr;
	for(i = 0; i < sk_CONF_VALUE_num(nval); i++) {
		DIST_POINT *point;
		cnf = sk_CONF_VALUE_value(nval, i);
		if(!(gen = v2i_GENERAL_NAME(method, ctx, cnf))) goto err; 
		if(!(gens = GENERAL_NAMES_new())) goto merr;
		if(!sk_GENERAL_NAME_push(gens, gen)) goto merr;
		gen = NULL;
		if(!(point = DIST_POINT_new())) goto merr;
		if(!sk_DIST_POINT_push(crld, point)) {
			DIST_POINT_free(point);
			goto merr;
		}
		if(!(point->distpoint = DIST_POINT_NAME_new())) goto merr;
		point->distpoint->name.fullname = gens;
		point->distpoint->type = 0;
		gens = NULL;
	}
	return crld;

	merr:
	X509V3err(X509V3_F_V2I_CRLD,ERR_R_MALLOC_FAILURE);
	err:
	GENERAL_NAME_free(gen);
	GENERAL_NAMES_free(gens);
	sk_DIST_POINT_pop_free(crld, DIST_POINT_free);
	return NULL;
}

IMPLEMENT_STACK_OF(DIST_POINT)
IMPLEMENT_ASN1_SET_OF(DIST_POINT)


ASN1_CHOICE(DIST_POINT_NAME) = {
	ASN1_IMP_SEQUENCE_OF(DIST_POINT_NAME, name.fullname, GENERAL_NAME, 0),
	ASN1_IMP_SET_OF(DIST_POINT_NAME, name.relativename, X509_NAME_ENTRY, 1)
} ASN1_CHOICE_END(DIST_POINT_NAME)

IMPLEMENT_ASN1_FUNCTIONS(DIST_POINT_NAME)

ASN1_SEQUENCE(DIST_POINT) = {
	ASN1_EXP_OPT(DIST_POINT, distpoint, DIST_POINT_NAME, 0),
	ASN1_IMP_OPT(DIST_POINT, reasons, ASN1_BIT_STRING, 1),
	ASN1_IMP_SEQUENCE_OF_OPT(DIST_POINT, CRLissuer, GENERAL_NAME, 2)
} ASN1_SEQUENCE_END(DIST_POINT)

IMPLEMENT_ASN1_FUNCTIONS(DIST_POINT)

ASN1_ITEM_TEMPLATE(CRL_DIST_POINTS) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, CRLDistributionPoints, DIST_POINT)
ASN1_ITEM_TEMPLATE_END(CRL_DIST_POINTS)

IMPLEMENT_ASN1_FUNCTIONS(CRL_DIST_POINTS)
