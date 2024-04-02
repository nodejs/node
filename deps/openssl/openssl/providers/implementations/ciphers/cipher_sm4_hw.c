/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "cipher_sm4.h"

static int cipher_hw_sm4_initkey(PROV_CIPHER_CTX *ctx,
                                 const unsigned char *key, size_t keylen)
{
    PROV_SM4_CTX *sctx =  (PROV_SM4_CTX *)ctx;
    SM4_KEY *ks = &sctx->ks.ks;

    ossl_sm4_set_key(key, ks);
    ctx->ks = ks;
    if (ctx->enc
            || (ctx->mode != EVP_CIPH_ECB_MODE
                && ctx->mode != EVP_CIPH_CBC_MODE))
        ctx->block = (block128_f)ossl_sm4_encrypt;
    else
        ctx->block = (block128_f)ossl_sm4_decrypt;
    return 1;
}

IMPLEMENT_CIPHER_HW_COPYCTX(cipher_hw_sm4_copyctx, PROV_SM4_CTX)

# define PROV_CIPHER_HW_sm4_mode(mode)                                         \
static const PROV_CIPHER_HW sm4_##mode = {                                     \
    cipher_hw_sm4_initkey,                                                     \
    ossl_cipher_hw_chunked_##mode,                                             \
    cipher_hw_sm4_copyctx                                                      \
};                                                                             \
const PROV_CIPHER_HW *ossl_prov_cipher_hw_sm4_##mode(size_t keybits)           \
{                                                                              \
    return &sm4_##mode;                                                        \
}

PROV_CIPHER_HW_sm4_mode(cbc)
PROV_CIPHER_HW_sm4_mode(ecb)
PROV_CIPHER_HW_sm4_mode(ofb128)
PROV_CIPHER_HW_sm4_mode(cfb128)
PROV_CIPHER_HW_sm4_mode(ctr)
