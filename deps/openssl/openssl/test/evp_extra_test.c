/*
 * Copyright 2015-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/kdf.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include "testutil.h"
#include "internal/nelem.h"
#include "crypto/evp.h"

/*
 * kExampleRSAKeyDER is an RSA private key in ASN.1, DER format. Of course, you
 * should never use this key anywhere but in an example.
 */
static const unsigned char kExampleRSAKeyDER[] = {
    0x30, 0x82, 0x02, 0x5c, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xf8,
    0xb8, 0x6c, 0x83, 0xb4, 0xbc, 0xd9, 0xa8, 0x57, 0xc0, 0xa5, 0xb4, 0x59,
    0x76, 0x8c, 0x54, 0x1d, 0x79, 0xeb, 0x22, 0x52, 0x04, 0x7e, 0xd3, 0x37,
    0xeb, 0x41, 0xfd, 0x83, 0xf9, 0xf0, 0xa6, 0x85, 0x15, 0x34, 0x75, 0x71,
    0x5a, 0x84, 0xa8, 0x3c, 0xd2, 0xef, 0x5a, 0x4e, 0xd3, 0xde, 0x97, 0x8a,
    0xdd, 0xff, 0xbb, 0xcf, 0x0a, 0xaa, 0x86, 0x92, 0xbe, 0xb8, 0x50, 0xe4,
    0xcd, 0x6f, 0x80, 0x33, 0x30, 0x76, 0x13, 0x8f, 0xca, 0x7b, 0xdc, 0xec,
    0x5a, 0xca, 0x63, 0xc7, 0x03, 0x25, 0xef, 0xa8, 0x8a, 0x83, 0x58, 0x76,
    0x20, 0xfa, 0x16, 0x77, 0xd7, 0x79, 0x92, 0x63, 0x01, 0x48, 0x1a, 0xd8,
    0x7b, 0x67, 0xf1, 0x52, 0x55, 0x49, 0x4e, 0xd6, 0x6e, 0x4a, 0x5c, 0xd7,
    0x7a, 0x37, 0x36, 0x0c, 0xde, 0xdd, 0x8f, 0x44, 0xe8, 0xc2, 0xa7, 0x2c,
    0x2b, 0xb5, 0xaf, 0x64, 0x4b, 0x61, 0x07, 0x02, 0x03, 0x01, 0x00, 0x01,
    0x02, 0x81, 0x80, 0x74, 0x88, 0x64, 0x3f, 0x69, 0x45, 0x3a, 0x6d, 0xc7,
    0x7f, 0xb9, 0xa3, 0xc0, 0x6e, 0xec, 0xdc, 0xd4, 0x5a, 0xb5, 0x32, 0x85,
    0x5f, 0x19, 0xd4, 0xf8, 0xd4, 0x3f, 0x3c, 0xfa, 0xc2, 0xf6, 0x5f, 0xee,
    0xe6, 0xba, 0x87, 0x74, 0x2e, 0xc7, 0x0c, 0xd4, 0x42, 0xb8, 0x66, 0x85,
    0x9c, 0x7b, 0x24, 0x61, 0xaa, 0x16, 0x11, 0xf6, 0xb5, 0xb6, 0xa4, 0x0a,
    0xc9, 0x55, 0x2e, 0x81, 0xa5, 0x47, 0x61, 0xcb, 0x25, 0x8f, 0xc2, 0x15,
    0x7b, 0x0e, 0x7c, 0x36, 0x9f, 0x3a, 0xda, 0x58, 0x86, 0x1c, 0x5b, 0x83,
    0x79, 0xe6, 0x2b, 0xcc, 0xe6, 0xfa, 0x2c, 0x61, 0xf2, 0x78, 0x80, 0x1b,
    0xe2, 0xf3, 0x9d, 0x39, 0x2b, 0x65, 0x57, 0x91, 0x3d, 0x71, 0x99, 0x73,
    0xa5, 0xc2, 0x79, 0x20, 0x8c, 0x07, 0x4f, 0xe5, 0xb4, 0x60, 0x1f, 0x99,
    0xa2, 0xb1, 0x4f, 0x0c, 0xef, 0xbc, 0x59, 0x53, 0x00, 0x7d, 0xb1, 0x02,
    0x41, 0x00, 0xfc, 0x7e, 0x23, 0x65, 0x70, 0xf8, 0xce, 0xd3, 0x40, 0x41,
    0x80, 0x6a, 0x1d, 0x01, 0xd6, 0x01, 0xff, 0xb6, 0x1b, 0x3d, 0x3d, 0x59,
    0x09, 0x33, 0x79, 0xc0, 0x4f, 0xde, 0x96, 0x27, 0x4b, 0x18, 0xc6, 0xd9,
    0x78, 0xf1, 0xf4, 0x35, 0x46, 0xe9, 0x7c, 0x42, 0x7a, 0x5d, 0x9f, 0xef,
    0x54, 0xb8, 0xf7, 0x9f, 0xc4, 0x33, 0x6c, 0xf3, 0x8c, 0x32, 0x46, 0x87,
    0x67, 0x30, 0x7b, 0xa7, 0xac, 0xe3, 0x02, 0x41, 0x00, 0xfc, 0x2c, 0xdf,
    0x0c, 0x0d, 0x88, 0xf5, 0xb1, 0x92, 0xa8, 0x93, 0x47, 0x63, 0x55, 0xf5,
    0xca, 0x58, 0x43, 0xba, 0x1c, 0xe5, 0x9e, 0xb6, 0x95, 0x05, 0xcd, 0xb5,
    0x82, 0xdf, 0xeb, 0x04, 0x53, 0x9d, 0xbd, 0xc2, 0x38, 0x16, 0xb3, 0x62,
    0xdd, 0xa1, 0x46, 0xdb, 0x6d, 0x97, 0x93, 0x9f, 0x8a, 0xc3, 0x9b, 0x64,
    0x7e, 0x42, 0xe3, 0x32, 0x57, 0x19, 0x1b, 0xd5, 0x6e, 0x85, 0xfa, 0xb8,
    0x8d, 0x02, 0x41, 0x00, 0xbc, 0x3d, 0xde, 0x6d, 0xd6, 0x97, 0xe8, 0xba,
    0x9e, 0x81, 0x37, 0x17, 0xe5, 0xa0, 0x64, 0xc9, 0x00, 0xb7, 0xe7, 0xfe,
    0xf4, 0x29, 0xd9, 0x2e, 0x43, 0x6b, 0x19, 0x20, 0xbd, 0x99, 0x75, 0xe7,
    0x76, 0xf8, 0xd3, 0xae, 0xaf, 0x7e, 0xb8, 0xeb, 0x81, 0xf4, 0x9d, 0xfe,
    0x07, 0x2b, 0x0b, 0x63, 0x0b, 0x5a, 0x55, 0x90, 0x71, 0x7d, 0xf1, 0xdb,
    0xd9, 0xb1, 0x41, 0x41, 0x68, 0x2f, 0x4e, 0x39, 0x02, 0x40, 0x5a, 0x34,
    0x66, 0xd8, 0xf5, 0xe2, 0x7f, 0x18, 0xb5, 0x00, 0x6e, 0x26, 0x84, 0x27,
    0x14, 0x93, 0xfb, 0xfc, 0xc6, 0x0f, 0x5e, 0x27, 0xe6, 0xe1, 0xe9, 0xc0,
    0x8a, 0xe4, 0x34, 0xda, 0xe9, 0xa2, 0x4b, 0x73, 0xbc, 0x8c, 0xb9, 0xba,
    0x13, 0x6c, 0x7a, 0x2b, 0x51, 0x84, 0xa3, 0x4a, 0xe0, 0x30, 0x10, 0x06,
    0x7e, 0xed, 0x17, 0x5a, 0x14, 0x00, 0xc9, 0xef, 0x85, 0xea, 0x52, 0x2c,
    0xbc, 0x65, 0x02, 0x40, 0x51, 0xe3, 0xf2, 0x83, 0x19, 0x9b, 0xc4, 0x1e,
    0x2f, 0x50, 0x3d, 0xdf, 0x5a, 0xa2, 0x18, 0xca, 0x5f, 0x2e, 0x49, 0xaf,
    0x6f, 0xcc, 0xfa, 0x65, 0x77, 0x94, 0xb5, 0xa1, 0x0a, 0xa9, 0xd1, 0x8a,
    0x39, 0x37, 0xf4, 0x0b, 0xa0, 0xd7, 0x82, 0x27, 0x5e, 0xae, 0x17, 0x17,
    0xa1, 0x1e, 0x54, 0x34, 0xbf, 0x6e, 0xc4, 0x8e, 0x99, 0x5d, 0x08, 0xf1,
    0x2d, 0x86, 0x9d, 0xa5, 0x20, 0x1b, 0xe5, 0xdf,
};

/*
 * kExampleBadRSAKeyDER is an RSA private key in ASN.1, DER format. The private
 * components are not correct.
 */
