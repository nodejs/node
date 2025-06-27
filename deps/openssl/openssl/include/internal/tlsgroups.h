/*
 * Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_TLSGROUPS_H
# define OSSL_INTERNAL_TLSGROUPS_H
# pragma once

# define OSSL_TLS_GROUP_ID_sect163k1        0x0001
# define OSSL_TLS_GROUP_ID_sect163r1        0x0002
# define OSSL_TLS_GROUP_ID_sect163r2        0x0003
# define OSSL_TLS_GROUP_ID_sect193r1        0x0004
# define OSSL_TLS_GROUP_ID_sect193r2        0x0005
# define OSSL_TLS_GROUP_ID_sect233k1        0x0006
# define OSSL_TLS_GROUP_ID_sect233r1        0x0007
# define OSSL_TLS_GROUP_ID_sect239k1        0x0008
# define OSSL_TLS_GROUP_ID_sect283k1        0x0009
# define OSSL_TLS_GROUP_ID_sect283r1        0x000A
# define OSSL_TLS_GROUP_ID_sect409k1        0x000B
# define OSSL_TLS_GROUP_ID_sect409r1        0x000C
# define OSSL_TLS_GROUP_ID_sect571k1        0x000D
# define OSSL_TLS_GROUP_ID_sect571r1        0x000E
# define OSSL_TLS_GROUP_ID_secp160k1        0x000F
# define OSSL_TLS_GROUP_ID_secp160r1        0x0010
# define OSSL_TLS_GROUP_ID_secp160r2        0x0011
# define OSSL_TLS_GROUP_ID_secp192k1        0x0012
# define OSSL_TLS_GROUP_ID_secp192r1        0x0013
# define OSSL_TLS_GROUP_ID_secp224k1        0x0014
# define OSSL_TLS_GROUP_ID_secp224r1        0x0015
# define OSSL_TLS_GROUP_ID_secp256k1        0x0016
# define OSSL_TLS_GROUP_ID_secp256r1        0x0017
# define OSSL_TLS_GROUP_ID_secp384r1        0x0018
# define OSSL_TLS_GROUP_ID_secp521r1        0x0019
# define OSSL_TLS_GROUP_ID_brainpoolP256r1  0x001A
# define OSSL_TLS_GROUP_ID_brainpoolP384r1  0x001B
# define OSSL_TLS_GROUP_ID_brainpoolP512r1  0x001C
# define OSSL_TLS_GROUP_ID_x25519           0x001D
# define OSSL_TLS_GROUP_ID_x448             0x001E
# define OSSL_TLS_GROUP_ID_ffdhe2048        0x0100
# define OSSL_TLS_GROUP_ID_ffdhe3072        0x0101
# define OSSL_TLS_GROUP_ID_ffdhe4096        0x0102
# define OSSL_TLS_GROUP_ID_ffdhe6144        0x0103
# define OSSL_TLS_GROUP_ID_ffdhe8192        0x0104

#endif
