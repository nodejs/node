/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#ifndef OPENSSL_NO_CAST
#include <openssl/cast.h>
#include "crypto/evp.h"

IMPLEMENT_BLOCK_CIPHER(cast5, ks, CAST, EVP_CAST_KEY,
    NID_cast5, 8, CAST_KEY_LENGTH, 8, 64,
    EVP_CIPH_VARIABLE_LENGTH)

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
