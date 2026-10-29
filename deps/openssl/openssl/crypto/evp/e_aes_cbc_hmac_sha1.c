/*
 * Copyright 2011-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/aes.h>
#include "internal/cryptlib.h"
#include "crypto/evp.h"

#if defined(AES_ASM) && (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))

#define AESNI_CAPABLE (1 << (57 - 32))

static const EVP_CIPHER aesni_128_cbc_hmac_sha1_cipher = {
#ifdef NID_aes_128_cbc_hmac_sha1
    NID_aes_128_cbc_hmac_sha1,
#else
    NID_undef,
#endif
    AES_BLOCK_SIZE, 16, AES_BLOCK_SIZE,
    EVP_CIPH_CBC_MODE | EVP_CIPH_FLAG_DEFAULT_ASN1 | EVP_CIPH_FLAG_AEAD_CIPHER | EVP_CIPH_FLAG_TLS1_1_MULTIBLOCK,
    EVP_ORIG_GLOBAL
};

static const EVP_CIPHER aesni_256_cbc_hmac_sha1_cipher = {
#ifdef NID_aes_256_cbc_hmac_sha1
    NID_aes_256_cbc_hmac_sha1,
#else
    NID_undef,
#endif
    AES_BLOCK_SIZE, 32, AES_BLOCK_SIZE,
    EVP_CIPH_CBC_MODE | EVP_CIPH_FLAG_DEFAULT_ASN1 | EVP_CIPH_FLAG_AEAD_CIPHER | EVP_CIPH_FLAG_TLS1_1_MULTIBLOCK,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_aes_128_cbc_hmac_sha1(void)
{
    return (OPENSSL_ia32cap_P[1] & AESNI_CAPABLE ? &aesni_128_cbc_hmac_sha1_cipher : NULL);
}

const EVP_CIPHER *EVP_aes_256_cbc_hmac_sha1(void)
{
    return (OPENSSL_ia32cap_P[1] & AESNI_CAPABLE ? &aesni_256_cbc_hmac_sha1_cipher : NULL);
}
#else
const EVP_CIPHER *EVP_aes_128_cbc_hmac_sha1(void)
{
    return NULL;
}

const EVP_CIPHER *EVP_aes_256_cbc_hmac_sha1(void)
{
    return NULL;
}
#endif
