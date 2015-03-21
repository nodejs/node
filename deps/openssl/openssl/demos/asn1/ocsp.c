/* ocsp.c */
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
#include <openssl/x509v3.h>

/*-
   Example of new ASN1 code, OCSP request

        OCSPRequest     ::=     SEQUENCE {
            tbsRequest                  TBSRequest,
            optionalSignature   [0]     EXPLICIT Signature OPTIONAL }

        TBSRequest      ::=     SEQUENCE {
            version             [0] EXPLICIT Version DEFAULT v1,
            requestorName       [1] EXPLICIT GeneralName OPTIONAL,
            requestList             SEQUENCE OF Request,
            requestExtensions   [2] EXPLICIT Extensions OPTIONAL }

        Signature       ::=     SEQUENCE {
            signatureAlgorithm   AlgorithmIdentifier,
            signature            BIT STRING,
            certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }

        Version  ::=  INTEGER  {  v1(0) }

        Request ::=     SEQUENCE {
            reqCert                    CertID,
            singleRequestExtensions    [0] EXPLICIT Extensions OPTIONAL }

        CertID ::= SEQUENCE {
            hashAlgorithm            AlgorithmIdentifier,
            issuerNameHash     OCTET STRING, -- Hash of Issuer's DN
            issuerKeyHash      OCTET STRING, -- Hash of Issuers public key
            serialNumber       CertificateSerialNumber }

        OCSPResponse ::= SEQUENCE {
           responseStatus         OCSPResponseStatus,
           responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }

        OCSPResponseStatus ::= ENUMERATED {
            successful            (0),      --Response has valid confirmations
            malformedRequest      (1),      --Illegal confirmation request
            internalError         (2),      --Internal error in issuer
            tryLater              (3),      --Try again later
                                            --(4) is not used
            sigRequired           (5),      --Must sign the request
            unauthorized          (6)       --Request unauthorized
        }

        ResponseBytes ::=       SEQUENCE {
            responseType   OBJECT IDENTIFIER,
            response       OCTET STRING }

        BasicOCSPResponse       ::= SEQUENCE {
           tbsResponseData      ResponseData,
           signatureAlgorithm   AlgorithmIdentifier,
           signature            BIT STRING,
           certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }

        ResponseData ::= SEQUENCE {
           version              [0] EXPLICIT Version DEFAULT v1,
           responderID              ResponderID,
           producedAt               GeneralizedTime,
           responses                SEQUENCE OF SingleResponse,
           responseExtensions   [1] EXPLICIT Extensions OPTIONAL }

        ResponderID ::= CHOICE {
           byName   [1] Name,    --EXPLICIT
           byKey    [2] KeyHash }

        KeyHash ::= OCTET STRING --SHA-1 hash of responder's public key
                                 --(excluding the tag and length fields)

        SingleResponse ::= SEQUENCE {
           certID                       CertID,
           certStatus                   CertStatus,
           thisUpdate                   GeneralizedTime,
           nextUpdate           [0]     EXPLICIT GeneralizedTime OPTIONAL,
           singleExtensions     [1]     EXPLICIT Extensions OPTIONAL }

        CertStatus ::= CHOICE {
            good                [0]     IMPLICIT NULL,
            revoked             [1]     IMPLICIT RevokedInfo,
            unknown             [2]     IMPLICIT UnknownInfo }

        RevokedInfo ::= SEQUENCE {
            revocationTime              GeneralizedTime,
            revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }

        UnknownInfo ::= NULL -- this can be replaced with an enumeration

        ArchiveCutoff ::= GeneralizedTime

        AcceptableResponses ::= SEQUENCE OF OBJECT IDENTIFIER

        ServiceLocator ::= SEQUENCE {
            issuer    Name,
            locator   AuthorityInfoAccessSyntax }

        -- Object Identifiers

        id-kp-OCSPSigning            OBJECT IDENTIFIER ::= { id-kp 9 }
        id-pkix-ocsp                 OBJECT IDENTIFIER ::= { id-ad-ocsp }
        id-pkix-ocsp-basic           OBJECT IDENTIFIER ::= { id-pkix-ocsp 1 }
        id-pkix-ocsp-nonce           OBJECT IDENTIFIER ::= { id-pkix-ocsp 2 }
        id-pkix-ocsp-crl             OBJECT IDENTIFIER ::= { id-pkix-ocsp 3 }
        id-pkix-ocsp-response        OBJECT IDENTIFIER ::= { id-pkix-ocsp 4 }
        id-pkix-ocsp-nocheck         OBJECT IDENTIFIER ::= { id-pkix-ocsp 5 }
        id-pkix-ocsp-archive-cutoff  OBJECT IDENTIFIER ::= { id-pkix-ocsp 6 }
        id-pkix-ocsp-service-locator OBJECT IDENTIFIER ::= { id-pkix-ocsp 7 }

*/