static const unsigned char kExampleBadRSAKeyDER[] = {
    0x30, 0x82, 0x04, 0x27, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00,
    0xa6, 0x1a, 0x1e, 0x6e, 0x7b, 0xee, 0xc6, 0x89, 0x66, 0xe7, 0x93, 0xef,
    0x54, 0x12, 0x68, 0xea, 0xbf, 0x86, 0x2f, 0xdd, 0xd2, 0x79, 0xb8, 0xa9,
    0x6e, 0x03, 0xc2, 0xa3, 0xb9, 0xa3, 0xe1, 0x4b, 0x2a, 0xb3, 0xf8, 0xb4,
    0xcd, 0xea, 0xbe, 0x24, 0xa6, 0x57, 0x5b, 0x83, 0x1f, 0x0f, 0xf2, 0xd3,
    0xb7, 0xac, 0x7e, 0xd6, 0x8e, 0x6e, 0x1e, 0xbf, 0xb8, 0x73, 0x8c, 0x05,
    0x56, 0xe6, 0x35, 0x1f, 0xe9, 0x04, 0x0b, 0x09, 0x86, 0x7d, 0xf1, 0x26,
    0x08, 0x99, 0xad, 0x7b, 0xc8, 0x4d, 0x94, 0xb0, 0x0b, 0x8b, 0x38, 0xa0,
    0x5c, 0x62, 0xa0, 0xab, 0xd3, 0x8f, 0xd4, 0x09, 0x60, 0x72, 0x1e, 0x33,
    0x50, 0x80, 0x6e, 0x22, 0xa6, 0x77, 0x57, 0x6b, 0x9a, 0x33, 0x21, 0x66,
    0x87, 0x6e, 0x21, 0x7b, 0xc7, 0x24, 0x0e, 0xd8, 0x13, 0xdf, 0x83, 0xde,
    0xcd, 0x40, 0x58, 0x1d, 0x84, 0x86, 0xeb, 0xb8, 0x12, 0x4e, 0xd2, 0xfa,
    0x80, 0x1f, 0xe4, 0xe7, 0x96, 0x29, 0xb8, 0xcc, 0xce, 0x66, 0x6d, 0x53,
    0xca, 0xb9, 0x5a, 0xd7, 0xf6, 0x84, 0x6c, 0x2d, 0x9a, 0x1a, 0x14, 0x1c,
    0x4e, 0x93, 0x39, 0xba, 0x74, 0xed, 0xed, 0x87, 0x87, 0x5e, 0x48, 0x75,
    0x36, 0xf0, 0xbc, 0x34, 0xfb, 0x29, 0xf9, 0x9f, 0x96, 0x5b, 0x0b, 0xa7,
    0x54, 0x30, 0x51, 0x29, 0x18, 0x5b, 0x7d, 0xac, 0x0f, 0xd6, 0x5f, 0x7c,
    0xf8, 0x98, 0x8c, 0xd8, 0x86, 0x62, 0xb3, 0xdc, 0xff, 0x0f, 0xff, 0x7a,
    0xaf, 0x5c, 0x4c, 0x61, 0x49, 0x2e, 0xc8, 0x95, 0x86, 0xc4, 0x0e, 0x87,
    0xfc, 0x1d, 0xcf, 0x8b, 0x7c, 0x61, 0xf6, 0xd8, 0xd0, 0x69, 0xf6, 0xcd,
    0x8a, 0x8c, 0xf6, 0x62, 0xa2, 0x56, 0xa9, 0xe3, 0xd1, 0xcf, 0x4d, 0xa0,
    0xf6, 0x2d, 0x20, 0x0a, 0x04, 0xb7, 0xa2, 0xf7, 0xb5, 0x99, 0x47, 0x18,
    0x56, 0x85, 0x87, 0xc7, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x82, 0x01,
    0x01, 0x00, 0x99, 0x41, 0x38, 0x1a, 0xd0, 0x96, 0x7a, 0xf0, 0x83, 0xd5,
    0xdf, 0x94, 0xce, 0x89, 0x3d, 0xec, 0x7a, 0x52, 0x21, 0x10, 0x16, 0x06,
    0xe0, 0xee, 0xd2, 0xe6, 0xfd, 0x4b, 0x7b, 0x19, 0x4d, 0xe1, 0xc0, 0xc0,
    0xd5, 0x14, 0x5d, 0x79, 0xdd, 0x7e, 0x8b, 0x4b, 0xc6, 0xcf, 0xb0, 0x75,
    0x52, 0xa3, 0x2d, 0xb1, 0x26, 0x46, 0x68, 0x9c, 0x0a, 0x1a, 0xf2, 0xe1,
    0x09, 0xac, 0x53, 0x85, 0x8c, 0x36, 0xa9, 0x14, 0x65, 0xea, 0xa0, 0x00,
    0xcb, 0xe3, 0x3f, 0xc4, 0x2b, 0x61, 0x2e, 0x6b, 0x06, 0x69, 0x77, 0xfd,
    0x38, 0x7e, 0x1d, 0x3f, 0x92, 0xe7, 0x77, 0x08, 0x19, 0xa7, 0x9d, 0x29,
    0x2d, 0xdc, 0x42, 0xc6, 0x7c, 0xd7, 0xd3, 0xa8, 0x01, 0x2c, 0xf2, 0xd5,
    0x82, 0x57, 0xcb, 0x55, 0x3d, 0xe7, 0xaa, 0xd2, 0x06, 0x30, 0x30, 0x05,
    0xe6, 0xf2, 0x47, 0x86, 0xba, 0xc6, 0x61, 0x64, 0xeb, 0x4f, 0x2a, 0x5e,
    0x07, 0x29, 0xe0, 0x96, 0xb2, 0x43, 0xff, 0x5f, 0x1a, 0x54, 0x16, 0xcf,
    0xb5, 0x56, 0x5c, 0xa0, 0x9b, 0x0c, 0xfd, 0xb3, 0xd2, 0xe3, 0x79, 0x1d,
    0x21, 0xe2, 0xd6, 0x13, 0xc4, 0x74, 0xa6, 0xf5, 0x8e, 0x8e, 0x81, 0xbb,
    0xb4, 0xad, 0x8a, 0xf0, 0x93, 0x0a, 0xd8, 0x0a, 0x42, 0x36, 0xbc, 0xe5,
    0x26, 0x2a, 0x0d, 0x5d, 0x57, 0x13, 0xc5, 0x4e, 0x2f, 0x12, 0x0e, 0xef,
    0xa7, 0x81, 0x1e, 0xc3, 0xa5, 0xdb, 0xc9, 0x24, 0xeb, 0x1a, 0xa1, 0xf9,
    0xf6, 0xa1, 0x78, 0x98, 0x93, 0x77, 0x42, 0x45, 0x03, 0xe2, 0xc9, 0xa2,
    0xfe, 0x2d, 0x77, 0xc8, 0xc6, 0xac, 0x9b, 0x98, 0x89, 0x6d, 0x9a, 0xe7,
    0x61, 0x63, 0xb7, 0xf2, 0xec, 0xd6, 0xb1, 0xa1, 0x6e, 0x0a, 0x1a, 0xff,
    0xfd, 0x43, 0x28, 0xc3, 0x0c, 0xdc, 0xf2, 0x47, 0x4f, 0x27, 0xaa, 0x99,
    0x04, 0x8e, 0xac, 0xe8, 0x7c, 0x01, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78,
    0x02, 0x81, 0x81, 0x00, 0xca, 0x69, 0xe5, 0xbb, 0x3a, 0x90, 0x82, 0xcb,
    0x82, 0x50, 0x2f, 0x29, 0xe2, 0x76, 0x6a, 0x57, 0x55, 0x45, 0x4e, 0x35,
    0x18, 0x61, 0xe0, 0x12, 0x70, 0xc0, 0xab, 0xc7, 0x80, 0xa2, 0xd4, 0x46,
    0x34, 0x03, 0xa0, 0x19, 0x26, 0x23, 0x9e, 0xef, 0x1a, 0xcb, 0x75, 0xd6,
    0xba, 0x81, 0xf4, 0x7e, 0x52, 0xe5, 0x2a, 0xe8, 0xf1, 0x49, 0x6c, 0x0f,
    0x1a, 0xa0, 0xf9, 0xc6, 0xe7, 0xec, 0x60, 0xe4, 0xcb, 0x2a, 0xb5, 0x56,
    0xe9, 0x9c, 0xcd, 0x19, 0x75, 0x92, 0xb1, 0x66, 0xce, 0xc3, 0xd9, 0x3d,
    0x11, 0xcb, 0xc4, 0x09, 0xce, 0x1e, 0x30, 0xba, 0x2f, 0x60, 0x60, 0x55,
    0x8d, 0x02, 0xdc, 0x5d, 0xaf, 0xf7, 0x52, 0x31, 0x17, 0x07, 0x53, 0x20,
    0x33, 0xad, 0x8c, 0xd5, 0x2f, 0x5a, 0xd0, 0x57, 0xd7, 0xd1, 0x80, 0xd6,
    0x3a, 0x9b, 0x04, 0x4f, 0x35, 0xbf, 0xe7, 0xd5, 0xbc, 0x8f, 0xd4, 0x81,
    0x02, 0x81, 0x81, 0x00, 0xc0, 0x9f, 0xf8, 0xcd, 0xf7, 0x3f, 0x26, 0x8a,
    0x3d, 0x4d, 0x2b, 0x0c, 0x01, 0xd0, 0xa2, 0xb4, 0x18, 0xfe, 0xf7, 0x5e,
    0x2f, 0x06, 0x13, 0xcd, 0x63, 0xaa, 0x12, 0xa9, 0x24, 0x86, 0xe3, 0xf3,
    0x7b, 0xda, 0x1a, 0x3c, 0xb1, 0x38, 0x80, 0x80, 0xef, 0x64, 0x64, 0xa1,
    0x9b, 0xfe, 0x76, 0x63, 0x8e, 0x83, 0xd2, 0xd9, 0xb9, 0x86, 0xb0, 0xe6,
    0xa6, 0x0c, 0x7e, 0xa8, 0x84, 0x90, 0x98, 0x0c, 0x1e, 0xf3, 0x14, 0x77,
    0xe0, 0x5f, 0x81, 0x08, 0x11, 0x8f, 0xa6, 0x23, 0xc4, 0xba, 0xc0, 0x8a,
    0xe4, 0xc6, 0xe3, 0x5c, 0xbe, 0xc5, 0xec, 0x2c, 0xb9, 0xd8, 0x8c, 0x4d,
    0x1a, 0x9d, 0xe7, 0x7c, 0x85, 0x4c, 0x0d, 0x71, 0x4e, 0x72, 0x33, 0x1b,
    0xfe, 0xa9, 0x17, 0x72, 0x76, 0x56, 0x9d, 0x74, 0x7e, 0x52, 0x67, 0x9a,
    0x87, 0x9a, 0xdb, 0x30, 0xde, 0xe4, 0x49, 0x28, 0x3b, 0xd2, 0x67, 0xaf,
    0x02, 0x81, 0x81, 0x00, 0x89, 0x74, 0x9a, 0x8e, 0xa7, 0xb9, 0xa5, 0x28,
    0xc0, 0x68, 0xe5, 0x6e, 0x63, 0x1c, 0x99, 0x20, 0x8f, 0x86, 0x8e, 0x12,
    0x9e, 0x69, 0x30, 0xfa, 0x34, 0xd9, 0x92, 0x8d, 0xdb, 0x7c, 0x37, 0xfd,
    0x28, 0xab, 0x61, 0x98, 0x52, 0x7f, 0x14, 0x1a, 0x39, 0xae, 0xfb, 0x6a,
    0x03, 0xa3, 0xe6, 0xbd, 0xb6, 0x5b, 0x6b, 0xe5, 0x5e, 0x9d, 0xc6, 0xa5,
    0x07, 0x27, 0x54, 0x17, 0xd0, 0x3d, 0x84, 0x9b, 0x3a, 0xa0, 0xd9, 0x1e,
    0x99, 0x6c, 0x63, 0x17, 0xab, 0xf1, 0x1f, 0x49, 0xba, 0x95, 0xe3, 0x3b,
    0x86, 0x8f, 0x42, 0xa4, 0x89, 0xf5, 0x94, 0x8f, 0x8b, 0x46, 0xbe, 0x84,
    0xba, 0x4a, 0xbc, 0x0d, 0x5f, 0x46, 0xeb, 0xe8, 0xec, 0x43, 0x8c, 0x1e,
    0xad, 0x19, 0x69, 0x2f, 0x08, 0x86, 0x7a, 0x3f, 0x7d, 0x0f, 0x07, 0x97,
    0xf3, 0x9a, 0x7b, 0xb5, 0xb2, 0xc1, 0x8c, 0x95, 0x68, 0x04, 0xa0, 0x81,
    0x02, 0x81, 0x80, 0x4e, 0xbf, 0x7e, 0x1b, 0xcb, 0x13, 0x61, 0x75, 0x3b,
    0xdb, 0x59, 0x5f, 0xb1, 0xd4, 0xb8, 0xeb, 0x9e, 0x73, 0xb5, 0xe7, 0xf6,
    0x89, 0x3d, 0x1c, 0xda, 0xf0, 0x36, 0xff, 0x35, 0xbd, 0x1e, 0x0b, 0x74,
    0xe3, 0x9e, 0xf0, 0xf2, 0xf7, 0xd7, 0x82, 0xb7, 0x7b, 0x6a, 0x1b, 0x0e,
    0x30, 0x4a, 0x98, 0x0e, 0xb4, 0xf9, 0x81, 0x07, 0xe4, 0x75, 0x39, 0xe9,
    0x53, 0xca, 0xbb, 0x5c, 0xaa, 0x93, 0x07, 0x0e, 0xa8, 0x2f, 0xba, 0x98,
    0x49, 0x30, 0xa7, 0xcc, 0x1a, 0x3c, 0x68, 0x0c, 0xe1, 0xa4, 0xb1, 0x05,
    0xe6, 0xe0, 0x25, 0x78, 0x58, 0x14, 0x37, 0xf5, 0x1f, 0xe3, 0x22, 0xef,
    0xa8, 0x0e, 0x22, 0xa0, 0x94, 0x3a, 0xf6, 0xc9, 0x13, 0xe6, 0x06, 0xbf,
    0x7f, 0x99, 0xc6, 0xcc, 0xd8, 0xc6, 0xbe, 0xd9, 0x2e, 0x24, 0xc7, 0x69,
    0x8c, 0x95, 0xba, 0xf6, 0x04, 0xb3, 0x0a, 0xf4, 0xcb, 0xf0, 0xce,
};

static const unsigned char kMsg[] = { 1, 2, 3, 4 };

static const unsigned char kSignature[] = {
    0xa5, 0xf0, 0x8a, 0x47, 0x5d, 0x3c, 0xb3, 0xcc, 0xa9, 0x79, 0xaf, 0x4d,
    0x8c, 0xae, 0x4c, 0x14, 0xef, 0xc2, 0x0b, 0x34, 0x36, 0xde, 0xf4, 0x3e,
    0x3d, 0xbb, 0x4a, 0x60, 0x5c, 0xc8, 0x91, 0x28, 0xda, 0xfb, 0x7e, 0x04,
    0x96, 0x7e, 0x63, 0x13, 0x90, 0xce, 0xb9, 0xb4, 0x62, 0x7a, 0xfd, 0x09,
    0x3d, 0xc7, 0x67, 0x78, 0x54, 0x04, 0xeb, 0x52, 0x62, 0x6e, 0x24, 0x67,
    0xb4, 0x40, 0xfc, 0x57, 0x62, 0xc6, 0xf1, 0x67, 0xc1, 0x97, 0x8f, 0x6a,
    0xa8, 0xae, 0x44, 0x46, 0x5e, 0xab, 0x67, 0x17, 0x53, 0x19, 0x3a, 0xda,
    0x5a, 0xc8, 0x16, 0x3e, 0x86, 0xd5, 0xc5, 0x71, 0x2f, 0xfc, 0x23, 0x48,
    0xd9, 0x0b, 0x13, 0xdd, 0x7b, 0x5a, 0x25, 0x79, 0xef, 0xa5, 0x7b, 0x04,
    0xed, 0x44, 0xf6, 0x18, 0x55, 0xe4, 0x0a, 0xe9, 0x57, 0x79, 0x5d, 0xd7,
    0x55, 0xa7, 0xab, 0x45, 0x02, 0x97, 0x60, 0x42,
};

/*
 * kExampleRSAKeyPKCS8 is kExampleRSAKeyDER encoded in a PKCS #8
 * PrivateKeyInfo.
 */
