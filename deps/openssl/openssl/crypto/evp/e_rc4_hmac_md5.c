/*
 * Copyright 2011-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/macros.h>

#if !defined(OPENSSL_NO_RC4) && !defined(OPENSSL_NO_MD5)
#include "crypto/evp.h"

static const EVP_CIPHER r4_hmac_md5_cipher = {
#ifdef NID_rc4_hmac_md5
    NID_rc4_hmac_md5,
#else
    NID_undef,
#endif
    1, EVP_RC4_KEY_SIZE, 0,
    EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_FLAG_AEAD_CIPHER,
    EVP_ORIG_GLOBAL
};

const EVP_CIPHER *EVP_rc4_hmac_md5(void)
{
    return &r4_hmac_md5_cipher;
}
#else
NON_EMPTY_TRANSLATION_UNIT
#endif
