/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/ec.h"
#include "internal/der.h"

/* Well known OIDs precompiled */

/*
 * ecdsa-with-SHA1 OBJECT IDENTIFIER ::= { id-ecSigType 1 }
 */
#define DER_OID_V_ecdsa_with_SHA1 DER_P_OBJECT, 7, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x01
#define DER_OID_SZ_ecdsa_with_SHA1 9
extern const unsigned char ossl_der_oid_ecdsa_with_SHA1[DER_OID_SZ_ecdsa_with_SHA1];

/*
 * id-ecPublicKey OBJECT IDENTIFIER ::= { id-publicKeyType 1 }
 */
#define DER_OID_V_id_ecPublicKey DER_P_OBJECT, 7, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01
#define DER_OID_SZ_id_ecPublicKey 9
extern const unsigned char ossl_der_oid_id_ecPublicKey[DER_OID_SZ_id_ecPublicKey];

/*
 * c2pnb163v1  OBJECT IDENTIFIER  ::=  { c-TwoCurve  1 }
 */
#define DER_OID_V_c2pnb163v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x01
#define DER_OID_SZ_c2pnb163v1 10
extern const unsigned char ossl_der_oid_c2pnb163v1[DER_OID_SZ_c2pnb163v1];

/*
 * c2pnb163v2  OBJECT IDENTIFIER  ::=  { c-TwoCurve  2 }
 */
#define DER_OID_V_c2pnb163v2 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x02
#define DER_OID_SZ_c2pnb163v2 10
extern const unsigned char ossl_der_oid_c2pnb163v2[DER_OID_SZ_c2pnb163v2];

/*
 * c2pnb163v3  OBJECT IDENTIFIER  ::=  { c-TwoCurve  3 }
 */
#define DER_OID_V_c2pnb163v3 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x03
#define DER_OID_SZ_c2pnb163v3 10
extern const unsigned char ossl_der_oid_c2pnb163v3[DER_OID_SZ_c2pnb163v3];

/*
 * c2pnb176w1  OBJECT IDENTIFIER  ::=  { c-TwoCurve  4 }
 */
#define DER_OID_V_c2pnb176w1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x04
#define DER_OID_SZ_c2pnb176w1 10
extern const unsigned char ossl_der_oid_c2pnb176w1[DER_OID_SZ_c2pnb176w1];

/*
 * c2tnb191v1  OBJECT IDENTIFIER  ::=  { c-TwoCurve  5 }
 */
#define DER_OID_V_c2tnb191v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x05
#define DER_OID_SZ_c2tnb191v1 10
extern const unsigned char ossl_der_oid_c2tnb191v1[DER_OID_SZ_c2tnb191v1];

/*
 * c2tnb191v2  OBJECT IDENTIFIER  ::=  { c-TwoCurve  6 }
 */
#define DER_OID_V_c2tnb191v2 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x06
#define DER_OID_SZ_c2tnb191v2 10
extern const unsigned char ossl_der_oid_c2tnb191v2[DER_OID_SZ_c2tnb191v2];

/*
 * c2tnb191v3  OBJECT IDENTIFIER  ::=  { c-TwoCurve  7 }
 */
#define DER_OID_V_c2tnb191v3 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x07
#define DER_OID_SZ_c2tnb191v3 10
extern const unsigned char ossl_der_oid_c2tnb191v3[DER_OID_SZ_c2tnb191v3];

/*
 * c2onb191v4  OBJECT IDENTIFIER  ::=  { c-TwoCurve  8 }
 */
#define DER_OID_V_c2onb191v4 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x08
#define DER_OID_SZ_c2onb191v4 10
extern const unsigned char ossl_der_oid_c2onb191v4[DER_OID_SZ_c2onb191v4];

/*
 * c2onb191v5  OBJECT IDENTIFIER  ::=  { c-TwoCurve  9 }
 */
#define DER_OID_V_c2onb191v5 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x09
#define DER_OID_SZ_c2onb191v5 10
extern const unsigned char ossl_der_oid_c2onb191v5[DER_OID_SZ_c2onb191v5];

/*
 * c2pnb208w1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 10 }
 */
#define DER_OID_V_c2pnb208w1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0A
#define DER_OID_SZ_c2pnb208w1 10
extern const unsigned char ossl_der_oid_c2pnb208w1[DER_OID_SZ_c2pnb208w1];

/*
 * c2tnb239v1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 11 }
 */
