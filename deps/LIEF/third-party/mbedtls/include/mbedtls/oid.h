/**
 * \file oid.h
 *
 * \brief Object Identifier (OID) values
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_OID_H
#define MBEDTLS_OID_H

#include "mbedtls/build_info.h"
#include "mbedtls/asn1.h"

/*
 * Top level OID tuples
 */
#define MBEDTLS_OID_ISO_MEMBER_BODIES           "\x2a"          /* {iso(1) member-body(2)} */
#define MBEDTLS_OID_ISO_IDENTIFIED_ORG          "\x2b"          /* {iso(1) identified-organization(3)} */
#define MBEDTLS_OID_ISO_CCITT_DS                "\x55"          /* {joint-iso-ccitt(2) ds(5)} */
#define MBEDTLS_OID_ISO_ITU_COUNTRY             "\x60"          /* {joint-iso-itu-t(2) country(16)} */

/*
 * ISO Member bodies OID parts
 */
#define MBEDTLS_OID_COUNTRY_US                  "\x86\x48"      /* {us(840)} */
#define MBEDTLS_OID_ORG_RSA_DATA_SECURITY       "\x86\xf7\x0d"  /* {rsadsi(113549)} */
#define MBEDTLS_OID_RSA_COMPANY                 MBEDTLS_OID_ISO_MEMBER_BODIES MBEDTLS_OID_COUNTRY_US \
        MBEDTLS_OID_ORG_RSA_DATA_SECURITY                                 /* {iso(1) member-body(2) us(840) rsadsi(113549)} */
#define MBEDTLS_OID_ORG_ANSI_X9_62              "\xce\x3d" /* ansi-X9-62(10045) */
#define MBEDTLS_OID_ANSI_X9_62                  MBEDTLS_OID_ISO_MEMBER_BODIES MBEDTLS_OID_COUNTRY_US \
        MBEDTLS_OID_ORG_ANSI_X9_62

/*
 * ISO Identified organization OID parts
 */
#define MBEDTLS_OID_ORG_DOD                     "\x06"          /* {dod(6)} */
#define MBEDTLS_OID_ORG_OIW                     "\x0e"
#define MBEDTLS_OID_OIW_SECSIG                  MBEDTLS_OID_ORG_OIW "\x03"
#define MBEDTLS_OID_OIW_SECSIG_ALG              MBEDTLS_OID_OIW_SECSIG "\x02"
#define MBEDTLS_OID_OIW_SECSIG_SHA1             MBEDTLS_OID_OIW_SECSIG_ALG "\x1a"
#define MBEDTLS_OID_ORG_THAWTE                  "\x65"          /* thawte(101) */
#define MBEDTLS_OID_THAWTE                      MBEDTLS_OID_ISO_IDENTIFIED_ORG \
        MBEDTLS_OID_ORG_THAWTE
#define MBEDTLS_OID_ORG_CERTICOM                "\x81\x04"  /* certicom(132) */
#define MBEDTLS_OID_CERTICOM                    MBEDTLS_OID_ISO_IDENTIFIED_ORG \
        MBEDTLS_OID_ORG_CERTICOM
#define MBEDTLS_OID_ORG_TELETRUST               "\x24" /* teletrust(36) */
#define MBEDTLS_OID_TELETRUST                   MBEDTLS_OID_ISO_IDENTIFIED_ORG \
        MBEDTLS_OID_ORG_TELETRUST

/*
 * ISO ITU OID parts
 */
#define MBEDTLS_OID_ORGANIZATION                "\x01"          /* {organization(1)} */
#define MBEDTLS_OID_ISO_ITU_US_ORG              MBEDTLS_OID_ISO_ITU_COUNTRY MBEDTLS_OID_COUNTRY_US \
        MBEDTLS_OID_ORGANIZATION                                                                                            /* {joint-iso-itu-t(2) country(16) us(840) organization(1)} */