static const unsigned char kExampleRSAKeyPKCS8[] = {
    0x30, 0x82, 0x02, 0x76, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x02, 0x60, 0x30, 0x82, 0x02, 0x5c, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81,
    0x00, 0xf8, 0xb8, 0x6c, 0x83, 0xb4, 0xbc, 0xd9, 0xa8, 0x57, 0xc0, 0xa5,
    0xb4, 0x59, 0x76, 0x8c, 0x54, 0x1d, 0x79, 0xeb, 0x22, 0x52, 0x04, 0x7e,
    0xd3, 0x37, 0xeb, 0x41, 0xfd, 0x83, 0xf9, 0xf0, 0xa6, 0x85, 0x15, 0x34,
    0x75, 0x71, 0x5a, 0x84, 0xa8, 0x3c, 0xd2, 0xef, 0x5a, 0x4e, 0xd3, 0xde,
    0x97, 0x8a, 0xdd, 0xff, 0xbb, 0xcf, 0x0a, 0xaa, 0x86, 0x92, 0xbe, 0xb8,
    0x50, 0xe4, 0xcd, 0x6f, 0x80, 0x33, 0x30, 0x76, 0x13, 0x8f, 0xca, 0x7b,
    0xdc, 0xec, 0x5a, 0xca, 0x63, 0xc7, 0x03, 0x25, 0xef, 0xa8, 0x8a, 0x83,
    0x58, 0x76, 0x20, 0xfa, 0x16, 0x77, 0xd7, 0x79, 0x92, 0x63, 0x01, 0x48,
    0x1a, 0xd8, 0x7b, 0x67, 0xf1, 0x52, 0x55, 0x49, 0x4e, 0xd6, 0x6e, 0x4a,
    0x5c, 0xd7, 0x7a, 0x37, 0x36, 0x0c, 0xde, 0xdd, 0x8f, 0x44, 0xe8, 0xc2,
    0xa7, 0x2c, 0x2b, 0xb5, 0xaf, 0x64, 0x4b, 0x61, 0x07, 0x02, 0x03, 0x01,
    0x00, 0x01, 0x02, 0x81, 0x80, 0x74, 0x88, 0x64, 0x3f, 0x69, 0x45, 0x3a,
    0x6d, 0xc7, 0x7f, 0xb9, 0xa3, 0xc0, 0x6e, 0xec, 0xdc, 0xd4, 0x5a, 0xb5,
    0x32, 0x85, 0x5f, 0x19, 0xd4, 0xf8, 0xd4, 0x3f, 0x3c, 0xfa, 0xc2, 0xf6,
    0x5f, 0xee, 0xe6, 0xba, 0x87, 0x74, 0x2e, 0xc7, 0x0c, 0xd4, 0x42, 0xb8,
    0x66, 0x85, 0x9c, 0x7b, 0x24, 0x61, 0xaa, 0x16, 0x11, 0xf6, 0xb5, 0xb6,
    0xa4, 0x0a, 0xc9, 0x55, 0x2e, 0x81, 0xa5, 0x47, 0x61, 0xcb, 0x25, 0x8f,
    0xc2, 0x15, 0x7b, 0x0e, 0x7c, 0x36, 0x9f, 0x3a, 0xda, 0x58, 0x86, 0x1c,
    0x5b, 0x83, 0x79, 0xe6, 0x2b, 0xcc, 0xe6, 0xfa, 0x2c, 0x61, 0xf2, 0x78,
    0x80, 0x1b, 0xe2, 0xf3, 0x9d, 0x39, 0x2b, 0x65, 0x57, 0x91, 0x3d, 0x71,
    0x99, 0x73, 0xa5, 0xc2, 0x79, 0x20, 0x8c, 0x07, 0x4f, 0xe5, 0xb4, 0x60,
    0x1f, 0x99, 0xa2, 0xb1, 0x4f, 0x0c, 0xef, 0xbc, 0x59, 0x53, 0x00, 0x7d,
    0xb1, 0x02, 0x41, 0x00, 0xfc, 0x7e, 0x23, 0x65, 0x70, 0xf8, 0xce, 0xd3,
    0x40, 0x41, 0x80, 0x6a, 0x1d, 0x01, 0xd6, 0x01, 0xff, 0xb6, 0x1b, 0x3d,
    0x3d, 0x59, 0x09, 0x33, 0x79, 0xc0, 0x4f, 0xde, 0x96, 0x27, 0x4b, 0x18,
    0xc6, 0xd9, 0x78, 0xf1, 0xf4, 0x35, 0x46, 0xe9, 0x7c, 0x42, 0x7a, 0x5d,
    0x9f, 0xef, 0x54, 0xb8, 0xf7, 0x9f, 0xc4, 0x33, 0x6c, 0xf3, 0x8c, 0x32,
    0x46, 0x87, 0x67, 0x30, 0x7b, 0xa7, 0xac, 0xe3, 0x02, 0x41, 0x00, 0xfc,
    0x2c, 0xdf, 0x0c, 0x0d, 0x88, 0xf5, 0xb1, 0x92, 0xa8, 0x93, 0x47, 0x63,
    0x55, 0xf5, 0xca, 0x58, 0x43, 0xba, 0x1c, 0xe5, 0x9e, 0xb6, 0x95, 0x05,
    0xcd, 0xb5, 0x82, 0xdf, 0xeb, 0x04, 0x53, 0x9d, 0xbd, 0xc2, 0x38, 0x16,
    0xb3, 0x62, 0xdd, 0xa1, 0x46, 0xdb, 0x6d, 0x97, 0x93, 0x9f, 0x8a, 0xc3,
    0x9b, 0x64, 0x7e, 0x42, 0xe3, 0x32, 0x57, 0x19, 0x1b, 0xd5, 0x6e, 0x85,
    0xfa, 0xb8, 0x8d, 0x02, 0x41, 0x00, 0xbc, 0x3d, 0xde, 0x6d, 0xd6, 0x97,
    0xe8, 0xba, 0x9e, 0x81, 0x37, 0x17, 0xe5, 0xa0, 0x64, 0xc9, 0x00, 0xb7,
    0xe7, 0xfe, 0xf4, 0x29, 0xd9, 0x2e, 0x43, 0x6b, 0x19, 0x20, 0xbd, 0x99,
    0x75, 0xe7, 0x76, 0xf8, 0xd3, 0xae, 0xaf, 0x7e, 0xb8, 0xeb, 0x81, 0xf4,
    0x9d, 0xfe, 0x07, 0x2b, 0x0b, 0x63, 0x0b, 0x5a, 0x55, 0x90, 0x71, 0x7d,
    0xf1, 0xdb, 0xd9, 0xb1, 0x41, 0x41, 0x68, 0x2f, 0x4e, 0x39, 0x02, 0x40,
    0x5a, 0x34, 0x66, 0xd8, 0xf5, 0xe2, 0x7f, 0x18, 0xb5, 0x00, 0x6e, 0x26,
    0x84, 0x27, 0x14, 0x93, 0xfb, 0xfc, 0xc6, 0x0f, 0x5e, 0x27, 0xe6, 0xe1,
    0xe9, 0xc0, 0x8a, 0xe4, 0x34, 0xda, 0xe9, 0xa2, 0x4b, 0x73, 0xbc, 0x8c,
    0xb9, 0xba, 0x13, 0x6c, 0x7a, 0x2b, 0x51, 0x84, 0xa3, 0x4a, 0xe0, 0x30,
    0x10, 0x06, 0x7e, 0xed, 0x17, 0x5a, 0x14, 0x00, 0xc9, 0xef, 0x85, 0xea,
    0x52, 0x2c, 0xbc, 0x65, 0x02, 0x40, 0x51, 0xe3, 0xf2, 0x83, 0x19, 0x9b,
    0xc4, 0x1e, 0x2f, 0x50, 0x3d, 0xdf, 0x5a, 0xa2, 0x18, 0xca, 0x5f, 0x2e,
    0x49, 0xaf, 0x6f, 0xcc, 0xfa, 0x65, 0x77, 0x94, 0xb5, 0xa1, 0x0a, 0xa9,
    0xd1, 0x8a, 0x39, 0x37, 0xf4, 0x0b, 0xa0, 0xd7, 0x82, 0x27, 0x5e, 0xae,
    0x17, 0x17, 0xa1, 0x1e, 0x54, 0x34, 0xbf, 0x6e, 0xc4, 0x8e, 0x99, 0x5d,
    0x08, 0xf1, 0x2d, 0x86, 0x9d, 0xa5, 0x20, 0x1b, 0xe5, 0xdf,
};

#ifndef OPENSSL_NO_EC
/*
 * kExampleECKeyDER is a sample EC private key encoded as an ECPrivateKey
 * structure.
 */
static const unsigned char kExampleECKeyDER[] = {
    0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x07, 0x0f, 0x08, 0x72, 0x7a,
    0xd4, 0xa0, 0x4a, 0x9c, 0xdd, 0x59, 0xc9, 0x4d, 0x89, 0x68, 0x77, 0x08,
    0xb5, 0x6f, 0xc9, 0x5d, 0x30, 0x77, 0x0e, 0xe8, 0xd1, 0xc9, 0xce, 0x0a,
    0x8b, 0xb4, 0x6a, 0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0xe6, 0x2b, 0x69,
    0xe2, 0xbf, 0x65, 0x9f, 0x97, 0xbe, 0x2f, 0x1e, 0x0d, 0x94, 0x8a, 0x4c,
    0xd5, 0x97, 0x6b, 0xb7, 0xa9, 0x1e, 0x0d, 0x46, 0xfb, 0xdd, 0xa9, 0xa9,
    0x1e, 0x9d, 0xdc, 0xba, 0x5a, 0x01, 0xe7, 0xd6, 0x97, 0xa8, 0x0a, 0x18,
    0xf9, 0xc3, 0xc4, 0xa3, 0x1e, 0x56, 0xe2, 0x7c, 0x83, 0x48, 0xdb, 0x16,
    0x1a, 0x1c, 0xf5, 0x1d, 0x7e, 0xf1, 0x94, 0x2d, 0x4b, 0xcf, 0x72, 0x22,
    0xc1,
};

/*
 * kExampleBadECKeyDER is a sample EC private key encoded as an ECPrivateKey
 * structure. The private key is equal to the order and will fail to import
 */
static const unsigned char kExampleBadECKeyDER[] = {
    0x30, 0x66, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48,
    0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03,
    0x01, 0x07, 0x04, 0x4C, 0x30, 0x4A, 0x02, 0x01, 0x01, 0x04, 0x20, 0xFF,
    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3,
    0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51, 0xA1, 0x23, 0x03, 0x21, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
    0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

/* prime256v1 */
static const unsigned char kExampleECPubKeyDER[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xba, 0xeb, 0x83, 0xfb, 0x3b, 0xb2, 0xff, 0x30, 0x53,
    0xdb, 0xce, 0x32, 0xf2, 0xac, 0xae, 0x44, 0x0d, 0x3d, 0x13, 0x53, 0xb8,
    0xd1, 0x68, 0x55, 0xde, 0x44, 0x46, 0x05, 0xa6, 0xc9, 0xd2, 0x04, 0xb7,
    0xe3, 0xa2, 0x96, 0xc8, 0xb2, 0x5e, 0x22, 0x03, 0xd7, 0x03, 0x7a, 0x8b,
    0x13, 0x5c, 0x42, 0x49, 0xc2, 0xab, 0x86, 0xd6, 0xac, 0x6b, 0x93, 0x20,
    0x56, 0x6a, 0xc6, 0xc8, 0xa5, 0x0b, 0xe5
};

/*
 * kExampleBadECPubKeyDER is a sample EC public key with a wrong OID
 * 1.2.840.10045.2.2 instead of 1.2.840.10045.2.1 - EC Public Key
 */
static const unsigned char kExampleBadECPubKeyDER[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x02, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xba, 0xeb, 0x83, 0xfb, 0x3b, 0xb2, 0xff, 0x30, 0x53,
    0xdb, 0xce, 0x32, 0xf2, 0xac, 0xae, 0x44, 0x0d, 0x3d, 0x13, 0x53, 0xb8,
    0xd1, 0x68, 0x55, 0xde, 0x44, 0x46, 0x05, 0xa6, 0xc9, 0xd2, 0x04, 0xb7,
    0xe3, 0xa2, 0x96, 0xc8, 0xb2, 0x5e, 0x22, 0x03, 0xd7, 0x03, 0x7a, 0x8b,
    0x13, 0x5c, 0x42, 0x49, 0xc2, 0xab, 0x86, 0xd6, 0xac, 0x6b, 0x93, 0x20,
    0x56, 0x6a, 0xc6, 0xc8, 0xa5, 0x0b, 0xe5
};

static const unsigned char pExampleECParamDER[] = {
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};
#endif

static const unsigned char kCFBDefaultKey[] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88,
    0x09, 0xCF, 0x4F, 0x3C
};