/* Request Structures */

DECLARE_STACK_OF(Request)

typedef struct {
    ASN1_INTEGER *version;
    GENERAL_NAME *requestorName;
    STACK_OF(Request) *requestList;
    STACK_OF(X509_EXTENSION) *requestExtensions;
} TBSRequest;

typedef struct {
    X509_ALGOR *signatureAlgorithm;
    ASN1_BIT_STRING *signature;
    STACK_OF(X509) *certs;
} Signature;

typedef struct {
    TBSRequest *tbsRequest;
    Signature *optionalSignature;
} OCSPRequest;

typedef struct {
    X509_ALGOR *hashAlgorithm;
    ASN1_OCTET_STRING *issuerNameHash;
    ASN1_OCTET_STRING *issuerKeyHash;
    ASN1_INTEGER *certificateSerialNumber;
} CertID;

typedef struct {
    CertID *reqCert;
    STACK_OF(X509_EXTENSION) *singleRequestExtensions;
} Request;

/* Response structures */

typedef struct {
    ASN1_OBJECT *responseType;
    ASN1_OCTET_STRING *response;
} ResponseBytes;

typedef struct {
    ASN1_ENUMERATED *responseStatus;
    ResponseBytes *responseBytes;
} OCSPResponse;

typedef struct {
    int type;
    union {
        X509_NAME *byName;
        ASN1_OCTET_STRING *byKey;
    } d;
} ResponderID;

typedef struct {
    ASN1_INTEGER *version;
    ResponderID *responderID;
    ASN1_GENERALIZEDTIME *producedAt;
    STACK_OF(SingleResponse) *responses;
    STACK_OF(X509_EXTENSION) *responseExtensions;
} ResponseData;

typedef struct {
    ResponseData *tbsResponseData;
    X509_ALGOR *signatureAlgorithm;
    ASN1_BIT_STRING *signature;
    STACK_OF(X509) *certs;
} BasicOCSPResponse;

typedef struct {
    ASN1_GENERALIZEDTIME *revocationTime;
    ASN1_ENUMERATED *revocationReason;
} RevokedInfo;

typedef struct {
    int type;
    union {
        ASN1_NULL *good;
        RevokedInfo *revoked;
        ASN1_NULL *unknown;
    } d;
} CertStatus;

typedef struct {
    CertID *certID;
    CertStatus *certStatus;
    ASN1_GENERALIZEDTIME *thisUpdate;
    ASN1_GENERALIZEDTIME *nextUpdate;
    STACK_OF(X509_EXTENSION) *singleExtensions;
} SingleResponse;

typedef struct {
    X509_NAME *issuer;
    STACK_OF(ACCESS_DESCRIPTION) *locator;
} ServiceLocator;

/* Now the ASN1 templates */

IMPLEMENT_COMPAT_ASN1(X509);
IMPLEMENT_COMPAT_ASN1(X509_ALGOR);
// IMPLEMENT_COMPAT_ASN1(X509_EXTENSION);
IMPLEMENT_COMPAT_ASN1(GENERAL_NAME);
IMPLEMENT_COMPAT_ASN1(X509_NAME);

