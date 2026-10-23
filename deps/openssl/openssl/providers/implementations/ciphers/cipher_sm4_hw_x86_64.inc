/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * x86_64 SM4 support for modes ecb, cbc, ofb, cfb, ctr.
 * This file is included by cipher_sm4_hw.c
 */

#define cipher_hw_x86_64_sm4_cbc    ossl_cipher_hw_generic_cbc
#define cipher_hw_x86_64_sm4_ecb    ossl_cipher_hw_generic_ecb
#define cipher_hw_x86_64_sm4_ofb128 ossl_cipher_hw_generic_ofb128
#define cipher_hw_x86_64_sm4_cfb128 ossl_cipher_hw_generic_cfb128
#define cipher_hw_x86_64_sm4_ctr    ossl_cipher_hw_generic_ctr

static int cipher_hw_x86_64_sm4_initkey(PROV_CIPHER_CTX *ctx,
                                        const unsigned char *key,
                                        size_t keylen)
{
    PROV_SM4_CTX *sctx = (PROV_SM4_CTX *)ctx;
    SM4_KEY *ks = &sctx->ks.ks;

    ctx->ks = ks;
    hw_x86_64_sm4_set_key(key, ks);
    if (ctx->enc
            || (ctx->mode != EVP_CIPH_ECB_MODE
                && ctx->mode != EVP_CIPH_CBC_MODE)) {
        ctx->block = (block128_f) hw_x86_64_sm4_encrypt;
        ctx->stream.cbc = NULL;
    } else {
        ctx->block = (block128_f) hw_x86_64_sm4_decrypt;
        ctx->stream.cbc = NULL;
    }

    return 1;
}

#define PROV_CIPHER_HW_declare(mode)                                                  \
    static const PROV_CIPHER_HW hw_x86_64_sm4_##mode = {                              \
                                                        cipher_hw_x86_64_sm4_initkey, \
                                                        cipher_hw_x86_64_sm4_##mode,  \
                                                        cipher_hw_sm4_copyctx         \
    };

#define PROV_CIPHER_HW_select(mode)                                      \
    if (HWSM4_CAPABLE_X86_64)                                            \
        return &hw_x86_64_sm4_##mode;
