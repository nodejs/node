/*
 * Copyright 2017-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2017 Ribose Inc. All Rights Reserved.
 * Ported from Ribose contributions from Botan.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_SM4
#include "crypto/evp.h"

#define BLOCK_CIPHER_generic(nid, blocksize, ivlen, nmode, mode, MODE, flags) \
    static const EVP_CIPHER sm4_##mode = {                                    \
        nid##_##nmode, blocksize, 128 / 8, ivlen,                             \
        flags | EVP_CIPH_##MODE##_MODE,                                       \
        EVP_ORIG_GLOBAL                                                       \
    };                                                                        \
    const EVP_CIPHER *EVP_sm4_##mode(void)                                    \
    {                                                                         \
        return &sm4_##mode;                                                   \
    }

#define DEFINE_BLOCK_CIPHERS(nid, flags)                                                   \
    BLOCK_CIPHER_generic(nid, 16, 16, cbc, cbc, CBC, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)   \
    BLOCK_CIPHER_generic(nid, 16, 0, ecb, ecb, ECB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)    \
    BLOCK_CIPHER_generic(nid, 1, 16, ofb128, ofb, OFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, 1, 16, cfb128, cfb, CFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, 1, 16, ctr, ctr, CTR, flags)

DEFINE_BLOCK_CIPHERS(NID_sm4, 0)
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
