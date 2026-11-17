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

BLOCK_CIPHER_defs(des, EVP_DES_KEY, NID_des, 8, 8, 8, 64,
    EVP_CIPH_RAND_KEY)

BLOCK_CIPHER_def_cfb(des, EVP_DES_KEY, NID_des, 8, 8, 1,
    EVP_CIPH_RAND_KEY)

BLOCK_CIPHER_def_cfb(des, EVP_DES_KEY, NID_des, 8, 8, 8,
    EVP_CIPH_RAND_KEY)

#else
NON_EMPTY_TRANSLATION_UNIT
#endif