static const unsigned char kGCMDefaultKey[32] = { 0 };

static const unsigned char kGCMResetKey[] = {
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c, 0x6d, 0x6a, 0x8f, 0x94,
    0x67, 0x30, 0x83, 0x08, 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
    0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08
};

static const unsigned char iCFBIV[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F
};

static const unsigned char iGCMDefaultIV[12] = { 0 };

static const unsigned char iGCMResetIV1[] = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad
};

static const unsigned char iGCMResetIV2[] = {
    0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88
};

static const unsigned char cfbPlaintext[] = {
    0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96, 0xE9, 0x3D, 0x7E, 0x11,
    0x73, 0x93, 0x17, 0x2A
};

static const unsigned char gcmDefaultPlaintext[16] = { 0 };

static const unsigned char gcmResetPlaintext[] = {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5, 0xa5, 0x59, 0x09, 0xc5,
    0xaf, 0xf5, 0x26, 0x9a, 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
    0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72, 0x1c, 0x3c, 0x0c, 0x95,
    0x95, 0x68, 0x09, 0x53, 0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
    0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57, 0xba, 0x63, 0x7b, 0x39
};

static const unsigned char cfbCiphertext[] = {
    0x3B, 0x3F, 0xD9, 0x2E, 0xB7, 0x2D, 0xAD, 0x20, 0x33, 0x34, 0x49, 0xF8,
    0xE8, 0x3C, 0xFB, 0x4A
};

static const unsigned char gcmDefaultCiphertext[] = {
    0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e, 0x07, 0x4e, 0xc5, 0xd3,
    0xba, 0xf3, 0x9d, 0x18
};

static const unsigned char gcmResetCiphertext1[] = {
    0xc3, 0x76, 0x2d, 0xf1, 0xca, 0x78, 0x7d, 0x32, 0xae, 0x47, 0xc1, 0x3b,
    0xf1, 0x98, 0x44, 0xcb, 0xaf, 0x1a, 0xe1, 0x4d, 0x0b, 0x97, 0x6a, 0xfa,
    0xc5, 0x2f, 0xf7, 0xd7, 0x9b, 0xba, 0x9d, 0xe0, 0xfe, 0xb5, 0x82, 0xd3,
    0x39, 0x34, 0xa4, 0xf0, 0x95, 0x4c, 0xc2, 0x36, 0x3b, 0xc7, 0x3f, 0x78,
    0x62, 0xac, 0x43, 0x0e, 0x64, 0xab, 0xe4, 0x99, 0xf4, 0x7c, 0x9b, 0x1f
};

static const unsigned char gcmResetCiphertext2[] = {
    0x52, 0x2d, 0xc1, 0xf0, 0x99, 0x56, 0x7d, 0x07, 0xf4, 0x7f, 0x37, 0xa3,
    0x2a, 0x84, 0x42, 0x7d, 0x64, 0x3a, 0x8c, 0xdc, 0xbf, 0xe5, 0xc0, 0xc9,
    0x75, 0x98, 0xa2, 0xbd, 0x25, 0x55, 0xd1, 0xaa, 0x8c, 0xb0, 0x8e, 0x48,
    0x59, 0x0d, 0xbb, 0x3d, 0xa7, 0xb0, 0x8b, 0x10, 0x56, 0x82, 0x88, 0x38,
    0xc5, 0xf6, 0x1e, 0x63, 0x93, 0xba, 0x7a, 0x0a, 0xbc, 0xc9, 0xf6, 0x62
};

static const unsigned char gcmAAD[] = {
    0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed, 0xfa, 0xce,
    0xde, 0xad, 0xbe, 0xef, 0xab, 0xad, 0xda, 0xd2
};

static const unsigned char gcmDefaultTag[] = {
    0xd0, 0xd1, 0xc8, 0xa7, 0x99, 0x99, 0x6b, 0xf0, 0x26, 0x5b, 0x98, 0xb5,
    0xd4, 0x8a, 0xb9, 0x19
};

static const unsigned char gcmResetTag1[] = {
    0x3a, 0x33, 0x7d, 0xbf, 0x46, 0xa7, 0x92, 0xc4, 0x5e, 0x45, 0x49, 0x13,
    0xfe, 0x2e, 0xa8, 0xf2
};

static const unsigned char gcmResetTag2[] = {
    0x76, 0xfc, 0x6e, 0xce, 0x0f, 0x4e, 0x17, 0x68, 0xcd, 0xdf, 0x88, 0x53,
    0xbb, 0x2d, 0x55, 0x1b
};


typedef struct APK_DATA_st {
    const unsigned char *kder;
    size_t size;
    int evptype;
    int check;
    int pub_check;
    int param_check;
    int type; /* 0 for private, 1 for public, 2 for params */
} APK_DATA;

typedef struct {
    const char *cipher;
    const unsigned char *key;
    const unsigned char *iv;
    const unsigned char *input;
    const unsigned char *expected;
    const unsigned char *tag;
    size_t ivlen; /* 0 if we do not need to set a specific IV len */
    size_t inlen;
    size_t expectedlen;
    size_t taglen;
    int keyfirst;
    int initenc;
    int finalenc;
} EVP_INIT_TEST_st;

static const EVP_INIT_TEST_st evp_init_tests[] = {
    {
        "aes-128-cfb", kCFBDefaultKey, iCFBIV, cfbPlaintext,
        cfbCiphertext, NULL, 0, sizeof(cfbPlaintext), sizeof(cfbCiphertext),
        0, 1, 0, 1
    },
    {
        "aes-256-gcm", kGCMDefaultKey, iGCMDefaultIV, gcmDefaultPlaintext,
        gcmDefaultCiphertext, gcmDefaultTag, sizeof(iGCMDefaultIV),
        sizeof(gcmDefaultPlaintext), sizeof(gcmDefaultCiphertext),
        sizeof(gcmDefaultTag), 1, 0, 1
    },
    {
        "aes-128-cfb", kCFBDefaultKey, iCFBIV, cfbPlaintext,
        cfbCiphertext, NULL, 0, sizeof(cfbPlaintext), sizeof(cfbCiphertext),
        0, 0, 0, 1
    },
    {
        "aes-256-gcm", kGCMDefaultKey, iGCMDefaultIV, gcmDefaultPlaintext,
        gcmDefaultCiphertext, gcmDefaultTag, sizeof(iGCMDefaultIV),
        sizeof(gcmDefaultPlaintext), sizeof(gcmDefaultCiphertext),
        sizeof(gcmDefaultTag), 0, 0, 1
    },
    {
        "aes-128-cfb", kCFBDefaultKey, iCFBIV, cfbCiphertext,
        cfbPlaintext, NULL, 0, sizeof(cfbCiphertext), sizeof(cfbPlaintext),
        0, 1, 1, 0
    },
    {
        "aes-256-gcm", kGCMDefaultKey, iGCMDefaultIV, gcmDefaultCiphertext,
        gcmDefaultPlaintext, gcmDefaultTag, sizeof(iGCMDefaultIV),
        sizeof(gcmDefaultCiphertext), sizeof(gcmDefaultPlaintext),
        sizeof(gcmDefaultTag), 1, 1, 0
    },
    {
        "aes-128-cfb", kCFBDefaultKey, iCFBIV, cfbCiphertext,
        cfbPlaintext, NULL, 0, sizeof(cfbCiphertext), sizeof(cfbPlaintext),
        0, 0, 1, 0
    },
    {
        "aes-256-gcm", kGCMDefaultKey, iGCMDefaultIV, gcmDefaultCiphertext,
        gcmDefaultPlaintext, gcmDefaultTag, sizeof(iGCMDefaultIV),
        sizeof(gcmDefaultCiphertext), sizeof(gcmDefaultPlaintext),
        sizeof(gcmDefaultTag), 0, 1, 0
    }
};

static int evp_init_seq_set_iv(EVP_CIPHER_CTX *ctx, const EVP_INIT_TEST_st *t)
{
    int res = 0;

    if (t->ivlen != 0) {
        if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, t->ivlen, NULL)))
            goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, NULL, t->iv, -1)))
        goto err;
    res = 1;
 err:
    return res;
}

/*
 * Test step-wise cipher initialization via EVP_CipherInit_ex where the
 * arguments are given one at a time and a final adjustment to the enc
 * parameter sets the correct operation.
 */
static int test_evp_init_seq(int idx)
{
    int outlen1, outlen2;
    int testresult = 0;
    unsigned char outbuf[1024];
    unsigned char tag[16];
    const EVP_INIT_TEST_st *t = &evp_init_tests[idx];
    EVP_CIPHER_CTX *ctx = NULL;
    const EVP_CIPHER *type = NULL;
    size_t taglen = sizeof(tag);
    char *errmsg = NULL;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        errmsg = "CTX_ALLOC";
        goto err;
    }
    if (!TEST_ptr(type = EVP_get_cipherbyname(t->cipher))) {
        errmsg = "GET_CIPHERBYNAME";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, type, NULL, NULL, NULL, t->initenc))) {
        errmsg = "EMPTY_ENC_INIT";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_set_padding(ctx, 0))) {
        errmsg = "PADDING";
        goto err;
    }
    if (t->keyfirst && !TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, t->key, NULL, -1))) {
        errmsg = "KEY_INIT (before iv)";
        goto err;
    }
    if (!evp_init_seq_set_iv(ctx, t)) {
        errmsg = "IV_INIT";
        goto err;
    }
    if (t->keyfirst == 0 &&  !TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, t->key, NULL, -1))) {
        errmsg = "KEY_INIT (after iv)";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, NULL, NULL, t->finalenc))) {
        errmsg = "FINAL_ENC_INIT";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, t->input, t->inlen))) {
        errmsg = "CIPHER_UPDATE";
        goto err;
    }
    if (t->finalenc == 0 && t->tag != NULL) {
        /* Set expected tag */
        if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                           t->taglen, (void *)t->tag))) {
            errmsg = "SET_TAG";
            goto err;
        }
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL";
        goto err;
    }
    if (!TEST_mem_eq(t->expected, t->expectedlen, outbuf, outlen1 + outlen2)) {
        errmsg = "WRONG_RESULT";
        goto err;
    }
    if (t->finalenc != 0 && t->tag != NULL) {
        if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen, tag))) {
            errmsg = "GET_TAG";
            goto err;
        }
        if (!TEST_mem_eq(t->tag, t->taglen, tag, taglen)) {
            errmsg = "TAG_ERROR";
            goto err;
        }
    }
    testresult = 1;
 err:
    if (errmsg != NULL)
        TEST_info("evp_init_test %d: %s", idx, errmsg);
    EVP_CIPHER_CTX_free(ctx);
    return testresult;
}

typedef struct {
    const unsigned char *input;
    const unsigned char *expected;
    size_t inlen;
    size_t expectedlen;
    int enc;
} EVP_RESET_TEST_st;

static const EVP_RESET_TEST_st evp_reset_tests[] = {
    {
        cfbPlaintext, cfbCiphertext,
        sizeof(cfbPlaintext), sizeof(cfbCiphertext), 1
    },
    {
        cfbCiphertext, cfbPlaintext,
        sizeof(cfbCiphertext), sizeof(cfbPlaintext), 0
    }
};

/*
 * Test a reset of a cipher via EVP_CipherInit_ex after the cipher has already
 * been used.
 */
static int test_evp_reset(int idx)
{
    const EVP_RESET_TEST_st *t = &evp_reset_tests[idx];
    int outlen1, outlen2;
    int testresult = 0;
    unsigned char outbuf[1024];
    EVP_CIPHER_CTX *ctx = NULL;
    const EVP_CIPHER *type = NULL;
    char *errmsg = NULL;

    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new())) {
        errmsg = "CTX_ALLOC";
        goto err;
    }
    if (!TEST_ptr(type = EVP_get_cipherbyname("aes-128-cfb"))) {
        errmsg = "GET_CIPHERBYNAME";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, type, NULL, kCFBDefaultKey, iCFBIV, t->enc))) {
        errmsg = "CIPHER_INIT";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_set_padding(ctx, 0))) {
        errmsg = "PADDING";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, t->input, t->inlen))) {
        errmsg = "CIPHER_UPDATE";
        goto err;
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL";
        goto err;
    }
    if (!TEST_mem_eq(t->expected, t->expectedlen, outbuf, outlen1 + outlen2)) {
        errmsg = "WRONG_RESULT";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, NULL, NULL, -1))) {
        errmsg = "CIPHER_REINIT";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, t->input, t->inlen))) {
        errmsg = "CIPHER_UPDATE (reinit)";
        goto err;
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL (reinit)";
        goto err;
    }
    if (!TEST_mem_eq(t->expected, t->expectedlen, outbuf, outlen1 + outlen2)) {
        errmsg = "WRONG_RESULT (reinit)";
        goto err;
    }
    testresult = 1;
 err:
    if (errmsg != NULL)
        TEST_info("test_evp_reset %d: %s", idx, errmsg);
    EVP_CIPHER_CTX_free(ctx);
    return testresult;
}

