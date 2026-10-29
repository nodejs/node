/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_RC5
#include "crypto/evp.h"
#include <openssl/rc5.h>

IMPLEMENT_BLOCK_CIPHER(rc5_32_12_16, ks, RC5_32, EVP_RC5_KEY, NID_rc5,
    8, RC5_32_KEY_LENGTH, 8, 64,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CTRL_INIT)

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