#define MBEDTLS_OID_ORG_GOV                     "\x65"          /* {gov(101)} */
#define MBEDTLS_OID_GOV                         MBEDTLS_OID_ISO_ITU_US_ORG MBEDTLS_OID_ORG_GOV /* {joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101)} */

#define MBEDTLS_OID_ORG_NETSCAPE                "\x86\xF8\x42"  /* {netscape(113730)} */
#define MBEDTLS_OID_NETSCAPE                    MBEDTLS_OID_ISO_ITU_US_ORG MBEDTLS_OID_ORG_NETSCAPE /* Netscape OID {joint-iso-itu-t(2) country(16) us(840) organization(1) netscape(113730)} */

/* ISO arc for standard certificate and CRL extensions */
#define MBEDTLS_OID_ID_CE                       MBEDTLS_OID_ISO_CCITT_DS "\x1D" /**< id-ce OBJECT IDENTIFIER  ::=  {joint-iso-ccitt(2) ds(5) 29} */

#define MBEDTLS_OID_NIST_ALG                    MBEDTLS_OID_GOV "\x03\x04" /** { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistAlgorithm(4) */

/**
 * Private Internet Extensions
 * { iso(1) identified-organization(3) dod(6) internet(1)
 *                      security(5) mechanisms(5) pkix(7) }
 */
#define MBEDTLS_OID_INTERNET                    MBEDTLS_OID_ISO_IDENTIFIED_ORG MBEDTLS_OID_ORG_DOD \
    "\x01"
#define MBEDTLS_OID_PKIX                        MBEDTLS_OID_INTERNET "\x05\x05\x07"

/*
 * Arc for standard naming attributes
 */
#define MBEDTLS_OID_AT                          MBEDTLS_OID_ISO_CCITT_DS "\x04" /**< id-at OBJECT IDENTIFIER ::= {joint-iso-ccitt(2) ds(5) 4} */
#define MBEDTLS_OID_AT_CN                       MBEDTLS_OID_AT "\x03" /**< id-at-commonName AttributeType:= {id-at 3} */
#define MBEDTLS_OID_AT_SUR_NAME                 MBEDTLS_OID_AT "\x04" /**< id-at-surName AttributeType:= {id-at 4} */
#define MBEDTLS_OID_AT_SERIAL_NUMBER            MBEDTLS_OID_AT "\x05" /**< id-at-serialNumber AttributeType:= {id-at 5} */
#define MBEDTLS_OID_AT_COUNTRY                  MBEDTLS_OID_AT "\x06" /**< id-at-countryName AttributeType:= {id-at 6} */
#define MBEDTLS_OID_AT_LOCALITY                 MBEDTLS_OID_AT "\x07" /**< id-at-locality AttributeType:= {id-at 7} */
#define MBEDTLS_OID_AT_STATE                    MBEDTLS_OID_AT "\x08" /**< id-at-state AttributeType:= {id-at 8} */
#define MBEDTLS_OID_AT_ORGANIZATION             MBEDTLS_OID_AT "\x0A" /**< id-at-organizationName AttributeType:= {id-at 10} */
#define MBEDTLS_OID_AT_ORG_UNIT                 MBEDTLS_OID_AT "\x0B" /**< id-at-organizationalUnitName AttributeType:= {id-at 11} */
#define MBEDTLS_OID_AT_TITLE                    MBEDTLS_OID_AT "\x0C" /**< id-at-title AttributeType:= {id-at 12} */
#define MBEDTLS_OID_AT_POSTAL_ADDRESS           MBEDTLS_OID_AT "\x10" /**< id-at-postalAddress AttributeType:= {id-at 16} */
#define MBEDTLS_OID_AT_POSTAL_CODE              MBEDTLS_OID_AT "\x11" /**< id-at-postalCode AttributeType:= {id-at 17} */
#define MBEDTLS_OID_AT_GIVEN_NAME               MBEDTLS_OID_AT "\x2A" /**< id-at-givenName AttributeType:= {id-at 42} */
#define MBEDTLS_OID_AT_INITIALS                 MBEDTLS_OID_AT "\x2B" /**< id-at-initials AttributeType:= {id-at 43} */
#define MBEDTLS_OID_AT_GENERATION_QUALIFIER     MBEDTLS_OID_AT "\x2C" /**< id-at-generationQualifier AttributeType:= {id-at 44} */
#define MBEDTLS_OID_AT_UNIQUE_IDENTIFIER        MBEDTLS_OID_AT "\x2D" /**< id-at-uniqueIdentifier AttributeType:= {id-at 45} */
#define MBEDTLS_OID_AT_DN_QUALIFIER             MBEDTLS_OID_AT "\x2E" /**< id-at-dnQualifier AttributeType:= {id-at 46} */
#define MBEDTLS_OID_AT_PSEUDONYM                MBEDTLS_OID_AT "\x41" /**< id-at-pseudonym AttributeType:= {id-at 65} */

