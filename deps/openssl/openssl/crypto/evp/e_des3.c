/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_DES
#include "crypto/evp.h"

BLOCK_CIPHER_defs(des_ede, DES_EDE_KEY, NID_des_ede, 8, 16, 8, 64,
    EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_DEFAULT_ASN1)

BLOCK_CIPHER_defs(des_ede3, DES_EDE_KEY, NID_des_ede3, 8, 24, 8, 64,
    EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_DEFAULT_ASN1)

BLOCK_CIPHER_def_cfb(des_ede3, DES_EDE_KEY, NID_des_ede3, 24, 8, 1,
    EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_DEFAULT_ASN1)

BLOCK_CIPHER_def_cfb(des_ede3, DES_EDE_KEY, NID_des_ede3, 24, 8, 8,
    EVP_CIPH_RAND_KEY | EVP_CIPH_FLAG_DEFAULT_ASN1)

const EVP_CIPHER *EVP_des_ede(void)
{
    return &des_ede_ecb;
}

const EVP_CIPHER *EVP_des_ede3(void)
{
    return &des_ede3_ecb;
}

static const EVP_CIPHER des3_wrap = {
    NID_id_smime_alg_CMS3DESwrap,
    8, 24, 0,
    EVP_CIPH_WRAP_MODE | EVP_CIPH_CUSTOM_IV | EVP_CIPH_FLAG_CUSTOM_CIPHER
        | EVP_CIPH_FLAG_DEFAULT_ASN1,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_des_ede3_wrap(void)
{
    return &des3_wrap;
}

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