ASN1_SEQUENCE(X509_EXTENSION) = {
        ASN1_SIMPLE(X509_EXTENSION, object, ASN1_OBJECT),
        ASN1_OPT(X509_EXTENSION, critical, ASN1_BOOLEAN),
        ASN1_SIMPLE(X509_EXTENSION, value, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(X509_EXTENSION);


ASN1_SEQUENCE(Signature) = {
        ASN1_SIMPLE(Signature, signatureAlgorithm, X509_ALGOR),
        ASN1_SIMPLE(Signature, signature, ASN1_BIT_STRING),
        ASN1_SEQUENCE_OF(Signature, certs, X509)
} ASN1_SEQUENCE_END(Signature);

ASN1_SEQUENCE(CertID) = {
        ASN1_SIMPLE(CertID, hashAlgorithm, X509_ALGOR),
        ASN1_SIMPLE(CertID, issuerNameHash, ASN1_OCTET_STRING),
        ASN1_SIMPLE(CertID, issuerKeyHash, ASN1_OCTET_STRING),
        ASN1_SIMPLE(CertID, certificateSerialNumber, ASN1_INTEGER)
} ASN1_SEQUENCE_END(CertID);

ASN1_SEQUENCE(Request) = {
        ASN1_SIMPLE(Request, reqCert, CertID),
        ASN1_EXP_SEQUENCE_OF_OPT(Request, singleRequestExtensions, X509_EXTENSION, 0)
} ASN1_SEQUENCE_END(Request);

ASN1_SEQUENCE(TBSRequest) = {
        ASN1_EXP_OPT(TBSRequest, version, ASN1_INTEGER, 0),
        ASN1_EXP_OPT(TBSRequest, requestorName, GENERAL_NAME, 1),
        ASN1_SEQUENCE_OF(TBSRequest, requestList, Request),
        ASN1_EXP_SEQUENCE_OF_OPT(TBSRequest, requestExtensions, X509_EXTENSION, 2)
} ASN1_SEQUENCE_END(TBSRequest);

ASN1_SEQUENCE(OCSPRequest) = {
        ASN1_SIMPLE(OCSPRequest, tbsRequest, TBSRequest),
        ASN1_EXP_OPT(OCSPRequest, optionalSignature, Signature, 0)
} ASN1_SEQUENCE_END(OCSPRequest);

/* Response templates */

ASN1_SEQUENCE(ResponseBytes) = {
            ASN1_SIMPLE(ResponseBytes, responseType, ASN1_OBJECT),
            ASN1_SIMPLE(ResponseBytes, response, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(ResponseBytes);

ASN1_SEQUENCE(OCSPResponse) = {
        ASN1_SIMPLE(OCSPResponse, responseStatus, ASN1_ENUMERATED),
        ASN1_EXP_OPT(OCSPResponse, responseBytes, ResponseBytes, 0)
} ASN1_SEQUENCE_END(OCSPResponse);

ASN1_CHOICE(ResponderID) = {
           ASN1_EXP(ResponderID, d.byName, X509_NAME, 1),
           ASN1_IMP(ResponderID, d.byKey, ASN1_OCTET_STRING, 2)
} ASN1_CHOICE_END(ResponderID);

ASN1_SEQUENCE(RevokedInfo) = {
        ASN1_SIMPLE(RevokedInfo, revocationTime, ASN1_GENERALIZEDTIME),
        ASN1_EXP_OPT(RevokedInfo, revocationReason, ASN1_ENUMERATED, 0)
} ASN1_SEQUENCE_END(RevokedInfo);

ASN1_CHOICE(CertStatus) = {
        ASN1_IMP(CertStatus, d.good, ASN1_NULL, 0),
        ASN1_IMP(CertStatus, d.revoked, RevokedInfo, 1),
        ASN1_IMP(CertStatus, d.unknown, ASN1_NULL, 2)
} ASN1_CHOICE_END(CertStatus);

ASN1_SEQUENCE(SingleResponse) = {
           ASN1_SIMPLE(SingleResponse, certID, CertID),
           ASN1_SIMPLE(SingleResponse, certStatus, CertStatus),
           ASN1_SIMPLE(SingleResponse, thisUpdate, ASN1_GENERALIZEDTIME),
           ASN1_EXP_OPT(SingleResponse, nextUpdate, ASN1_GENERALIZEDTIME, 0),
           ASN1_EXP_SEQUENCE_OF_OPT(SingleResponse, singleExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(SingleResponse);

ASN1_SEQUENCE(ResponseData) = {
           ASN1_EXP_OPT(ResponseData, version, ASN1_INTEGER, 0),
           ASN1_SIMPLE(ResponseData, responderID, ResponderID),
           ASN1_SIMPLE(ResponseData, producedAt, ASN1_GENERALIZEDTIME),
           ASN1_SEQUENCE_OF(ResponseData, responses, SingleResponse),
           ASN1_EXP_SEQUENCE_OF_OPT(ResponseData, responseExtensions, X509_EXTENSION, 1)
} ASN1_SEQUENCE_END(ResponseData);

ASN1_SEQUENCE(BasicOCSPResponse) = {
           ASN1_SIMPLE(BasicOCSPResponse, tbsResponseData, ResponseData),
           ASN1_SIMPLE(BasicOCSPResponse, signatureAlgorithm, X509_ALGOR),
           ASN1_SIMPLE(BasicOCSPResponse, signature, ASN1_BIT_STRING),
           ASN1_EXP_SEQUENCE_OF_OPT(BasicOCSPResponse, certs, X509, 0)
} ASN1_SEQUENCE_END(BasicOCSPResponse);
