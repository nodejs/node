/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"
#include "prov/blake2.h"        /* diverse BLAKE2 macros */
#include "legacy_meth.h"

#define ossl_blake2b_init ossl_blake2b512_init
#define ossl_blake2s_init ossl_blake2s256_init

IMPLEMENT_LEGACY_EVP_MD_METH_LC(blake2s_int, ossl_blake2s)
IMPLEMENT_LEGACY_EVP_MD_METH_LC(blake2b_int, ossl_blake2b)

static const EVP_MD blake2b_md = {
    NID_blake2b512,
    0,
    BLAKE2B_DIGEST_LENGTH,
    0,
    EVP_ORIG_GLOBAL,
    LEGACY_EVP_MD_METH_TABLE(blake2b_int_init, blake2b_int_update,
                             blake2b_int_final, NULL, BLAKE2B_BLOCKBYTES),
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
    LEGACY_EVP_MD_METH_TABLE(blake2s_int_init, blake2s_int_update,
                             blake2s_int_final, NULL, BLAKE2S_BLOCKBYTES),
};

const EVP_MD *EVP_blake2s256(void)
{
    return &blake2s_md;
}
