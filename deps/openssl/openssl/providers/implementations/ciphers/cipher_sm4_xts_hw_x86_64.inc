/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * x86_64 support for SM4 XTS.
 * This file is included by cipher_sm4_xts_hw.c
 */

static int hw_x86_64_sm4_xts_initkey(PROV_CIPHER_CTX *ctx,
                                     const unsigned char *key,
                                     size_t keylen)
{
    PROV_SM4_XTS_CTX *xctx = (PROV_SM4_XTS_CTX *)ctx;
    OSSL_xts_stream_fn stream_fn = NULL;
    OSSL_xts_stream_fn stream_gb_fn = NULL;

    XTS_SET_KEY_FN(hw_x86_64_sm4_set_key,
                   hw_x86_64_sm4_set_key,
                   hw_x86_64_sm4_encrypt,
                   hw_x86_64_sm4_decrypt,
                   stream_fn, stream_gb_fn);
    return 1;
}

static const PROV_CIPHER_HW hw_x86_64_sm4_xts = {
    hw_x86_64_sm4_xts_initkey,
    NULL,
    cipher_hw_sm4_xts_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_sm4_xts(size_t keybits)
{
    if (HWSM4_CAPABLE_X86_64)
        return &hw_x86_64_sm4_xts;
    else
        return &sm4_generic_xts;
}
