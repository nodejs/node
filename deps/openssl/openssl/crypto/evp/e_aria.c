/*
 * Copyright 2017-2026 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2017, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_ARIA
#include "crypto/evp.h"

IMPLEMENT_BLOCK_CIPHER(aria_128, ks, aria, EVP_ARIA_KEY,
    NID_aria_128, 16, 16, 16, 128,
    0)
IMPLEMENT_BLOCK_CIPHER(aria_192, ks, aria, EVP_ARIA_KEY,
    NID_aria_192, 16, 24, 16, 128,
    0)
IMPLEMENT_BLOCK_CIPHER(aria_256, ks, aria, EVP_ARIA_KEY,
    NID_aria_256, 16, 32, 16, 128,
    0)

#define IMPLEMENT_ARIA_CFBR(ksize, cbits) \
    IMPLEMENT_CFBR(aria, aria, EVP_ARIA_KEY, ks, ksize, cbits, 16, 0)
IMPLEMENT_ARIA_CFBR(128, 1)
IMPLEMENT_ARIA_CFBR(192, 1)
IMPLEMENT_ARIA_CFBR(256, 1)
IMPLEMENT_ARIA_CFBR(128, 8)
IMPLEMENT_ARIA_CFBR(192, 8)
IMPLEMENT_ARIA_CFBR(256, 8)

#define BLOCK_CIPHER_generic(nid, keylen, blocksize, ivlen, nmode, mode, MODE, flags) \
    static const EVP_CIPHER aria_##keylen##_##mode = {                                \
        nid##_##keylen##_##nmode, blocksize, keylen / 8, ivlen,                       \
        flags | EVP_CIPH_##MODE##_MODE,                                               \
        EVP_ORIG_GLOBAL                                                               \
    };                                                                                \
    const EVP_CIPHER *EVP_aria_##keylen##_##mode(void)                                \
    {                                                                                 \
        return &aria_##keylen##_##mode;                                               \
    }

BLOCK_CIPHER_generic(NID_aria, 128, 1, 16, ctr, ctr, CTR, 0)
BLOCK_CIPHER_generic(NID_aria, 192, 1, 16, ctr, ctr, CTR, 0)
BLOCK_CIPHER_generic(NID_aria, 256, 1, 16, ctr, ctr, CTR, 0)

#define ARIA_AUTH_FLAGS (EVP_CIPH_FLAG_DEFAULT_ASN1    \
    | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER \
    | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CTRL_INIT   \
    | EVP_CIPH_CUSTOM_COPY | EVP_CIPH_FLAG_AEAD_CIPHER \
    | EVP_CIPH_CUSTOM_IV_LENGTH)

#define BLOCK_CIPHER_aead(keylen, mode, MODE)          \
    static const EVP_CIPHER aria_##keylen##_##mode = { \
        NID_aria_##keylen##_##mode,                    \
        1, keylen / 8, 12,                             \
        ARIA_AUTH_FLAGS | EVP_CIPH_##MODE##_MODE,      \
        EVP_ORIG_GLOBAL                                \
    };                                                 \
    const EVP_CIPHER *EVP_aria_##keylen##_##mode(void) \
    {                                                  \
        return (EVP_CIPHER *)&aria_##keylen##_##mode;  \
    }

BLOCK_CIPHER_aead(128, gcm, GCM)
BLOCK_CIPHER_aead(192, gcm, GCM)
BLOCK_CIPHER_aead(256, gcm, GCM)

BLOCK_CIPHER_aead(128, ccm, CCM)
BLOCK_CIPHER_aead(192, ccm, CCM)
BLOCK_CIPHER_aead(256, ccm, CCM)

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
