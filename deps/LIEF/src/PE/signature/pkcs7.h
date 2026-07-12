/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PKCS7_H
#define LIEF_PKCS7_H

#include "mbedtls/oid.h"

#define MBEDTLS_OID_PKCS7 MBEDTLS_OID_PKCS "\x07" /**< pkcs-7 OBJECT IDENTIFIER ::= { iso(1) member-body(2) US(840) rsadsi(113549) pkcs(1) 7 } */

/*
 * PKCS#7 OIDs
 */
#define MBEDTLS_OID_PKCS7_DATA                      MBEDTLS_OID_PKCS7 "\x01" /**< data OBJECT IDENTIFIER ::= { pkcs-7 1 } */
#define MBEDTLS_OID_PKCS7_SIGNED_DATA               MBEDTLS_OID_PKCS7 "\x02" /**< signedData OBJECT IDENTIFIER ::= { pkcs-7 2 } */
#define MBEDTLS_OID_PKCS7_ENVELOPED_DATA            MBEDTLS_OID_PKCS7 "\x03" /**< data OBJECT IDENTIFIER ::= { pkcs-7 1 } */
#define MBEDTLS_OID_PKCS7_SIGNED_AND_ENVELOPED_DATA MBEDTLS_OID_PKCS7 "\x04" /**< data OBJECT IDENTIFIER ::= { pkcs-7 1 } */
#define MBEDTLS_OID_PKCS7_DIGESTED_DATA             MBEDTLS_OID_PKCS7 "\x05" /**< data OBJECT IDENTIFIER ::= { pkcs-7 1 } */
#define MBEDTLS_OID_PKCS7_ENCRYPTED_DATA            MBEDTLS_OID_PKCS7 "\x06" /**< data OBJECT IDENTIFIER ::= { pkcs-7 1 } */


/*
 * PE Authenticode OID
 */
#define MBEDTLS_SPC_INDIRECT_DATA_OBJID     "\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x04"
#define MBEDTLS_SPC_SP_OPUS_INFO_OBJID      "\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0C"
#define MBEDTLS_SPC_PE_IMAGE_DATAOBJ_OBJID  "\x2B\x06\x01\x04\x01\x82\x37\x02\x01\x0F"

/* MS Specific OID */
#define MS_SZOID_PLATFORM_MANIFEST_BINARY_ID "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x0A\x03\x1C"
#define MS_COUNTER_SIGNATURE                 "\x06\x0A\x2B\x06\x01\x04\x01\x82\x37\x03\x03\x01"

#endif
