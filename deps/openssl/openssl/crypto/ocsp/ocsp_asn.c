/* ocsp_asn.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2000.
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
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/ocsp.h>

ASN1_SEQUENCE(OCSP_SIGNATURE) = {
        ASN1_SIMPLE(OCSP_SIGNATURE, signatureAlgorithm, X509_ALGOR),
        ASN1_SIMPLE(OCSP_SIGNATURE, signature, ASN1_BIT_STRING),
        ASN1_EXP_SEQUENCE_OF_OPT(OCSP_SIGNATURE, certs, X509, 0)
} ASN1_SEQUENCE_END(OCSP_SIGNATURE)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_SIGNATURE)

ASN1_SEQUENCE(OCSP_CERTID) = {
        ASN1_SIMPLE(OCSP_CERTID, hashAlgorithm, X509_ALGOR),
        ASN1_SIMPLE(OCSP_CERTID, issuerNameHash, ASN1_OCTET_STRING),
        ASN1_SIMPLE(OCSP_CERTID, issuerKeyHash, ASN1_OCTET_STRING),
        ASN1_SIMPLE(OCSP_CERTID, serialNumber, ASN1_INTEGER)
} ASN1_SEQUENCE_END(OCSP_CERTID)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_CERTID)

ASN1_SEQUENCE(OCSP_ONEREQ) = {
        ASN1_SIMPLE(OCSP_ONEREQ, reqCert, OCSP_CERTID),
        ASN1_EXP_SEQUENCE_OF_OPT(OCSP_ONEREQ, singleRequestExtensions, X509_EXTENSION, 0)
} ASN1_SEQUENCE_END(OCSP_ONEREQ)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_ONEREQ)

ASN1_SEQUENCE(OCSP_REQINFO) = {
        ASN1_EXP_OPT(OCSP_REQINFO, version, ASN1_INTEGER, 0),
        ASN1_EXP_OPT(OCSP_REQINFO, requestorName, GENERAL_NAME, 1),
        ASN1_SEQUENCE_OF(OCSP_REQINFO, requestList, OCSP_ONEREQ),
        ASN1_EXP_SEQUENCE_OF_OPT(OCSP_REQINFO, requestExtensions, X509_EXTENSION, 2)
} ASN1_SEQUENCE_END(OCSP_REQINFO)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_REQINFO)

ASN1_SEQUENCE(OCSP_REQUEST) = {
        ASN1_SIMPLE(OCSP_REQUEST, tbsRequest, OCSP_REQINFO),
        ASN1_EXP_OPT(OCSP_REQUEST, optionalSignature, OCSP_SIGNATURE, 0)
} ASN1_SEQUENCE_END(OCSP_REQUEST)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_REQUEST)

/* OCSP_RESPONSE templates */

ASN1_SEQUENCE(OCSP_RESPBYTES) = {
            ASN1_SIMPLE(OCSP_RESPBYTES, responseType, ASN1_OBJECT),
            ASN1_SIMPLE(OCSP_RESPBYTES, response, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(OCSP_RESPBYTES)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_RESPBYTES)

ASN1_SEQUENCE(OCSP_RESPONSE) = {
        ASN1_SIMPLE(OCSP_RESPONSE, responseStatus, ASN1_ENUMERATED),
        ASN1_EXP_OPT(OCSP_RESPONSE, responseBytes, OCSP_RESPBYTES, 0)
} ASN1_SEQUENCE_END(OCSP_RESPONSE)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_RESPONSE)

ASN1_CHOICE(OCSP_RESPID) = {
           ASN1_EXP(OCSP_RESPID, value.byName, X509_NAME, 1),
           ASN1_EXP(OCSP_RESPID, value.byKey, ASN1_OCTET_STRING, 2)
} ASN1_CHOICE_END(OCSP_RESPID)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_RESPID)

