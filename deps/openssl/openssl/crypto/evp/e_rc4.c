/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_RC4
#include "crypto/evp.h"

static const EVP_CIPHER r4_cipher = {
    NID_rc4,
    1, EVP_RC4_KEY_SIZE, 0,
    EVP_CIPH_VARIABLE_LENGTH,
    EVP_ORIG_GLOBAL
};

static const EVP_CIPHER r4_40_cipher = {
    NID_rc4_40,
    1, 5 /* 40 bit */, 0,
    EVP_CIPH_VARIABLE_LENGTH,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_rc4(void)
{
    return &r4_cipher;
}

const EVP_CIPHER *EVP_rc4_40(void)
{
    return &r4_40_cipher;
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
