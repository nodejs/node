/*
 * Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"

#define BLOCK_CIPHER_generic(nid, keylen, blocksize, ivlen, nmode, mode, MODE, flags) \
    static const EVP_CIPHER camellia_##keylen##_##mode = {                            \
        nid##_##keylen##_##nmode,                                                     \
        blocksize,                                                                    \
        keylen / 8,                                                                   \
        ivlen,                                                                        \
        flags | EVP_CIPH_##MODE##_MODE,                                               \
        EVP_ORIG_GLOBAL,                                                              \
    };                                                                                \
    const EVP_CIPHER *EVP_camellia_##keylen##_##mode(void)                            \
    {                                                                                 \
        return &camellia_##keylen##_##mode;                                           \
    }

#define BLOCK_CIPHER_generic_pack(nid, keylen, flags)                                              \
    BLOCK_CIPHER_generic(nid, keylen, 16, 16, cbc, cbc, CBC, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)   \
    BLOCK_CIPHER_generic(nid, keylen, 16, 0, ecb, ecb, ECB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)    \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, ofb128, ofb, OFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb128, cfb, CFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb1, cfb1, CFB, flags)                               \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb8, cfb8, CFB, flags)                               \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, ctr, ctr, CTR, flags)

BLOCK_CIPHER_generic_pack(NID_camellia, 128, 0)
BLOCK_CIPHER_generic_pack(NID_camellia, 192, 0)
BLOCK_CIPHER_generic_pack(NID_camellia, 256, 0)
