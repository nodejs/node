/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <openssl/macros.h>

#ifndef OPENSSL_NO_RC2
#include "crypto/evp.h"
#include <openssl/rc2.h>

IMPLEMENT_BLOCK_CIPHER(rc2, ks, RC2, EVP_RC2_KEY, NID_rc2,
    8,
    RC2_KEY_LENGTH, 8, 64,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT)

static const EVP_CIPHER r2_64_cbc_cipher = {
    NID_rc2_64_cbc,
    8, 8 /* 64 bit */, 8,
    EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
    EVP_ORIG_GLOBAL
};

static const EVP_CIPHER r2_40_cbc_cipher = {
    NID_rc2_40_cbc,
    8, 5 /* 40 bit */, 8,
    EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_rc2_64_cbc(void)
{
    return &r2_64_cbc_cipher;
}

const EVP_CIPHER *EVP_rc2_40_cbc(void)
{
    return &r2_40_cbc_cipher;
}

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