#define MBEDTLS_OID_UID                         "\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x01" /** id-domainComponent AttributeType:= {itu-t(0) data(9) pss(2342) ucl(19200300) pilot(100) pilotAttributeType(1) uid(1)} */
#define MBEDTLS_OID_DOMAIN_COMPONENT            "\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x19" /** id-domainComponent AttributeType:= {itu-t(0) data(9) pss(2342) ucl(19200300) pilot(100) pilotAttributeType(1) domainComponent(25)} */

/*
 * OIDs for standard certificate extensions
 */
#define MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER    MBEDTLS_OID_ID_CE "\x23" /**< id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 } */
#define MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER      MBEDTLS_OID_ID_CE "\x0E" /**< id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 } */
#define MBEDTLS_OID_KEY_USAGE                   MBEDTLS_OID_ID_CE "\x0F" /**< id-ce-keyUsage OBJECT IDENTIFIER ::=  { id-ce 15 } */
#define MBEDTLS_OID_CERTIFICATE_POLICIES        MBEDTLS_OID_ID_CE "\x20" /**< id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 } */
#define MBEDTLS_OID_POLICY_MAPPINGS             MBEDTLS_OID_ID_CE "\x21" /**< id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 } */
#define MBEDTLS_OID_SUBJECT_ALT_NAME            MBEDTLS_OID_ID_CE "\x11" /**< id-ce-subjectAltName OBJECT IDENTIFIER ::=  { id-ce 17 } */
#define MBEDTLS_OID_ISSUER_ALT_NAME             MBEDTLS_OID_ID_CE "\x12" /**< id-ce-issuerAltName OBJECT IDENTIFIER ::=  { id-ce 18 } */
#define MBEDTLS_OID_SUBJECT_DIRECTORY_ATTRS     MBEDTLS_OID_ID_CE "\x09" /**< id-ce-subjectDirectoryAttributes OBJECT IDENTIFIER ::=  { id-ce 9 } */
#define MBEDTLS_OID_BASIC_CONSTRAINTS           MBEDTLS_OID_ID_CE "\x13" /**< id-ce-basicConstraints OBJECT IDENTIFIER ::=  { id-ce 19 } */
#define MBEDTLS_OID_NAME_CONSTRAINTS            MBEDTLS_OID_ID_CE "\x1E" /**< id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 } */
#define MBEDTLS_OID_POLICY_CONSTRAINTS          MBEDTLS_OID_ID_CE "\x24" /**< id-ce-policyConstraints OBJECT IDENTIFIER ::=  { id-ce 36 } */
#define MBEDTLS_OID_EXTENDED_KEY_USAGE          MBEDTLS_OID_ID_CE "\x25" /**< id-ce-extKeyUsage OBJECT IDENTIFIER ::= { id-ce 37 } */
#define MBEDTLS_OID_CRL_DISTRIBUTION_POINTS     MBEDTLS_OID_ID_CE "\x1F" /**< id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 } */
#define MBEDTLS_OID_INIHIBIT_ANYPOLICY          MBEDTLS_OID_ID_CE "\x36" /**< id-ce-inhibitAnyPolicy OBJECT IDENTIFIER ::=  { id-ce 54 } */
#define MBEDTLS_OID_FRESHEST_CRL                MBEDTLS_OID_ID_CE "\x2E" /**< id-ce-freshestCRL OBJECT IDENTIFIER ::=  { id-ce 46 } */