ASN1_SEQUENCE(OCSP_REVOKEDINFO) = {
        ASN1_SIMPLE(OCSP_REVOKEDINFO, revocationTime, ASN1_GENERALIZEDTIME),
        ASN1_EXP_OPT(OCSP_REVOKEDINFO, revocationReason, ASN1_ENUMERATED, 0)
} ASN1_SEQUENCE_END(OCSP_REVOKEDINFO)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_REVOKEDINFO)

ASN1_CHOICE(OCSP_CERTSTATUS) = {
        ASN1_IMP(OCSP_CERTSTATUS, value.good, ASN1_NULL, 0),
        ASN1_IMP(OCSP_CERTSTATUS, value.revoked, OCSP_REVOKEDINFO, 1),
        ASN1_IMP(OCSP_CERTSTATUS, value.unknown, ASN1_NULL, 2)
} ASN1_CHOICE_END(OCSP_CERTSTATUS)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_CERTSTATUS)

ASN1_SEQUENCE(OCSP_SINGLERESP) = {
           ASN1_SIMPLE(OCSP_SINGLERESP, certId, OCSP_CERTID),
           ASN1_SIMPLE(OCSP_SINGLERESP, certStatus, OCSP_CERTSTATUS),
           ASN1_SIMPLE(OCSP_SINGLERESP, thisUpdate, ASN1_GENERALIZEDTIME),
           ASN1_EXP_OPT(OCSP_SINGLERESP, nextUpdate, ASN1_GENERALIZEDTIME, 0),
           ASN1_EXP_SEQUENCE_OF_OPT(OCSP_SINGLERESP, singleExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(OCSP_SINGLERESP)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_SINGLERESP)

ASN1_SEQUENCE(OCSP_RESPDATA) = {
           ASN1_EXP_OPT(OCSP_RESPDATA, version, ASN1_INTEGER, 0),
           ASN1_SIMPLE(OCSP_RESPDATA, responderId, OCSP_RESPID),
           ASN1_SIMPLE(OCSP_RESPDATA, producedAt, ASN1_GENERALIZEDTIME),
           ASN1_SEQUENCE_OF(OCSP_RESPDATA, responses, OCSP_SINGLERESP),
           ASN1_EXP_SEQUENCE_OF_OPT(OCSP_RESPDATA, responseExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(OCSP_RESPDATA)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_RESPDATA)

ASN1_SEQUENCE(OCSP_BASICRESP) = {
           ASN1_SIMPLE(OCSP_BASICRESP, tbsResponseData, OCSP_RESPDATA),
           ASN1_SIMPLE(OCSP_BASICRESP, signatureAlgorithm, X509_ALGOR),
           ASN1_SIMPLE(OCSP_BASICRESP, signature, ASN1_BIT_STRING),
           ASN1_EXP_SEQUENCE_OF_OPT(OCSP_BASICRESP, certs, X509, 0)
} ASN1_SEQUENCE_END(OCSP_BASICRESP)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_BASICRESP)

ASN1_SEQUENCE(OCSP_CRLID) = {
           ASN1_EXP_OPT(OCSP_CRLID, crlUrl, ASN1_IA5STRING, 0),
           ASN1_EXP_OPT(OCSP_CRLID, crlNum, ASN1_INTEGER, 1),
           ASN1_EXP_OPT(OCSP_CRLID, crlTime, ASN1_GENERALIZEDTIME, 2)
} ASN1_SEQUENCE_END(OCSP_CRLID)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_CRLID)

ASN1_SEQUENCE(OCSP_SERVICELOC) = {
        ASN1_SIMPLE(OCSP_SERVICELOC, issuer, X509_NAME),
        ASN1_SEQUENCE_OF_OPT(OCSP_SERVICELOC, locator, ACCESS_DESCRIPTION)
} ASN1_SEQUENCE_END(OCSP_SERVICELOC)

IMPLEMENT_ASN1_FUNCTIONS(OCSP_SERVICELOC)