#define DER_OID_V_c2tnb239v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0B
#define DER_OID_SZ_c2tnb239v1 10
extern const unsigned char ossl_der_oid_c2tnb239v1[DER_OID_SZ_c2tnb239v1];

/*
 * c2tnb239v2  OBJECT IDENTIFIER  ::=  { c-TwoCurve 12 }
 */
#define DER_OID_V_c2tnb239v2 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0C
#define DER_OID_SZ_c2tnb239v2 10
extern const unsigned char ossl_der_oid_c2tnb239v2[DER_OID_SZ_c2tnb239v2];

/*
 * c2tnb239v3  OBJECT IDENTIFIER  ::=  { c-TwoCurve 13 }
 */
#define DER_OID_V_c2tnb239v3 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0D
#define DER_OID_SZ_c2tnb239v3 10
extern const unsigned char ossl_der_oid_c2tnb239v3[DER_OID_SZ_c2tnb239v3];

/*
 * c2onb239v4  OBJECT IDENTIFIER  ::=  { c-TwoCurve 14 }
 */
#define DER_OID_V_c2onb239v4 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0E
#define DER_OID_SZ_c2onb239v4 10
extern const unsigned char ossl_der_oid_c2onb239v4[DER_OID_SZ_c2onb239v4];

/*
 * c2onb239v5  OBJECT IDENTIFIER  ::=  { c-TwoCurve 15 }
 */
#define DER_OID_V_c2onb239v5 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x0F
#define DER_OID_SZ_c2onb239v5 10
extern const unsigned char ossl_der_oid_c2onb239v5[DER_OID_SZ_c2onb239v5];

/*
 * c2pnb272w1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 16 }
 */
#define DER_OID_V_c2pnb272w1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x10
#define DER_OID_SZ_c2pnb272w1 10
extern const unsigned char ossl_der_oid_c2pnb272w1[DER_OID_SZ_c2pnb272w1];

/*
 * c2pnb304w1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 17 }
 */
#define DER_OID_V_c2pnb304w1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x11
#define DER_OID_SZ_c2pnb304w1 10
extern const unsigned char ossl_der_oid_c2pnb304w1[DER_OID_SZ_c2pnb304w1];

/*
 * c2tnb359v1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 18 }
 */
#define DER_OID_V_c2tnb359v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x12
#define DER_OID_SZ_c2tnb359v1 10
extern const unsigned char ossl_der_oid_c2tnb359v1[DER_OID_SZ_c2tnb359v1];

/*
 * c2pnb368w1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 19 }
 */
#define DER_OID_V_c2pnb368w1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x13
#define DER_OID_SZ_c2pnb368w1 10
extern const unsigned char ossl_der_oid_c2pnb368w1[DER_OID_SZ_c2pnb368w1];

/*
 * c2tnb431r1  OBJECT IDENTIFIER  ::=  { c-TwoCurve 20 }
 */
#define DER_OID_V_c2tnb431r1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x00, 0x14
#define DER_OID_SZ_c2tnb431r1 10
extern const unsigned char ossl_der_oid_c2tnb431r1[DER_OID_SZ_c2tnb431r1];

/*
 * prime192v1  OBJECT IDENTIFIER  ::=  { primeCurve  1 }
 */
#define DER_OID_V_prime192v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x01
#define DER_OID_SZ_prime192v1 10
extern const unsigned char ossl_der_oid_prime192v1[DER_OID_SZ_prime192v1];

/*
 * prime192v2  OBJECT IDENTIFIER  ::=  { primeCurve  2 }
 */
#define DER_OID_V_prime192v2 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x02
#define DER_OID_SZ_prime192v2 10
extern const unsigned char ossl_der_oid_prime192v2[DER_OID_SZ_prime192v2];

/*
 * prime192v3  OBJECT IDENTIFIER  ::=  { primeCurve  3 }
 */
#define DER_OID_V_prime192v3 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x03
#define DER_OID_SZ_prime192v3 10
extern const unsigned char ossl_der_oid_prime192v3[DER_OID_SZ_prime192v3];

/*
 * prime239v1  OBJECT IDENTIFIER  ::=  { primeCurve  4 }
 */
#define DER_OID_V_prime239v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x04
#define DER_OID_SZ_prime239v1 10
extern const unsigned char ossl_der_oid_prime239v1[DER_OID_SZ_prime239v1];

/*
 * prime239v2  OBJECT IDENTIFIER  ::=  { primeCurve  5 }
 */