/*
 * Certificate policies
 */
#define MBEDTLS_OID_ANY_POLICY              MBEDTLS_OID_CERTIFICATE_POLICIES "\x00" /**< anyPolicy OBJECT IDENTIFIER ::= { id-ce-certificatePolicies 0 } */

/*
 * Netscape certificate extensions
 */
#define MBEDTLS_OID_NS_CERT                 MBEDTLS_OID_NETSCAPE "\x01"
#define MBEDTLS_OID_NS_CERT_TYPE            MBEDTLS_OID_NS_CERT  "\x01"
#define MBEDTLS_OID_NS_BASE_URL             MBEDTLS_OID_NS_CERT  "\x02"
#define MBEDTLS_OID_NS_REVOCATION_URL       MBEDTLS_OID_NS_CERT  "\x03"
#define MBEDTLS_OID_NS_CA_REVOCATION_URL    MBEDTLS_OID_NS_CERT  "\x04"
#define MBEDTLS_OID_NS_RENEWAL_URL          MBEDTLS_OID_NS_CERT  "\x07"
#define MBEDTLS_OID_NS_CA_POLICY_URL        MBEDTLS_OID_NS_CERT  "\x08"
#define MBEDTLS_OID_NS_SSL_SERVER_NAME      MBEDTLS_OID_NS_CERT  "\x0C"
#define MBEDTLS_OID_NS_COMMENT              MBEDTLS_OID_NS_CERT  "\x0D"
#define MBEDTLS_OID_NS_DATA_TYPE            MBEDTLS_OID_NETSCAPE "\x02"
#define MBEDTLS_OID_NS_CERT_SEQUENCE        MBEDTLS_OID_NS_DATA_TYPE "\x05"

/*
 * OIDs for CRL extensions
 */
#define MBEDTLS_OID_PRIVATE_KEY_USAGE_PERIOD    MBEDTLS_OID_ID_CE "\x10"
#define MBEDTLS_OID_CRL_NUMBER                  MBEDTLS_OID_ID_CE "\x14" /**< id-ce-cRLNumber OBJECT IDENTIFIER ::= { id-ce 20 } */

/*
 * X.509 v3 Extended key usage OIDs
 */
#define MBEDTLS_OID_ANY_EXTENDED_KEY_USAGE      MBEDTLS_OID_EXTENDED_KEY_USAGE "\x00" /**< anyExtendedKeyUsage OBJECT IDENTIFIER ::= { id-ce-extKeyUsage 0 } */

#define MBEDTLS_OID_KP                          MBEDTLS_OID_PKIX "\x03" /**< id-kp OBJECT IDENTIFIER ::= { id-pkix 3 } */
#define MBEDTLS_OID_SERVER_AUTH                 MBEDTLS_OID_KP "\x01" /**< id-kp-serverAuth OBJECT IDENTIFIER ::= { id-kp 1 } */
#define MBEDTLS_OID_CLIENT_AUTH                 MBEDTLS_OID_KP "\x02" /**< id-kp-clientAuth OBJECT IDENTIFIER ::= { id-kp 2 } */
#define MBEDTLS_OID_CODE_SIGNING                MBEDTLS_OID_KP "\x03" /**< id-kp-codeSigning OBJECT IDENTIFIER ::= { id-kp 3 } */
#define MBEDTLS_OID_EMAIL_PROTECTION            MBEDTLS_OID_KP "\x04" /**< id-kp-emailProtection OBJECT IDENTIFIER ::= { id-kp 4 } */
#define MBEDTLS_OID_TIME_STAMPING               MBEDTLS_OID_KP "\x08" /**< id-kp-timeStamping OBJECT IDENTIFIER ::= { id-kp 8 } */
#define MBEDTLS_OID_OCSP_SIGNING                MBEDTLS_OID_KP "\x09" /**< id-kp-OCSPSigning OBJECT IDENTIFIER ::= { id-kp 9 } */