typedef struct {
    const unsigned char *iv1;
    const unsigned char *iv2;
    const unsigned char *expected1;
    const unsigned char *expected2;
    const unsigned char *tag1;
    const unsigned char *tag2;
    size_t ivlen1;
    size_t ivlen2;
    size_t expectedlen1;
    size_t expectedlen2;
} TEST_GCM_IV_REINIT_st;

static const TEST_GCM_IV_REINIT_st gcm_reinit_tests[] = {
    {
        iGCMResetIV1, iGCMResetIV2, gcmResetCiphertext1, gcmResetCiphertext2,
        gcmResetTag1, gcmResetTag2, sizeof(iGCMResetIV1), sizeof(iGCMResetIV2),
        sizeof(gcmResetCiphertext1), sizeof(gcmResetCiphertext2)
    },
    {
        iGCMResetIV2, iGCMResetIV1, gcmResetCiphertext2, gcmResetCiphertext1,
        gcmResetTag2, gcmResetTag1, sizeof(iGCMResetIV2), sizeof(iGCMResetIV1),
        sizeof(gcmResetCiphertext2), sizeof(gcmResetCiphertext1)
    }
};

static int test_gcm_reinit(int idx)
{
    int outlen1, outlen2, outlen3;
    int testresult = 0;
    unsigned char outbuf[1024];
    unsigned char tag[16];
    const TEST_GCM_IV_REINIT_st *t = &gcm_reinit_tests[idx];
    EVP_CIPHER_CTX *ctx = NULL;
    const EVP_CIPHER *type = NULL;
    size_t taglen = sizeof(tag);
    char *errmsg = NULL;

    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new())) {
        errmsg = "CTX_ALLOC";
        goto err;
    }
    if (!TEST_ptr(type = EVP_get_cipherbyname("aes-256-gcm"))) {
        errmsg = "GET_CIPHERBYNAME";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, type, NULL, NULL, NULL, 1))) {
        errmsg = "ENC_INIT";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, t->ivlen1, NULL))) {
        errmsg = "SET_IVLEN1";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, kGCMResetKey, t->iv1, 1))) {
        errmsg = "SET_IV1";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, NULL, &outlen3, gcmAAD, sizeof(gcmAAD)))) {
        errmsg = "AAD1";
        goto err;
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, gcmResetPlaintext,
                                    sizeof(gcmResetPlaintext)))) {
        errmsg = "CIPHER_UPDATE1";
        goto err;
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL1";
        goto err;
    }
    if (!TEST_mem_eq(t->expected1, t->expectedlen1, outbuf, outlen1 + outlen2)) {
        errmsg = "WRONG_RESULT1";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen, tag))) {
        errmsg = "GET_TAG1";
        goto err;
    }
    if (!TEST_mem_eq(t->tag1, taglen, tag, taglen)) {
        errmsg = "TAG_ERROR1";
        goto err;
    }
    /* Now reinit */
    if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, t->ivlen2, NULL))) {
        errmsg = "SET_IVLEN2";
        goto err;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, NULL, NULL, NULL, t->iv2, -1))) {
        errmsg = "SET_IV2";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, NULL, &outlen3, gcmAAD, sizeof(gcmAAD)))) {
        errmsg = "AAD2";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, gcmResetPlaintext,
                                    sizeof(gcmResetPlaintext)))) {
        errmsg = "CIPHER_UPDATE2";
        goto err;
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL2";
        goto err;
    }
    if (!TEST_mem_eq(t->expected2, t->expectedlen2, outbuf, outlen1 + outlen2)) {
        errmsg = "WRONG_RESULT2";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen, tag))) {
        errmsg = "GET_TAG2";
        goto err;
    }
    if (!TEST_mem_eq(t->tag2, taglen, tag, taglen)) {
        errmsg = "TAG_ERROR2";
        goto err;
    }
    testresult = 1;
 err:
    if (errmsg != NULL)
        TEST_info("evp_init_test %d: %s", idx, errmsg);
    EVP_CIPHER_CTX_free(ctx);
    return testresult;
}

typedef struct {
    const char *cipher;
    int enc;
} EVP_UPDATED_IV_TEST_st;

static const EVP_UPDATED_IV_TEST_st evp_updated_iv_tests[] = {
    {
        "aes-128-cfb", 1
    },
    {
        "aes-128-cfb", 0
    },
    {
        "aes-128-cfb1", 1
    },
    {
        "aes-128-cfb1", 0
    },
    {
        "aes-128-cfb128", 1
    },
    {
        "aes-128-cfb128", 0
    },
    {
        "aes-128-cfb8", 1
    },
    {
        "aes-128-cfb8", 0
    },
    {
        "aes-128-ofb", 1
    },
    {
        "aes-128-ofb", 0
    },
    {
        "aes-128-ctr", 1
    },
    {
        "aes-128-ctr", 0
    },
    {
        "aes-128-cbc", 1
    },
    {
        "aes-128-cbc", 0
    }
};

/*
 * Test that the IV in the context is updated during a crypto operation for CFB
 * and OFB.
 */
static int test_evp_updated_iv(int idx)
{
    const EVP_UPDATED_IV_TEST_st *t = &evp_updated_iv_tests[idx];
    int outlen1, outlen2;
    int testresult = 0;
    unsigned char outbuf[1024];
    EVP_CIPHER_CTX *ctx = NULL;
    const EVP_CIPHER *type = NULL;
    const unsigned char *updated_iv;
    int iv_len;
    char *errmsg = NULL;

    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new())) {
        errmsg = "CTX_ALLOC";
        goto err;
    }
    if ((type = EVP_get_cipherbyname(t->cipher)) == NULL) {
        TEST_info("cipher %s not supported, skipping", t->cipher);
        goto ok;
    }
    if (!TEST_true(EVP_CipherInit_ex(ctx, type, NULL, kCFBDefaultKey, iCFBIV, t->enc))) {
        errmsg = "CIPHER_INIT";
        goto err;
    }
    if (!TEST_true(EVP_CIPHER_CTX_set_padding(ctx, 0))) {
        errmsg = "PADDING";
        goto err;
    }
    if (!TEST_true(EVP_CipherUpdate(ctx, outbuf, &outlen1, cfbPlaintext, sizeof(cfbPlaintext)))) {
        errmsg = "CIPHER_UPDATE";
        goto err;
    }
    if (!TEST_ptr(updated_iv = EVP_CIPHER_CTX_iv(ctx))) {
        errmsg = "CIPHER_CTX_IV";
        goto err;
    }
    if (!TEST_true(iv_len = EVP_CIPHER_CTX_iv_length(ctx))) {
        errmsg = "CIPHER_CTX_IV_LEN";
        goto err;
    }
    if (!TEST_mem_ne(iCFBIV, sizeof(iCFBIV), updated_iv, iv_len)) {
        errmsg = "IV_NOT_UPDATED";
        goto err;
    }
    if (!TEST_true(EVP_CipherFinal_ex(ctx, outbuf + outlen1, &outlen2))) {
        errmsg = "CIPHER_FINAL";
        goto err;
    }
 ok:
    testresult = 1;
 err:
    if (errmsg != NULL)
        TEST_info("test_evp_updated_iv %d: %s", idx, errmsg);
    EVP_CIPHER_CTX_free(ctx);
    return testresult;
}

static APK_DATA keydata[] = {
    {kExampleRSAKeyDER, sizeof(kExampleRSAKeyDER), EVP_PKEY_RSA},
    {kExampleRSAKeyPKCS8, sizeof(kExampleRSAKeyPKCS8), EVP_PKEY_RSA},
#ifndef OPENSSL_NO_EC
    {kExampleECKeyDER, sizeof(kExampleECKeyDER), EVP_PKEY_EC}
#endif
};

static APK_DATA keycheckdata[] = {
    {kExampleRSAKeyDER, sizeof(kExampleRSAKeyDER), EVP_PKEY_RSA, 1, -2, -2, 0},
    {kExampleBadRSAKeyDER, sizeof(kExampleBadRSAKeyDER), EVP_PKEY_RSA,
     0, -2, -2, 0},
#ifndef OPENSSL_NO_EC
    {kExampleECKeyDER, sizeof(kExampleECKeyDER), EVP_PKEY_EC, 1, 1, 1, 0},
    /* group is also associated in our pub key */
    {kExampleECPubKeyDER, sizeof(kExampleECPubKeyDER), EVP_PKEY_EC, 0, 1, 1, 1},
    {pExampleECParamDER, sizeof(pExampleECParamDER), EVP_PKEY_EC, 0, 0, 1, 2}
#endif
};

static EVP_PKEY *load_example_rsa_key(void)
{
    EVP_PKEY *ret = NULL;
    const unsigned char *derp = kExampleRSAKeyDER;
    EVP_PKEY *pkey = NULL;
    RSA *rsa = NULL;

    if (!TEST_true(d2i_RSAPrivateKey(&rsa, &derp, sizeof(kExampleRSAKeyDER))))
        return NULL;

    if (!TEST_ptr(pkey = EVP_PKEY_new())
            || !TEST_true(EVP_PKEY_set1_RSA(pkey, rsa)))
        goto end;

    ret = pkey;
    pkey = NULL;

end:
    EVP_PKEY_free(pkey);
    RSA_free(rsa);

    return ret;
}

static int test_EVP_Enveloped(void)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_PKEY *keypair = NULL;
    unsigned char *kek = NULL;
    unsigned char iv[EVP_MAX_IV_LENGTH];
    static const unsigned char msg[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int len, kek_len, ciphertext_len, plaintext_len;
    unsigned char ciphertext[32], plaintext[16];
    const EVP_CIPHER *type = EVP_aes_256_cbc();

    if (!TEST_ptr(keypair = load_example_rsa_key())
            || !TEST_ptr(kek = OPENSSL_zalloc(EVP_PKEY_size(keypair)))
            || !TEST_ptr(ctx = EVP_CIPHER_CTX_new())
            || !TEST_true(EVP_SealInit(ctx, type, &kek, &kek_len, iv,
                                       &keypair, 1))
            || !TEST_true(EVP_SealUpdate(ctx, ciphertext, &ciphertext_len,
                                         msg, sizeof(msg)))
            || !TEST_true(EVP_SealFinal(ctx, ciphertext + ciphertext_len,
                                        &len)))
        goto err;

    ciphertext_len += len;

    if (!TEST_true(EVP_OpenInit(ctx, type, kek, kek_len, iv, keypair))
            || !TEST_true(EVP_OpenUpdate(ctx, plaintext, &plaintext_len,
                                         ciphertext, ciphertext_len))
            || !TEST_true(EVP_OpenFinal(ctx, plaintext + plaintext_len, &len)))
        goto err;

    plaintext_len += len;
    if (!TEST_mem_eq(msg, sizeof(msg), plaintext, plaintext_len))
        goto err;

    ret = 1;
err:
    OPENSSL_free(kek);
    EVP_PKEY_free(keypair);
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}


static int test_EVP_DigestSignInit(void)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    unsigned char *sig = NULL;
    size_t sig_len = 0;
    EVP_MD_CTX *md_ctx, *md_ctx_verify = NULL;

    if (!TEST_ptr(md_ctx = EVP_MD_CTX_new())
            || !TEST_ptr(md_ctx_verify = EVP_MD_CTX_new())
            || !TEST_ptr(pkey = load_example_rsa_key()))
        goto out;

    if (!TEST_true(EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, pkey))
            || !TEST_true(EVP_DigestSignUpdate(md_ctx, kMsg, sizeof(kMsg))))
        goto out;

    /* Determine the size of the signature. */
    if (!TEST_true(EVP_DigestSignFinal(md_ctx, NULL, &sig_len))
            || !TEST_size_t_eq(sig_len, (size_t)EVP_PKEY_size(pkey)))
        goto out;

    if (!TEST_ptr(sig = OPENSSL_malloc(sig_len))
            || !TEST_true(EVP_DigestSignFinal(md_ctx, sig, &sig_len)))
        goto out;

    /* Ensure that the signature round-trips. */
    if (!TEST_true(EVP_DigestVerifyInit(md_ctx_verify, NULL, EVP_sha256(),
                                        NULL, pkey))
            || !TEST_true(EVP_DigestVerifyUpdate(md_ctx_verify,
                                                 kMsg, sizeof(kMsg)))
            || !TEST_true(EVP_DigestVerifyFinal(md_ctx_verify, sig, sig_len)))
        goto out;

    ret = 1;

 out:
    EVP_MD_CTX_free(md_ctx);
    EVP_MD_CTX_free(md_ctx_verify);
    EVP_PKEY_free(pkey);
    OPENSSL_free(sig);

    return ret;
}

