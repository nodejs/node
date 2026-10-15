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

static const EVP_CIPHER d_xcbc_cipher = {
    NID_desx_cbc,
    8, 24, 8,
    EVP_CIPH_CBC_MODE,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_desx_cbc(void)
{
    return &d_xcbc_cipher;
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