/**
 * Wi-SUN Alliance Field Area Network
 * { iso(1) identified-organization(3) dod(6) internet(1)
 *                      private(4) enterprise(1) WiSUN(45605) FieldAreaNetwork(1) }
 */
#define MBEDTLS_OID_WISUN_FAN                   MBEDTLS_OID_INTERNET "\x04\x01\x82\xe4\x25\x01"

#define MBEDTLS_OID_ON                          MBEDTLS_OID_PKIX "\x08" /**< id-on OBJECT IDENTIFIER ::= { id-pkix 8 } */
#define MBEDTLS_OID_ON_HW_MODULE_NAME           MBEDTLS_OID_ON "\x04" /**< id-on-hardwareModuleName OBJECT IDENTIFIER ::= { id-on 4 } */

/*
 * PKCS definition OIDs
 */

#define MBEDTLS_OID_PKCS                MBEDTLS_OID_RSA_COMPANY "\x01" /**< pkcs OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) 1 } */
#define MBEDTLS_OID_PKCS1               MBEDTLS_OID_PKCS "\x01" /**< pkcs-1 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 1 } */
#define MBEDTLS_OID_PKCS5               MBEDTLS_OID_PKCS "\x05" /**< pkcs-5 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 5 } */
#define MBEDTLS_OID_PKCS7               MBEDTLS_OID_PKCS "\x07" /**< pkcs-7 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 7 } */
#define MBEDTLS_OID_PKCS9               MBEDTLS_OID_PKCS "\x09" /**< pkcs-9 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 9 } */
#define MBEDTLS_OID_PKCS12              MBEDTLS_OID_PKCS "\x0c" /**< pkcs-12 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 12 } */

/*
 * PKCS#1 OIDs
 */
#define MBEDTLS_OID_PKCS1_MD5           MBEDTLS_OID_PKCS1 "\x04" /**< md5WithRSAEncryption ::= { pkcs-1 4 } */
#define MBEDTLS_OID_PKCS1_SHA1          MBEDTLS_OID_PKCS1 "\x05" /**< sha1WithRSAEncryption ::= { pkcs-1 5 } */
#define MBEDTLS_OID_PKCS1_SHA224        MBEDTLS_OID_PKCS1 "\x0e" /**< sha224WithRSAEncryption ::= { pkcs-1 14 } */
#define MBEDTLS_OID_PKCS1_SHA256        MBEDTLS_OID_PKCS1 "\x0b" /**< sha256WithRSAEncryption ::= { pkcs-1 11 } */
#define MBEDTLS_OID_PKCS1_SHA384        MBEDTLS_OID_PKCS1 "\x0c" /**< sha384WithRSAEncryption ::= { pkcs-1 12 } */
#define MBEDTLS_OID_PKCS1_SHA512        MBEDTLS_OID_PKCS1 "\x0d" /**< sha512WithRSAEncryption ::= { pkcs-1 13 } */

#define MBEDTLS_OID_RSA_SHA_OBS         "\x2B\x0E\x03\x02\x1D"

#define MBEDTLS_OID_PKCS9_EMAIL         MBEDTLS_OID_PKCS9 "\x01" /**< emailAddress AttributeType ::= { pkcs-9 1 } */

/* RFC 4055 */
#define MBEDTLS_OID_RSASSA_PSS          MBEDTLS_OID_PKCS1 "\x0a" /**< id-RSASSA-PSS ::= { pkcs-1 10 } */
#define MBEDTLS_OID_MGF1                MBEDTLS_OID_PKCS1 "\x08" /**< id-mgf1 ::= { pkcs-1 8 } */

/*
 * Digest algorithms
 */
