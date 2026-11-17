/*
 * Copyright 2001-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"

#define BLOCK_CIPHER_generic(nid, keylen, blocksize, ivlen, nmode, mode, MODE, flags) \
    static const EVP_CIPHER aes_##keylen##_##mode = {                                 \
        nid##_##keylen##_##nmode,                                                     \
        blocksize,                                                                    \
        keylen / 8,                                                                   \
        ivlen,                                                                        \
        flags | EVP_CIPH_##MODE##_MODE,                                               \
        EVP_ORIG_GLOBAL,                                                              \
    };                                                                                \
    const EVP_CIPHER *EVP_aes_##keylen##_##mode(void)                                 \
    {                                                                                 \
        return &aes_##keylen##_##mode;                                                \
    }

#define BLOCK_CIPHER_custom(nid, keylen, blocksize, ivlen, mode, MODE, flags)                                              \
    static const EVP_CIPHER aes_##keylen##_##mode = {                                                                      \
        nid##_##keylen##_##mode,                                                                                           \
        blocksize,                                                                                                         \
        (EVP_CIPH_##MODE##_MODE == EVP_CIPH_XTS_MODE || EVP_CIPH_##MODE##_MODE == EVP_CIPH_SIV_MODE ? 2 : 1) * keylen / 8, \
        ivlen,                                                                                                             \
        flags | EVP_CIPH_##MODE##_MODE,                                                                                    \
        EVP_ORIG_GLOBAL,                                                                                                   \
    };                                                                                                                     \
    const EVP_CIPHER *EVP_aes_##keylen##_##mode(void)                                                                      \
    {                                                                                                                      \
        return &aes_##keylen##_##mode;                                                                                     \
    }

#define BLOCK_CIPHER_generic_pack(nid, keylen, flags)                                              \
    BLOCK_CIPHER_generic(nid, keylen, 16, 16, cbc, cbc, CBC, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)   \
    BLOCK_CIPHER_generic(nid, keylen, 16, 0, ecb, ecb, ECB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1)    \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, ofb128, ofb, OFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb128, cfb, CFB, flags | EVP_CIPH_FLAG_DEFAULT_ASN1) \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb1, cfb1, CFB, flags)                               \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, cfb8, cfb8, CFB, flags)                               \
    BLOCK_CIPHER_generic(nid, keylen, 1, 16, ctr, ctr, CTR, flags)

BLOCK_CIPHER_generic_pack(NID_aes, 128, 0)
BLOCK_CIPHER_generic_pack(NID_aes, 192, 0)
BLOCK_CIPHER_generic_pack(NID_aes, 256, 0)

#define CUSTOM_FLAGS (EVP_CIPH_FLAG_DEFAULT_ASN1       \
    | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER \
    | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CTRL_INIT   \
    | EVP_CIPH_CUSTOM_COPY | EVP_CIPH_CUSTOM_IV_LENGTH)

BLOCK_CIPHER_custom(NID_aes, 128, 1, 12, gcm, GCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 192, 1, 12, gcm, GCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 256, 1, 12, gcm, GCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)

#define XTS_FLAGS (EVP_CIPH_FLAG_DEFAULT_ASN1 | EVP_CIPH_CUSTOM_IV \
    | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CTRL_INIT               \
    | EVP_CIPH_CUSTOM_COPY)

BLOCK_CIPHER_custom(NID_aes, 128, 1, 16, xts, XTS, XTS_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 256, 1, 16, xts, XTS, XTS_FLAGS)

BLOCK_CIPHER_custom(NID_aes, 128, 1, 12, ccm, CCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 192, 1, 12, ccm, CCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 256, 1, 12, ccm, CCM,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)

#define WRAP_FLAGS (EVP_CIPH_WRAP_MODE                 \
    | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER \
    | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_FLAG_DEFAULT_ASN1)

static const EVP_CIPHER aes_128_wrap = {
    NID_id_aes128_wrap,
    8, 16, 8, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_128_wrap(void)
{
    return &aes_128_wrap;
}

static const EVP_CIPHER aes_192_wrap = {
    NID_id_aes192_wrap,
    8, 24, 8, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_192_wrap(void)
{
    return &aes_192_wrap;
}

static const EVP_CIPHER aes_256_wrap = {
    NID_id_aes256_wrap,
    8, 32, 8, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_256_wrap(void)
{
    return &aes_256_wrap;
}

static const EVP_CIPHER aes_128_wrap_pad = {
    NID_id_aes128_wrap_pad,
    8, 16, 4, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_128_wrap_pad(void)
{
    return &aes_128_wrap_pad;
}

static const EVP_CIPHER aes_192_wrap_pad = {
    NID_id_aes192_wrap_pad,
    8, 24, 4, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_192_wrap_pad(void)
{
    return &aes_192_wrap_pad;
}

static const EVP_CIPHER aes_256_wrap_pad = {
    NID_id_aes256_wrap_pad,
    8, 32, 4, WRAP_FLAGS, EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_256_wrap_pad(void)
{
    return &aes_256_wrap_pad;
}

#ifndef OPENSSL_NO_OCB
BLOCK_CIPHER_custom(NID_aes, 128, 16, 12, ocb, OCB,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 192, 16, 12, ocb, OCB,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
BLOCK_CIPHER_custom(NID_aes, 256, 16, 12, ocb, OCB,
    EVP_CIPH_FLAG_AEAD_CIPHER | CUSTOM_FLAGS)
#endif /* OPENSSL_NO_OCB */