static int test_EVP_DigestVerifyInit(void)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *md_ctx = NULL;

    if (!TEST_ptr(md_ctx = EVP_MD_CTX_new())
            || !TEST_ptr(pkey = load_example_rsa_key()))
        goto out;

    if (!TEST_true(EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, pkey))
            || !TEST_true(EVP_DigestVerifyUpdate(md_ctx, kMsg, sizeof(kMsg)))
            || !TEST_true(EVP_DigestVerifyFinal(md_ctx, kSignature,
                                                 sizeof(kSignature))))
        goto out;
    ret = 1;

 out:
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

static int test_d2i_AutoPrivateKey(int i)
{
    int ret = 0;
    const unsigned char *p;
    EVP_PKEY *pkey = NULL;
    const APK_DATA *ak = &keydata[i];
    const unsigned char *input = ak->kder;
    size_t input_len = ak->size;
    int expected_id = ak->evptype;

    p = input;
    if (!TEST_ptr(pkey = d2i_AutoPrivateKey(NULL, &p, input_len))
            || !TEST_ptr_eq(p, input + input_len)
            || !TEST_int_eq(EVP_PKEY_id(pkey), expected_id))
        goto done;

    ret = 1;

 done:
    EVP_PKEY_free(pkey);
    return ret;
}

#ifndef OPENSSL_NO_EC

static const unsigned char ec_public_sect163k1_validxy[] = {
    0x30, 0x40, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x01, 0x03, 0x2c, 0x00, 0x04,
    0x02, 0x84, 0x58, 0xa6, 0xd4, 0xa0, 0x35, 0x2b, 0xae, 0xf0, 0xc0, 0x69,
    0x05, 0xcf, 0x2a, 0x50, 0x33, 0xf9, 0xe3, 0x92, 0x79, 0x02, 0xd1, 0x7b,
    0x9f, 0x22, 0x00, 0xf0, 0x3b, 0x0e, 0x5d, 0x2e, 0xb7, 0x23, 0x24, 0xf3,
    0x6a, 0xd8, 0x17, 0x65, 0x41, 0x2f
};

static const unsigned char ec_public_sect163k1_badx[] = {
    0x30, 0x40, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x01, 0x03, 0x2c, 0x00, 0x04,
    0x0a, 0x84, 0x58, 0xa6, 0xd4, 0xa0, 0x35, 0x2b, 0xae, 0xf0, 0xc0, 0x69,
    0x05, 0xcf, 0x2a, 0x50, 0x33, 0xf9, 0xe3, 0x92, 0xb0, 0x02, 0xd1, 0x7b,
    0x9f, 0x22, 0x00, 0xf0, 0x3b, 0x0e, 0x5d, 0x2e, 0xb7, 0x23, 0x24, 0xf3,
    0x6a, 0xd8, 0x17, 0x65, 0x41, 0x2f
};

static const unsigned char ec_public_sect163k1_bady[] = {
    0x30, 0x40, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x01, 0x03, 0x2c, 0x00, 0x04,
    0x02, 0x84, 0x58, 0xa6, 0xd4, 0xa0, 0x35, 0x2b, 0xae, 0xf0, 0xc0, 0x69,
    0x05, 0xcf, 0x2a, 0x50, 0x33, 0xf9, 0xe3, 0x92, 0x79, 0x0a, 0xd1, 0x7b,
    0x9f, 0x22, 0x00, 0xf0, 0x3b, 0x0e, 0x5d, 0x2e, 0xb7, 0x23, 0x24, 0xf3,
    0x6a, 0xd8, 0x17, 0x65, 0x41, 0xe6
};

static struct ec_der_pub_keys_st {
    const unsigned char *der;
    size_t len;
    int valid;
} ec_der_pub_keys[] = {
    { ec_public_sect163k1_validxy, sizeof(ec_public_sect163k1_validxy), 1 },
    { ec_public_sect163k1_badx, sizeof(ec_public_sect163k1_badx), 0 },
    { ec_public_sect163k1_bady, sizeof(ec_public_sect163k1_bady), 0 },
};

/*
 * Tests the range of the decoded EC char2 public point.
 * See ec_GF2m_simple_oct2point().
 */
static int test_invalide_ec_char2_pub_range_decode(int id)
{
    int ret = 0;
    BIO *bio = NULL;
    EC_KEY *eckey = NULL;

    if (!TEST_ptr(bio = BIO_new_mem_buf(ec_der_pub_keys[id].der,
                                        ec_der_pub_keys[id].len)))
        goto err;
    eckey = d2i_EC_PUBKEY_bio(bio, NULL);
    ret = (ec_der_pub_keys[id].valid && TEST_ptr(eckey))
          || TEST_ptr_null(eckey);
err:
    EC_KEY_free(eckey);
    BIO_free(bio);
    return ret;
}

/* Tests loading a bad key in PKCS8 format */
static int test_EVP_PKCS82PKEY(void)
{
    int ret = 0;
    const unsigned char *derp = kExampleBadECKeyDER;
    PKCS8_PRIV_KEY_INFO *p8inf = NULL;
    EVP_PKEY *pkey = NULL;

    if (!TEST_ptr(p8inf = d2i_PKCS8_PRIV_KEY_INFO(NULL, &derp,
                                              sizeof(kExampleBadECKeyDER))))
        goto done;

    if (!TEST_ptr_eq(derp,
                     kExampleBadECKeyDER + sizeof(kExampleBadECKeyDER)))
        goto done;

    if (!TEST_ptr_null(pkey = EVP_PKCS82PKEY(p8inf)))
        goto done;

    ret = 1;

 done:
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    EVP_PKEY_free(pkey);

    return ret;
}
#endif

#ifndef OPENSSL_NO_SM2

static int test_EVP_SM2_verify(void)
{
    /* From https://tools.ietf.org/html/draft-shen-sm2-ecdsa-02#appendix-A */
    const char *pubkey =
       "-----BEGIN PUBLIC KEY-----\n"
       "MIIBMzCB7AYHKoZIzj0CATCB4AIBATAsBgcqhkjOPQEBAiEAhULWnkwETxjouSQ1\n"
       "v2/33kVyg5FcRVF9ci7biwjx38MwRAQgeHlotPoyw/0kF4Quc7v+/y88hItoMdfg\n"
       "7GUiizk35JgEIGPkxtOyOwyEnPhCQUhL/kj2HVmlsWugbm4S0donxSSaBEEEQh3r\n"
       "1hti6rZ0ZDTrw8wxXjIiCzut1QvcTE5sFH/t1D0GgFEry7QsB9RzSdIVO3DE5df9\n"
       "/L+jbqGoWEG55G4JogIhAIVC1p5MBE8Y6LkkNb9v990pdyBjBIVijVrnTufDLnm3\n"
       "AgEBA0IABArkx3mKoPEZRxvuEYJb5GICu3nipYRElel8BP9N8lSKfAJA+I8c1OFj\n"
       "Uqc8F7fxbwc1PlOhdtaEqf4Ma7eY6Fc=\n"
       "-----END PUBLIC KEY-----\n";

    const char *msg = "message digest";
    const char *id = "ALICE123@YAHOO.COM";

    const uint8_t signature[] = {
       0x30, 0x44, 0x02, 0x20,

       0x40, 0xF1, 0xEC, 0x59, 0xF7, 0x93, 0xD9, 0xF4, 0x9E, 0x09, 0xDC,
       0xEF, 0x49, 0x13, 0x0D, 0x41, 0x94, 0xF7, 0x9F, 0xB1, 0xEE, 0xD2,
       0xCA, 0xA5, 0x5B, 0xAC, 0xDB, 0x49, 0xC4, 0xE7, 0x55, 0xD1,

       0x02, 0x20,

       0x6F, 0xC6, 0xDA, 0xC3, 0x2C, 0x5D, 0x5C, 0xF1, 0x0C, 0x77, 0xDF,
       0xB2, 0x0F, 0x7C, 0x2E, 0xB6, 0x67, 0xA4, 0x57, 0x87, 0x2F, 0xB0,
       0x9E, 0xC5, 0x63, 0x27, 0xA6, 0x7E, 0xC7, 0xDE, 0xEB, 0xE7
    };

    int rc = 0;
    BIO *bio = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY_CTX *pctx = NULL;

    bio = BIO_new_mem_buf(pubkey, strlen(pubkey));
    if (!TEST_true(bio != NULL))
        goto done;

    pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    if (!TEST_true(pkey != NULL))
        goto done;

    if (!TEST_true(EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2)))
        goto done;

    if (!TEST_ptr(mctx = EVP_MD_CTX_new()))
        goto done;

    if (!TEST_ptr(pctx = EVP_PKEY_CTX_new(pkey, NULL)))
        goto done;

    if (!TEST_int_gt(EVP_PKEY_CTX_set1_id(pctx, (const uint8_t *)id,
                                          strlen(id)), 0))
        goto done;

    EVP_MD_CTX_set_pkey_ctx(mctx, pctx);

    if (!TEST_true(EVP_DigestVerifyInit(mctx, NULL, EVP_sm3(), NULL, pkey)))
        goto done;

    if (!TEST_true(EVP_DigestVerifyUpdate(mctx, msg, strlen(msg))))
        goto done;

    if (!TEST_true(EVP_DigestVerifyFinal(mctx, signature, sizeof(signature))))
        goto done;
    rc = 1;

 done:
    BIO_free(bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    EVP_MD_CTX_free(mctx);
    return rc;
}

static int test_EVP_SM2(void)
{
    int ret = 0;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY *params = NULL;
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY_CTX *kctx = NULL;
    EVP_PKEY_CTX *sctx = NULL;
    size_t sig_len = 0;
    unsigned char *sig = NULL;
    EVP_MD_CTX *md_ctx = NULL;
    EVP_MD_CTX *md_ctx_verify = NULL;
    EVP_PKEY_CTX *cctx = NULL;

    uint8_t ciphertext[128];
    size_t ctext_len = sizeof(ciphertext);

    uint8_t plaintext[8];
    size_t ptext_len = sizeof(plaintext);

    uint8_t sm2_id[] = {1, 2, 3, 4, 'l', 'e', 't', 't', 'e', 'r'};

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!TEST_ptr(pctx))
        goto done;

    if (!TEST_true(EVP_PKEY_paramgen_init(pctx) == 1))
        goto done;

    if (!TEST_true(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_sm2)))
        goto done;

    if (!TEST_true(EVP_PKEY_paramgen(pctx, &params)))
        goto done;

    kctx = EVP_PKEY_CTX_new(params, NULL);
    if (!TEST_ptr(kctx))
        goto done;

    if (!TEST_true(EVP_PKEY_keygen_init(kctx)))
        goto done;

    if (!TEST_true(EVP_PKEY_keygen(kctx, &pkey)))
        goto done;

    if (!TEST_true(EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2)))
        goto done;

    if (!TEST_ptr(md_ctx = EVP_MD_CTX_new()))
        goto done;

    if (!TEST_ptr(md_ctx_verify = EVP_MD_CTX_new()))
        goto done;

    if (!TEST_ptr(sctx = EVP_PKEY_CTX_new(pkey, NULL)))
        goto done;

    EVP_MD_CTX_set_pkey_ctx(md_ctx, sctx);
    EVP_MD_CTX_set_pkey_ctx(md_ctx_verify, sctx);

    if (!TEST_int_gt(EVP_PKEY_CTX_set1_id(sctx, sm2_id, sizeof(sm2_id)), 0))
        goto done;

    if (!TEST_true(EVP_DigestSignInit(md_ctx, NULL, EVP_sm3(), NULL, pkey)))
        goto done;

    if(!TEST_true(EVP_DigestSignUpdate(md_ctx, kMsg, sizeof(kMsg))))
        goto done;

    /* Determine the size of the signature. */
    if (!TEST_true(EVP_DigestSignFinal(md_ctx, NULL, &sig_len)))
        goto done;

    if (!TEST_size_t_eq(sig_len, (size_t)EVP_PKEY_size(pkey)))
        goto done;

    if (!TEST_ptr(sig = OPENSSL_malloc(sig_len)))
        goto done;

    if (!TEST_true(EVP_DigestSignFinal(md_ctx, sig, &sig_len)))
        goto done;

    /* Ensure that the signature round-trips. */

    if (!TEST_true(EVP_DigestVerifyInit(md_ctx_verify, NULL, EVP_sm3(), NULL, pkey)))
        goto done;

    if (!TEST_true(EVP_DigestVerifyUpdate(md_ctx_verify, kMsg, sizeof(kMsg))))
        goto done;

    if (!TEST_true(EVP_DigestVerifyFinal(md_ctx_verify, sig, sig_len)))
        goto done;

    /* now check encryption/decryption */

    if (!TEST_ptr(cctx = EVP_PKEY_CTX_new(pkey, NULL)))
        goto done;

    if (!TEST_true(EVP_PKEY_encrypt_init(cctx)))
        goto done;

    if (!TEST_true(EVP_PKEY_encrypt(cctx, ciphertext, &ctext_len, kMsg, sizeof(kMsg))))
        goto done;

    if (!TEST_true(EVP_PKEY_decrypt_init(cctx)))
        goto done;

    if (!TEST_true(EVP_PKEY_decrypt(cctx, plaintext, &ptext_len, ciphertext, ctext_len)))
        goto done;

    if (!TEST_true(ptext_len == sizeof(kMsg)))
        goto done;

    if (!TEST_true(memcmp(plaintext, kMsg, sizeof(kMsg)) == 0))
        goto done;

    ret = 1;