#define MBEDTLS_OID_DIGEST_ALG_MD5              MBEDTLS_OID_RSA_COMPANY "\x02\x05" /**< id-mbedtls_md5 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) digestAlgorithm(2) 5 } */
#define MBEDTLS_OID_DIGEST_ALG_SHA1             MBEDTLS_OID_ISO_IDENTIFIED_ORG \
        MBEDTLS_OID_OIW_SECSIG_SHA1                                                                        /**< id-mbedtls_sha1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3) oiw(14) secsig(3) algorithms(2) 26 } */
#define MBEDTLS_OID_DIGEST_ALG_SHA224           MBEDTLS_OID_NIST_ALG "\x02\x04" /**< id-sha224 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistalgorithm(4) hashalgs(2) 4 } */
#define MBEDTLS_OID_DIGEST_ALG_SHA256           MBEDTLS_OID_NIST_ALG "\x02\x01" /**< id-mbedtls_sha256 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistalgorithm(4) hashalgs(2) 1 } */

#define MBEDTLS_OID_DIGEST_ALG_SHA384           MBEDTLS_OID_NIST_ALG "\x02\x02" /**< id-sha384 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistalgorithm(4) hashalgs(2) 2 } */

#define MBEDTLS_OID_DIGEST_ALG_SHA512           MBEDTLS_OID_NIST_ALG "\x02\x03" /**< id-mbedtls_sha512 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistalgorithm(4) hashalgs(2) 3 } */

#define MBEDTLS_OID_DIGEST_ALG_RIPEMD160        MBEDTLS_OID_TELETRUST "\x03\x02\x01" /**< id-ripemd160 OBJECT IDENTIFIER :: { iso(1) identified-organization(3) teletrust(36) algorithm(3) hashAlgorithm(2) ripemd160(1) } */

#define MBEDTLS_OID_DIGEST_ALG_SHA3_224         MBEDTLS_OID_NIST_ALG "\x02\x07" /**< id-sha3-224 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistAlgorithms(4) hashalgs(2) sha3-224(7) } */

#define MBEDTLS_OID_DIGEST_ALG_SHA3_256         MBEDTLS_OID_NIST_ALG "\x02\x08" /**< id-sha3-256 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistAlgorithms(4) hashalgs(2) sha3-256(8) } */

#define MBEDTLS_OID_DIGEST_ALG_SHA3_384         MBEDTLS_OID_NIST_ALG "\x02\x09" /**< id-sha3-384 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistAlgorithms(4) hashalgs(2) sha3-384(9) } */

#define MBEDTLS_OID_DIGEST_ALG_SHA3_512         MBEDTLS_OID_NIST_ALG "\x02\x0a" /**< id-sha3-512 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840) organization(1) gov(101) csor(3) nistAlgorithms(4) hashalgs(2) sha3-512(10) } */

/*
 * PKCS#7 OIDs
 */
#define MBEDTLS_OID_PKCS7_DATA                        MBEDTLS_OID_PKCS7 "\x01" /**< Content type is Data OBJECT IDENTIFIER ::= {pkcs-7 1} */
#define MBEDTLS_OID_PKCS7_SIGNED_DATA                 MBEDTLS_OID_PKCS7 "\x02" /**< Content type is Signed Data OBJECT IDENTIFIER ::= {pkcs-7 2} */
#define MBEDTLS_OID_PKCS7_ENVELOPED_DATA              MBEDTLS_OID_PKCS7 "\x03" /**< Content type is Enveloped Data OBJECT IDENTIFIER ::= {pkcs-7 3} */
#define MBEDTLS_OID_PKCS7_SIGNED_AND_ENVELOPED_DATA   MBEDTLS_OID_PKCS7 "\x04" /**< Content type is Signed and Enveloped Data OBJECT IDENTIFIER ::= {pkcs-7 4} */
#define MBEDTLS_OID_PKCS7_DIGESTED_DATA               MBEDTLS_OID_PKCS7 "\x05" /**< Content type is Digested Data OBJECT IDENTIFIER ::= {pkcs-7 5} */
#define MBEDTLS_OID_PKCS7_ENCRYPTED_DATA              MBEDTLS_OID_PKCS7 "\x06" /**< Content type is Encrypted Data OBJECT IDENTIFIER ::= {pkcs-7 6} */