#define DER_OID_V_prime239v2 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x05
#define DER_OID_SZ_prime239v2 10
extern const unsigned char ossl_der_oid_prime239v2[DER_OID_SZ_prime239v2];

/*
 * prime239v3  OBJECT IDENTIFIER  ::=  { primeCurve  6 }
 */
#define DER_OID_V_prime239v3 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x06
#define DER_OID_SZ_prime239v3 10
extern const unsigned char ossl_der_oid_prime239v3[DER_OID_SZ_prime239v3];

/*
 * prime256v1  OBJECT IDENTIFIER  ::=  { primeCurve  7 }
 */
#define DER_OID_V_prime256v1 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07
#define DER_OID_SZ_prime256v1 10
extern const unsigned char ossl_der_oid_prime256v1[DER_OID_SZ_prime256v1];

/*
 * ecdsa-with-SHA224 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
 *      us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 1 }
 */
#define DER_OID_V_ecdsa_with_SHA224 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x01
#define DER_OID_SZ_ecdsa_with_SHA224 10
extern const unsigned char ossl_der_oid_ecdsa_with_SHA224[DER_OID_SZ_ecdsa_with_SHA224];

/*
 * ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
 *      us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
 */
#define DER_OID_V_ecdsa_with_SHA256 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02
#define DER_OID_SZ_ecdsa_with_SHA256 10
extern const unsigned char ossl_der_oid_ecdsa_with_SHA256[DER_OID_SZ_ecdsa_with_SHA256];

/*
 * ecdsa-with-SHA384 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
 *      us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 3 }
 */
#define DER_OID_V_ecdsa_with_SHA384 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03
#define DER_OID_SZ_ecdsa_with_SHA384 10
extern const unsigned char ossl_der_oid_ecdsa_with_SHA384[DER_OID_SZ_ecdsa_with_SHA384];

/*
 * ecdsa-with-SHA512 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
 *      us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 4 }
 */
#define DER_OID_V_ecdsa_with_SHA512 DER_P_OBJECT, 8, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x04
#define DER_OID_SZ_ecdsa_with_SHA512 10
extern const unsigned char ossl_der_oid_ecdsa_with_SHA512[DER_OID_SZ_ecdsa_with_SHA512];

/*
 * id-ecdsa-with-sha3-224 OBJECT IDENTIFIER ::= { sigAlgs 9 }
 */
#define DER_OID_V_id_ecdsa_with_sha3_224 DER_P_OBJECT, 9, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x09
#define DER_OID_SZ_id_ecdsa_with_sha3_224 11
extern const unsigned char ossl_der_oid_id_ecdsa_with_sha3_224[DER_OID_SZ_id_ecdsa_with_sha3_224];

/*
 * id-ecdsa-with-sha3-256 OBJECT IDENTIFIER ::= { sigAlgs 10 }
 */
#define DER_OID_V_id_ecdsa_with_sha3_256 DER_P_OBJECT, 9, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0A
#define DER_OID_SZ_id_ecdsa_with_sha3_256 11
extern const unsigned char ossl_der_oid_id_ecdsa_with_sha3_256[DER_OID_SZ_id_ecdsa_with_sha3_256];

/*
 * id-ecdsa-with-sha3-384 OBJECT IDENTIFIER ::= { sigAlgs 11 }
 */
#define DER_OID_V_id_ecdsa_with_sha3_384 DER_P_OBJECT, 9, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0B
#define DER_OID_SZ_id_ecdsa_with_sha3_384 11
extern const unsigned char ossl_der_oid_id_ecdsa_with_sha3_384[DER_OID_SZ_id_ecdsa_with_sha3_384];

/*
 * id-ecdsa-with-sha3-512 OBJECT IDENTIFIER ::= { sigAlgs 12 }
 */
#define DER_OID_V_id_ecdsa_with_sha3_512 DER_P_OBJECT, 9, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x0C
#define DER_OID_SZ_id_ecdsa_with_sha3_512 11
extern const unsigned char ossl_der_oid_id_ecdsa_with_sha3_512[DER_OID_SZ_id_ecdsa_with_sha3_512];


/* Subject Public Key Info */
int ossl_DER_w_algorithmIdentifier_EC(WPACKET *pkt, int cont, EC_KEY *ec);
/* Signature */
int ossl_DER_w_algorithmIdentifier_ECDSA_with_MD(WPACKET *pkt, int cont,
                                                 EC_KEY *ec, int mdnid);