done:
    EVP_PKEY_CTX_free(pctx);
    EVP_PKEY_CTX_free(kctx);
    EVP_PKEY_CTX_free(sctx);
    EVP_PKEY_CTX_free(cctx);
    EVP_PKEY_free(pkey);
    EVP_PKEY_free(params);
    EVP_MD_CTX_free(md_ctx);
    EVP_MD_CTX_free(md_ctx_verify);
    OPENSSL_free(sig);
    return ret;
}

#endif

static struct keys_st {
    int type;
    char *priv;
    char *pub;
} keys[] = {
    {
        EVP_PKEY_HMAC, "0123456789", NULL
#ifndef OPENSSL_NO_POLY1305
    }, {
        EVP_PKEY_POLY1305, "01234567890123456789012345678901", NULL
#endif
#ifndef OPENSSL_NO_SIPHASH
    }, {
        EVP_PKEY_SIPHASH, "0123456789012345", NULL
#endif
    },
#ifndef OPENSSL_NO_EC
    {
        EVP_PKEY_X25519, "01234567890123456789012345678901",
        "abcdefghijklmnopqrstuvwxyzabcdef"
    }, {
        EVP_PKEY_ED25519, "01234567890123456789012345678901",
        "abcdefghijklmnopqrstuvwxyzabcdef"
    }, {
        EVP_PKEY_X448,
        "01234567890123456789012345678901234567890123456789012345",
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcd"
    }, {
        EVP_PKEY_ED448,
        "012345678901234567890123456789012345678901234567890123456",
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcde"
    }
#endif
};

static int test_set_get_raw_keys_int(int tst, int pub)
{
    int ret = 0;
    unsigned char buf[80];
    unsigned char *in;
    size_t inlen, len = 0;
    EVP_PKEY *pkey;

    /* Check if this algorithm supports public keys */
    if (pub && keys[tst].pub == NULL)
        return 1;

    memset(buf, 0, sizeof(buf));

    if (pub) {
#ifndef OPENSSL_NO_EC
        inlen = strlen(keys[tst].pub);
        in = (unsigned char *)keys[tst].pub;
        pkey = EVP_PKEY_new_raw_public_key(keys[tst].type,
                                           NULL,
                                           in,
                                           inlen);
#else
        return 1;
#endif
    } else {
        inlen = strlen(keys[tst].priv);
        in = (unsigned char *)keys[tst].priv;
        pkey = EVP_PKEY_new_raw_private_key(keys[tst].type,
                                            NULL,
                                            in,
                                            inlen);
    }

    if (!TEST_ptr(pkey)
            || !TEST_int_eq(EVP_PKEY_cmp(pkey, pkey), 1)
            || (!pub && !TEST_true(EVP_PKEY_get_raw_private_key(pkey, NULL, &len)))
            || (pub && !TEST_true(EVP_PKEY_get_raw_public_key(pkey, NULL, &len)))
            || !TEST_true(len == inlen)
            || (!pub && !TEST_true(EVP_PKEY_get_raw_private_key(pkey, buf, &len)))
            || (pub && !TEST_true(EVP_PKEY_get_raw_public_key(pkey, buf, &len)))
            || !TEST_mem_eq(in, inlen, buf, len))
        goto done;

    ret = 1;
 done:
    EVP_PKEY_free(pkey);
    return ret;
}

static int test_set_get_raw_keys(int tst)
{
    return test_set_get_raw_keys_int(tst, 0)
           && test_set_get_raw_keys_int(tst, 1);
}

static int pkey_custom_check(EVP_PKEY *pkey)
{
    return 0xbeef;
}

static int pkey_custom_pub_check(EVP_PKEY *pkey)
{
    return 0xbeef;
}

static int pkey_custom_param_check(EVP_PKEY *pkey)
{
    return 0xbeef;
}

static EVP_PKEY_METHOD *custom_pmeth;

static int test_EVP_PKEY_check(int i)
{
    int ret = 0;
    const unsigned char *p;
    EVP_PKEY *pkey = NULL;
#ifndef OPENSSL_NO_EC
    EC_KEY *eckey = NULL;
#endif
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY_CTX *ctx2 = NULL;
    const APK_DATA *ak = &keycheckdata[i];
    const unsigned char *input = ak->kder;
    size_t input_len = ak->size;
    int expected_id = ak->evptype;
    int expected_check = ak->check;
    int expected_pub_check = ak->pub_check;
    int expected_param_check = ak->param_check;
    int type = ak->type;
    BIO *pubkey = NULL;

    p = input;

    switch (type) {
    case 0:
        if (!TEST_ptr(pkey = d2i_AutoPrivateKey(NULL, &p, input_len))
            || !TEST_ptr_eq(p, input + input_len)
            || !TEST_int_eq(EVP_PKEY_id(pkey), expected_id))
            goto done;
        break;
#ifndef OPENSSL_NO_EC
    case 1:
        if (!TEST_ptr(pubkey = BIO_new_mem_buf(input, input_len))
            || !TEST_ptr(eckey = d2i_EC_PUBKEY_bio(pubkey, NULL))
            || !TEST_ptr(pkey = EVP_PKEY_new())
            || !TEST_true(EVP_PKEY_assign_EC_KEY(pkey, eckey)))
            goto done;
        break;
    case 2:
        if (!TEST_ptr(eckey = d2i_ECParameters(NULL, &p, input_len))
            || !TEST_ptr_eq(p, input + input_len)
            || !TEST_ptr(pkey = EVP_PKEY_new())
            || !TEST_true(EVP_PKEY_assign_EC_KEY(pkey, eckey)))
            goto done;
        break;
#endif
    default:
        return 0;
    }

    if (!TEST_ptr(ctx = EVP_PKEY_CTX_new(pkey, NULL)))
        goto done;

    if (!TEST_int_eq(EVP_PKEY_check(ctx), expected_check))
        goto done;

    if (!TEST_int_eq(EVP_PKEY_public_check(ctx), expected_pub_check))
        goto done;

    if (!TEST_int_eq(EVP_PKEY_param_check(ctx), expected_param_check))
        goto done;

    ctx2 = EVP_PKEY_CTX_new_id(0xdefaced, NULL);
    /* assign the pkey directly, as an internal test */
    EVP_PKEY_up_ref(pkey);
    ctx2->pkey = pkey;

    if (!TEST_int_eq(EVP_PKEY_check(ctx2), 0xbeef))
        goto done;

    if (!TEST_int_eq(EVP_PKEY_public_check(ctx2), 0xbeef))
        goto done;

    if (!TEST_int_eq(EVP_PKEY_param_check(ctx2), 0xbeef))
        goto done;

    ret = 1;

 done:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_CTX_free(ctx2);
    EVP_PKEY_free(pkey);
    BIO_free(pubkey);
    return ret;
}

static int test_HKDF(void)
{
    EVP_PKEY_CTX *pctx;
    unsigned char out[20];
    size_t outlen;
    int i, ret = 0;
    unsigned char salt[] = "0123456789";
    unsigned char key[] = "012345678901234567890123456789";
    unsigned char info[] = "infostring";
    const unsigned char expected[] = {
        0xe5, 0x07, 0x70, 0x7f, 0xc6, 0x78, 0xd6, 0x54, 0x32, 0x5f, 0x7e, 0xc5,
        0x7b, 0x59, 0x3e, 0xd8, 0x03, 0x6b, 0xed, 0xca
    };
    size_t expectedlen = sizeof(expected);

    if (!TEST_ptr(pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL)))
        goto done;

    /* We do this twice to test reuse of the EVP_PKEY_CTX */
    for (i = 0; i < 2; i++) {
        outlen = sizeof(out);
        memset(out, 0, outlen);

        if (!TEST_int_gt(EVP_PKEY_derive_init(pctx), 0)
                || !TEST_int_gt(EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()), 0)
                || !TEST_int_gt(EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt,
                                                            sizeof(salt) - 1), 0)
                || !TEST_int_gt(EVP_PKEY_CTX_set1_hkdf_key(pctx, key,
                                                           sizeof(key) - 1), 0)
                || !TEST_int_gt(EVP_PKEY_CTX_add1_hkdf_info(pctx, info,
                                                            sizeof(info) - 1), 0)
                || !TEST_int_gt(EVP_PKEY_derive(pctx, out, &outlen), 0)
                || !TEST_mem_eq(out, outlen, expected, expectedlen))
            goto done;
    }

    ret = 1;

 done:
    EVP_PKEY_CTX_free(pctx);

    return ret;
}

static int test_emptyikm_HKDF(void)
{
    EVP_PKEY_CTX *pctx;
    unsigned char out[20];
    size_t outlen;
    int ret = 0;
    unsigned char salt[] = "9876543210";
    unsigned char key[] = "";
    unsigned char info[] = "stringinfo";
    const unsigned char expected[] = {
        0x68, 0x81, 0xa5, 0x3e, 0x5b, 0x9c, 0x7b, 0x6f, 0x2e, 0xec, 0xc8, 0x47,
        0x7c, 0xfa, 0x47, 0x35, 0x66, 0x82, 0x15, 0x30
    };
    size_t expectedlen = sizeof(expected);

    if (!TEST_ptr(pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL)))
        goto done;

    outlen = sizeof(out);
    memset(out, 0, outlen);

    if (!TEST_int_gt(EVP_PKEY_derive_init(pctx), 0)
            || !TEST_int_gt(EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()), 0)
            || !TEST_int_gt(EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt,
                                                        sizeof(salt) - 1), 0)
            || !TEST_int_gt(EVP_PKEY_CTX_set1_hkdf_key(pctx, key,
                                                       sizeof(key) - 1), 0)
            || !TEST_int_gt(EVP_PKEY_CTX_add1_hkdf_info(pctx, info,
                                                        sizeof(info) - 1), 0)
            || !TEST_int_gt(EVP_PKEY_derive(pctx, out, &outlen), 0)
            || !TEST_mem_eq(out, outlen, expected, expectedlen))
        goto done;

    ret = 1;

 done:
    EVP_PKEY_CTX_free(pctx);

    return ret;
}

#ifndef OPENSSL_NO_EC
static int test_X509_PUBKEY_inplace(void)
{
  int ret = 0;
  X509_PUBKEY *xp = NULL;
  const unsigned char *p = kExampleECPubKeyDER;
  size_t input_len = sizeof(kExampleECPubKeyDER);

  if (!TEST_ptr(xp = d2i_X509_PUBKEY(NULL, &p, input_len)))
    goto done;

  if (!TEST_ptr(X509_PUBKEY_get0(xp)))
    goto done;

  p = kExampleBadECPubKeyDER;
  input_len = sizeof(kExampleBadECPubKeyDER);

  if (!TEST_ptr(xp = d2i_X509_PUBKEY(&xp, &p, input_len)))
    goto done;

  if (!TEST_true(X509_PUBKEY_get0(xp) == NULL))
    goto done;

  ret = 1;

done:
  X509_PUBKEY_free(xp);
  return ret;
}
#endif