#define MBEDTLS_OID_PKCS9_CSR_EXT_REQ           MBEDTLS_OID_PKCS9 "\x0e" /**< extensionRequest OBJECT IDENTIFIER ::= {pkcs-9 14} */


/*
 * ECDSA signature identifiers, from RFC 5480
 */
#define MBEDTLS_OID_ANSI_X9_62_SIG          MBEDTLS_OID_ANSI_X9_62 "\x04" /* signatures(4) */
#define MBEDTLS_OID_ANSI_X9_62_SIG_SHA2     MBEDTLS_OID_ANSI_X9_62_SIG "\x03" /* ecdsa-with-SHA2(3) */

/* ecdsa-with-SHA1 OBJECT IDENTIFIER ::= {
 *   iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4) 1 } */
#define MBEDTLS_OID_ECDSA_SHA1              MBEDTLS_OID_ANSI_X9_62_SIG "\x01"

/* ecdsa-with-SHA224 OBJECT IDENTIFIER ::= {
 *   iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
 *   ecdsa-with-SHA2(3) 1 } */
#define MBEDTLS_OID_ECDSA_SHA224            MBEDTLS_OID_ANSI_X9_62_SIG_SHA2 "\x01"

/* ecdsa-with-SHA256 OBJECT IDENTIFIER ::= {
 *   iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
 *   ecdsa-with-SHA2(3) 2 } */
#define MBEDTLS_OID_ECDSA_SHA256            MBEDTLS_OID_ANSI_X9_62_SIG_SHA2 "\x02"

/* ecdsa-with-SHA384 OBJECT IDENTIFIER ::= {
 *   iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
 *   ecdsa-with-SHA2(3) 3 } */
#define MBEDTLS_OID_ECDSA_SHA384            MBEDTLS_OID_ANSI_X9_62_SIG_SHA2 "\x03"

/* ecdsa-with-SHA512 OBJECT IDENTIFIER ::= {
 *   iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
 *   ecdsa-with-SHA2(3) 4 } */
#define MBEDTLS_OID_ECDSA_SHA512            MBEDTLS_OID_ANSI_X9_62_SIG_SHA2 "\x04"

#if defined(MBEDTLS_X509_USE_C)
/**
 * \brief           Translate an ASN.1 OID into its numeric representation
 *                  (e.g. "\x2A\x86\x48\x86\xF7\x0D" into "1.2.840.113549")
 *
 * \param buf       buffer to put representation in
 * \param size      size of the buffer
 * \param oid       OID to translate
 *
 * \return          Length of the string written (excluding final NULL) or
 *                  PSA_ERROR_BUFFER_TOO_SMALL in case of error
 */
int mbedtls_oid_get_numeric_string(char *buf, size_t size, const mbedtls_asn1_buf *oid);
#endif /* MBEDTLS_X509_USE_C */

#if defined(MBEDTLS_X509_CREATE_C)
/**
 * \brief           Translate a string containing a dotted-decimal
 *                  representation of an ASN.1 OID into its encoded form
 *                  (e.g. "1.2.840.113549" into "\x2A\x86\x48\x86\xF7\x0D").
 *                  On success, this function allocates oid->buf from the
 *                  heap. It must be freed by the caller using mbedtls_free().
 *
 * \param oid       #mbedtls_asn1_buf to populate with the DER-encoded OID
 * \param oid_str   string representation of the OID to parse
 * \param size      length of the OID string, not including any null terminator
 *
 * \return          0 if successful
 * \return          #MBEDTLS_ERR_ASN1_INVALID_DATA if \p oid_str does not
 *                  represent a valid OID
 * \return          #MBEDTLS_ERR_ASN1_ALLOC_FAILED if the function fails to
 *                  allocate oid->buf
 */
int mbedtls_oid_from_numeric_string(mbedtls_asn1_buf *oid, const char *oid_str, size_t size);
#endif /* MBEDTLS_X509_CREATE_C */

#endif /* oid.h */
