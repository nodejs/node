/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/evp.h"

static const EVP_CIPHER n_cipher = {
    NID_undef,
    1, 0, 0, 0,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_enc_null(void)
{
    return &n_cipher;
}
