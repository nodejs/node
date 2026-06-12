/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "prov/ciphercommon.h"

#define ossl_prov_cipher_hw_aes_cfb ossl_prov_cipher_hw_aes_cfb128

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb128(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb1(size_t keybits);
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_cfb8(size_t keybits);
