/* tasn_typ.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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
#include <openssl/asn1.h>
#include <openssl/asn1t.h>

/* Declarations for string types */


IMPLEMENT_ASN1_TYPE(ASN1_INTEGER)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_INTEGER)

IMPLEMENT_ASN1_TYPE(ASN1_ENUMERATED)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_ENUMERATED)

IMPLEMENT_ASN1_TYPE(ASN1_BIT_STRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_BIT_STRING)

IMPLEMENT_ASN1_TYPE(ASN1_OCTET_STRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_OCTET_STRING)

IMPLEMENT_ASN1_TYPE(ASN1_NULL)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_NULL)

IMPLEMENT_ASN1_TYPE(ASN1_OBJECT)

IMPLEMENT_ASN1_TYPE(ASN1_UTF8STRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_UTF8STRING)

IMPLEMENT_ASN1_TYPE(ASN1_PRINTABLESTRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_PRINTABLESTRING)

IMPLEMENT_ASN1_TYPE(ASN1_T61STRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_T61STRING)

IMPLEMENT_ASN1_TYPE(ASN1_IA5STRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_IA5STRING)

IMPLEMENT_ASN1_TYPE(ASN1_GENERALSTRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_GENERALSTRING)

IMPLEMENT_ASN1_TYPE(ASN1_UTCTIME)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_UTCTIME)

IMPLEMENT_ASN1_TYPE(ASN1_GENERALIZEDTIME)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_GENERALIZEDTIME)

IMPLEMENT_ASN1_TYPE(ASN1_VISIBLESTRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_VISIBLESTRING)

IMPLEMENT_ASN1_TYPE(ASN1_UNIVERSALSTRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_UNIVERSALSTRING)

IMPLEMENT_ASN1_TYPE(ASN1_BMPSTRING)
IMPLEMENT_ASN1_FUNCTIONS(ASN1_BMPSTRING)

IMPLEMENT_ASN1_TYPE(ASN1_ANY)

/* Just swallow an ASN1_SEQUENCE in an ASN1_STRING */
IMPLEMENT_ASN1_TYPE(ASN1_SEQUENCE)

IMPLEMENT_ASN1_FUNCTIONS_fname(ASN1_TYPE, ASN1_ANY, ASN1_TYPE)

/* Multistring types */

IMPLEMENT_ASN1_MSTRING(ASN1_PRINTABLE, B_ASN1_PRINTABLE)
IMPLEMENT_ASN1_FUNCTIONS_name(ASN1_STRING, ASN1_PRINTABLE)

IMPLEMENT_ASN1_MSTRING(DISPLAYTEXT, B_ASN1_DISPLAYTEXT)
IMPLEMENT_ASN1_FUNCTIONS_name(ASN1_STRING, DISPLAYTEXT)

IMPLEMENT_ASN1_MSTRING(DIRECTORYSTRING, B_ASN1_DIRECTORYSTRING)
IMPLEMENT_ASN1_FUNCTIONS_name(ASN1_STRING, DIRECTORYSTRING)

/* Three separate BOOLEAN type: normal, DEFAULT TRUE and DEFAULT FALSE */
IMPLEMENT_ASN1_TYPE_ex(ASN1_BOOLEAN, ASN1_BOOLEAN, -1)
IMPLEMENT_ASN1_TYPE_ex(ASN1_TBOOLEAN, ASN1_BOOLEAN, 1)
IMPLEMENT_ASN1_TYPE_ex(ASN1_FBOOLEAN, ASN1_BOOLEAN, 0)

/* Special, OCTET STRING with indefinite length constructed support */

IMPLEMENT_ASN1_TYPE_ex(ASN1_OCTET_STRING_NDEF, ASN1_OCTET_STRING, ASN1_TFLG_NDEF)

ASN1_ITEM_TEMPLATE(ASN1_SEQUENCE_ANY) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, ASN1_SEQUENCE_ANY, ASN1_ANY)
ASN1_ITEM_TEMPLATE_END(ASN1_SEQUENCE_ANY)

ASN1_ITEM_TEMPLATE(ASN1_SET_ANY) = 
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SET_OF, 0, ASN1_SET_ANY, ASN1_ANY)
ASN1_ITEM_TEMPLATE_END(ASN1_SET_ANY)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(ASN1_SEQUENCE_ANY, ASN1_SEQUENCE_ANY, ASN1_SEQUENCE_ANY)
IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(ASN1_SEQUENCE_ANY, ASN1_SET_ANY, ASN1_SET_ANY)
