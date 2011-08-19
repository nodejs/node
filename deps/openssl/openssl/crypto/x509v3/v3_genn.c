/* v3_genn.c */
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
#include <openssl/conf.h>
#include <openssl/x509v3.h>

ASN1_SEQUENCE(OTHERNAME) = {
	ASN1_SIMPLE(OTHERNAME, type_id, ASN1_OBJECT),
	/* Maybe have a true ANY DEFINED BY later */
	ASN1_EXP(OTHERNAME, value, ASN1_ANY, 0)
} ASN1_SEQUENCE_END(OTHERNAME)

IMPLEMENT_ASN1_FUNCTIONS(OTHERNAME)

ASN1_SEQUENCE(EDIPARTYNAME) = {
	ASN1_IMP_OPT(EDIPARTYNAME, nameAssigner, DIRECTORYSTRING, 0),
	ASN1_IMP_OPT(EDIPARTYNAME, partyName, DIRECTORYSTRING, 1)
} ASN1_SEQUENCE_END(EDIPARTYNAME)

IMPLEMENT_ASN1_FUNCTIONS(EDIPARTYNAME)

ASN1_CHOICE(GENERAL_NAME) = {
	ASN1_IMP(GENERAL_NAME, d.otherName, OTHERNAME, GEN_OTHERNAME),
	ASN1_IMP(GENERAL_NAME, d.rfc822Name, ASN1_IA5STRING, GEN_EMAIL),
	ASN1_IMP(GENERAL_NAME, d.dNSName, ASN1_IA5STRING, GEN_DNS),
	/* Don't decode this */
	ASN1_IMP(GENERAL_NAME, d.x400Address, ASN1_SEQUENCE, GEN_X400),
	/* X509_NAME is a CHOICE type so use EXPLICIT */
	ASN1_EXP(GENERAL_NAME, d.directoryName, X509_NAME, GEN_DIRNAME),
	ASN1_IMP(GENERAL_NAME, d.ediPartyName, EDIPARTYNAME, GEN_EDIPARTY),
	ASN1_IMP(GENERAL_NAME, d.uniformResourceIdentifier, ASN1_IA5STRING, GEN_URI),
	ASN1_IMP(GENERAL_NAME, d.iPAddress, ASN1_OCTET_STRING, GEN_IPADD),
	ASN1_IMP(GENERAL_NAME, d.registeredID, ASN1_OBJECT, GEN_RID)
} ASN1_CHOICE_END(GENERAL_NAME)

IMPLEMENT_ASN1_FUNCTIONS(GENERAL_NAME)

ASN1_ITEM_TEMPLATE(GENERAL_NAMES) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, GeneralNames, GENERAL_NAME)
ASN1_ITEM_TEMPLATE_END(GENERAL_NAMES)

IMPLEMENT_ASN1_FUNCTIONS(GENERAL_NAMES)
