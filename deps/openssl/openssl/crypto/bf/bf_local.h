/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_BF_LOCAL_H
#define OSSL_CRYPTO_BF_LOCAL_H

#include <openssl/opensslconf.h>
#include "internal/common.h"

/*
 * This is actually a big endian algorithm, the most significant byte is used
 * to lookup array 0
 */

#define BF_ENC(LL, R, S, P) ( \
    LL ^= P,                  \
    LL ^= (((S[((R >> 24) & 0xff)] + S[0x0100 + ((R >> 16) & 0xff)]) ^ S[0x0200 + ((R >> 8) & 0xff)]) + S[0x0300 + ((R) & 0xff)]) & 0xffffffffU)

#endif