#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
static int test_decrypt_null_chunks(void)
{
    EVP_CIPHER_CTX* ctx = NULL;
    const unsigned char key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1
    };
    unsigned char iv[12] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
    };
    unsigned char msg[] = "It was the best of times, it was the worst of times";
    unsigned char ciphertext[80];
    unsigned char plaintext[80];
    /* We initialise tmp to a non zero value on purpose */
    int ctlen, ptlen, tmp = 99;
    int ret = 0;
    const int enc_offset = 10, dec_offset = 20;

    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new())
            || !TEST_true(EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL,
                                             key, iv))
            || !TEST_true(EVP_EncryptUpdate(ctx, ciphertext, &ctlen, msg,
                                            enc_offset))
            /* Deliberate add a zero length update */
            || !TEST_true(EVP_EncryptUpdate(ctx, ciphertext + ctlen, &tmp, NULL,
                                            0))
            || !TEST_int_eq(tmp, 0)
            || !TEST_true(EVP_EncryptUpdate(ctx, ciphertext + ctlen, &tmp,
                                            msg + enc_offset,
                                            sizeof(msg) - enc_offset))
            || !TEST_int_eq(ctlen += tmp, sizeof(msg))
            || !TEST_true(EVP_EncryptFinal(ctx, ciphertext + ctlen, &tmp))
            || !TEST_int_eq(tmp, 0))
        goto err;

    /* Deliberately initialise tmp to a non zero value */
    tmp = 99;
    if (!TEST_true(EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, key,
                                      iv))
            || !TEST_true(EVP_DecryptUpdate(ctx, plaintext, &ptlen, ciphertext,
                                            dec_offset))
            /*
             * Deliberately add a zero length update. We also deliberately do
             * this at a different offset than for encryption.
             */
            || !TEST_true(EVP_DecryptUpdate(ctx, plaintext + ptlen, &tmp, NULL,
                                            0))
            || !TEST_int_eq(tmp, 0)
            || !TEST_true(EVP_DecryptUpdate(ctx, plaintext + ptlen, &tmp,
                                            ciphertext + dec_offset,
                                            ctlen - dec_offset))
            || !TEST_int_eq(ptlen += tmp, sizeof(msg))
            || !TEST_true(EVP_DecryptFinal(ctx, plaintext + ptlen, &tmp))
            || !TEST_int_eq(tmp, 0)
            || !TEST_mem_eq(msg, sizeof(msg), plaintext, ptlen))
        goto err;

    ret = 1;
 err:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}
#endif /* !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305) */

#ifndef OPENSSL_NO_DH
static int test_EVP_PKEY_set1_DH(void)
{
    DH *x942dh, *pkcs3dh;
    EVP_PKEY *pkey1, *pkey2;
    int ret = 0;

    x942dh = DH_get_2048_256();
    pkcs3dh = DH_new_by_nid(NID_ffdhe2048);
    pkey1 = EVP_PKEY_new();
    pkey2 = EVP_PKEY_new();
    if (!TEST_ptr(x942dh)
            || !TEST_ptr(pkcs3dh)
            || !TEST_ptr(pkey1)
            || !TEST_ptr(pkey2))
        goto err;

    if(!TEST_true(EVP_PKEY_set1_DH(pkey1, x942dh))
            || !TEST_int_eq(EVP_PKEY_id(pkey1), EVP_PKEY_DHX))
        goto err;


    if(!TEST_true(EVP_PKEY_set1_DH(pkey2, pkcs3dh))
            || !TEST_int_eq(EVP_PKEY_id(pkey2), EVP_PKEY_DH))
        goto err;

    ret = 1;
 err:
    EVP_PKEY_free(pkey1);
    EVP_PKEY_free(pkey2);
    DH_free(x942dh);
    DH_free(pkcs3dh);

    return ret;
}
#endif /* OPENSSL_NO_DH */

typedef struct {
        int data;
} custom_dgst_ctx;

static int custom_md_init_called = 0;
static int custom_md_cleanup_called = 0;

static int custom_md_init(EVP_MD_CTX *ctx)
{
    custom_dgst_ctx *p = EVP_MD_CTX_md_data(ctx);

    if (p == NULL)
        return 0;

    custom_md_init_called++;
    return 1;
}

static int custom_md_cleanup(EVP_MD_CTX *ctx)
{
    custom_dgst_ctx *p = EVP_MD_CTX_md_data(ctx);

    if (p == NULL)
        /* Nothing to do */
        return 1;

    custom_md_cleanup_called++;
    return 1;
}

static int test_custom_md_meth(void)
{
    EVP_MD_CTX *mdctx = NULL;
    EVP_MD *tmp = NULL;
    char mess[] = "Test Message\n";
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    int testresult = 0;
    int nid;

    custom_md_init_called = custom_md_cleanup_called = 0;

    nid = OBJ_create("1.3.6.1.4.1.16604.998866.1", "custom-md", "custom-md");
    if (!TEST_int_ne(nid, NID_undef))
        goto err;
    tmp = EVP_MD_meth_new(nid, NID_undef);
    if (!TEST_ptr(tmp))
        goto err;

    if (!TEST_true(EVP_MD_meth_set_init(tmp, custom_md_init))
            || !TEST_true(EVP_MD_meth_set_cleanup(tmp, custom_md_cleanup))
            || !TEST_true(EVP_MD_meth_set_app_datasize(tmp,
                                                       sizeof(custom_dgst_ctx))))
        goto err;

    mdctx = EVP_MD_CTX_new();
    if (!TEST_ptr(mdctx)
               /*
                * Initing our custom md and then initing another md should
                * result in the init and cleanup functions of the custom md
                * from being called.
                */
            || !TEST_true(EVP_DigestInit_ex(mdctx, tmp, NULL))
            || !TEST_true(EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL))
            || !TEST_true(EVP_DigestUpdate(mdctx, mess, strlen(mess)))
            || !TEST_true(EVP_DigestFinal_ex(mdctx, md_value, &md_len))
            || !TEST_int_eq(custom_md_init_called, 1)
            || !TEST_int_eq(custom_md_cleanup_called, 1))
        goto err;

    testresult = 1;
 err:
    EVP_MD_CTX_free(mdctx);
    EVP_MD_meth_free(tmp);
    return testresult;
}

#if !defined(OPENSSL_NO_ENGINE) && !defined(OPENSSL_NO_DYNAMIC_ENGINE)
/* Test we can create a signature keys with an associated ENGINE */
static int test_signatures_with_engine(int tst)
{
    ENGINE *e;
    const char *engine_id = "dasync";
    EVP_PKEY *pkey = NULL;
    const unsigned char badcmackey[] = { 0x00, 0x01 };
    const unsigned char cmackey[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };
    const unsigned char ed25519key[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    const unsigned char msg[] = { 0x00, 0x01, 0x02, 0x03 };
    int testresult = 0;
    EVP_MD_CTX *ctx = NULL;
    unsigned char *mac = NULL;
    size_t maclen = 0;
    int ret;

#  ifdef OPENSSL_NO_CMAC
    /* Skip CMAC tests in a no-cmac build */
    if (tst <= 1)
        return 1;
#  endif

    if (!TEST_ptr(e = ENGINE_by_id(engine_id)))
        return 0;

    if (!TEST_true(ENGINE_init(e))) {
        ENGINE_free(e);
        return 0;
    }

    switch (tst) {
    case 0:
        pkey = EVP_PKEY_new_CMAC_key(e, cmackey, sizeof(cmackey),
                                     EVP_aes_128_cbc());
        break;
    case 1:
        pkey = EVP_PKEY_new_CMAC_key(e, badcmackey, sizeof(badcmackey),
                                     EVP_aes_128_cbc());
        break;
    case 2:
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, e, ed25519key,
                                            sizeof(ed25519key));
        break;
    default:
        TEST_error("Invalid test case");
        goto err;
    }
    if (tst == 1) {
        /*
         * In 1.1.1 CMAC keys will fail to during EVP_PKEY_new_CMAC_key() if the
         * key is bad. In later versions this isn't detected until later.
         */
        if (!TEST_ptr_null(pkey))
            goto err;
    } else {
        if (!TEST_ptr(pkey))
            goto err;
    }

    if (tst == 0 || tst == 1) {
        /*
         * We stop the test here for tests 0 and 1. The dasync engine doesn't
         * actually support CMAC in 1.1.1.
         */
        testresult = 1;
        goto err;
    }

    if (!TEST_ptr(ctx = EVP_MD_CTX_new()))
        goto err;

    ret = EVP_DigestSignInit(ctx, NULL, tst == 2 ? NULL : EVP_sha256(), NULL,
                             pkey);
    if (tst == 0) {
        if (!TEST_true(ret))
            goto err;

        if (!TEST_true(EVP_DigestSignUpdate(ctx, msg, sizeof(msg)))
                || !TEST_true(EVP_DigestSignFinal(ctx, NULL, &maclen)))
            goto err;

        if (!TEST_ptr(mac = OPENSSL_malloc(maclen)))
            goto err;

        if (!TEST_true(EVP_DigestSignFinal(ctx, mac, &maclen)))
            goto err;
    } else {
        /* We used a bad key. We expect a failure here */
        if (!TEST_false(ret))
            goto err;
    }

    testresult = 1;
 err:
    EVP_MD_CTX_free(ctx);
    OPENSSL_free(mac);
    EVP_PKEY_free(pkey);
    ENGINE_finish(e);
    ENGINE_free(e);

    return testresult;
}

static int test_cipher_with_engine(void)
{
    ENGINE *e;
    const char *engine_id = "dasync";
    const unsigned char keyiv[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
    };
    const unsigned char msg[] = { 0x00, 0x01, 0x02, 0x03 };
    int testresult = 0;
    EVP_CIPHER_CTX *ctx = NULL, *ctx2 = NULL;
    unsigned char buf[AES_BLOCK_SIZE];
    int len = 0;

    if (!TEST_ptr(e = ENGINE_by_id(engine_id)))
        return 0;

    if (!TEST_true(ENGINE_init(e))) {
        ENGINE_free(e);
        return 0;
    }

    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new())
            || !TEST_ptr(ctx2 = EVP_CIPHER_CTX_new()))
        goto err;

    if (!TEST_true(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), e, keyiv, keyiv)))
        goto err;

    /* Copy the ctx, and complete the operation with the new ctx */
    if (!TEST_true(EVP_CIPHER_CTX_copy(ctx2, ctx)))
        goto err;

    if (!TEST_true(EVP_EncryptUpdate(ctx2, buf, &len, msg, sizeof(msg)))
            || !TEST_true(EVP_EncryptFinal_ex(ctx2, buf + len, &len)))
        goto err;

    testresult = 1;
 err:
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_CTX_free(ctx2);
    ENGINE_finish(e);
    ENGINE_free(e);

    return testresult;
}
#endif /* !defined(OPENSSL_NO_ENGINE) && !defined(OPENSSL_NO_DYNAMIC_ENGINE) */

int setup_tests(void)
{
#if !defined(OPENSSL_NO_ENGINE) && !defined(OPENSSL_NO_DYNAMIC_ENGINE)
    ENGINE_load_builtin_engines();
#endif
    ADD_TEST(test_EVP_DigestSignInit);
    ADD_TEST(test_EVP_DigestVerifyInit);
    ADD_TEST(test_EVP_Enveloped);
    ADD_ALL_TESTS(test_d2i_AutoPrivateKey, OSSL_NELEM(keydata));
#ifndef OPENSSL_NO_EC
    ADD_TEST(test_EVP_PKCS82PKEY);
#endif
#ifndef OPENSSL_NO_SM2
    ADD_TEST(test_EVP_SM2);
    ADD_TEST(test_EVP_SM2_verify);
#endif
    ADD_ALL_TESTS(test_set_get_raw_keys, OSSL_NELEM(keys));
    custom_pmeth = EVP_PKEY_meth_new(0xdefaced, 0);
    if (!TEST_ptr(custom_pmeth))
        return 0;
    EVP_PKEY_meth_set_check(custom_pmeth, pkey_custom_check);
    EVP_PKEY_meth_set_public_check(custom_pmeth, pkey_custom_pub_check);
    EVP_PKEY_meth_set_param_check(custom_pmeth, pkey_custom_param_check);
    if (!TEST_int_eq(EVP_PKEY_meth_add0(custom_pmeth), 1))
        return 0;
    ADD_ALL_TESTS(test_EVP_PKEY_check, OSSL_NELEM(keycheckdata));
    ADD_TEST(test_HKDF);
    ADD_TEST(test_emptyikm_HKDF);
#ifndef OPENSSL_NO_EC
    ADD_TEST(test_X509_PUBKEY_inplace);
    ADD_ALL_TESTS(test_invalide_ec_char2_pub_range_decode,
                  OSSL_NELEM(ec_der_pub_keys));
#endif
#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
    ADD_TEST(test_decrypt_null_chunks);
#endif
#ifndef OPENSSL_NO_DH
    ADD_TEST(test_EVP_PKEY_set1_DH);
#endif

    ADD_ALL_TESTS(test_evp_init_seq, OSSL_NELEM(evp_init_tests));
    ADD_ALL_TESTS(test_evp_reset, OSSL_NELEM(evp_reset_tests));
    ADD_ALL_TESTS(test_gcm_reinit, OSSL_NELEM(gcm_reinit_tests));
    ADD_ALL_TESTS(test_evp_updated_iv, OSSL_NELEM(evp_updated_iv_tests));

    ADD_TEST(test_custom_md_meth);
#if !defined(OPENSSL_NO_ENGINE) && !defined(OPENSSL_NO_DYNAMIC_ENGINE)
# ifndef OPENSSL_NO_EC
    ADD_ALL_TESTS(test_signatures_with_engine, 3);
# else
    ADD_ALL_TESTS(test_signatures_with_engine, 2);
# endif
    ADD_TEST(test_cipher_with_engine);
#endif

    return 1;
}
