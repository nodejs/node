/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"
#include "prov/blake2.h" /* diverse BLAKE2 macros */

static const EVP_MD blake2b_md = {
    NID_blake2b512,
    0,
    BLAKE2B_DIGEST_LENGTH,
    0,
    EVP_ORIG_GLOBAL,
    BLAKE2B_BLOCKBYTES,
};

const EVP_MD *EVP_blake2b512(void)
{
    return &blake2b_md;
}

static const EVP_MD blake2s_md = {
    NID_blake2s256,
    0,
    BLAKE2S_DIGEST_LENGTH,
    0,
    EVP_ORIG_GLOBAL,
    BLAKE2S_BLOCKBYTES,
};

const EVP_MD *EVP_blake2s256(void)
{
    return &blake2s_md;
}
