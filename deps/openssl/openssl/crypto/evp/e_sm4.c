/*
 * Copyright 2017-2021 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright 2017 Ribose Inc. All Rights Reserved.
 * Ported from Ribose contributions from Botan.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/deprecated.h"

#include "internal/cryptlib.h"
#ifndef OPENSSL_NO_SM4
# include <openssl/evp.h>
# include <openssl/modes.h>
# include "crypto/sm4.h"
# include "crypto/evp.h"
# include "evp_local.h"

typedef struct {
    SM4_KEY ks;
} EVP_SM4_KEY;

static int sm4_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                        const unsigned char *iv, int enc)
{
    ossl_sm4_set_key(key, EVP_CIPHER_CTX_get_cipher_data(ctx));
    return 1;
}

static void sm4_cbc_encrypt(const unsigned char *in, unsigned char *out,
                            size_t len, const SM4_KEY *key,
                            unsigned char *ivec, const int enc)
{
    if (enc)
        CRYPTO_cbc128_encrypt(in, out, len, key, ivec,
                              (block128_f)ossl_sm4_encrypt);
    else
        CRYPTO_cbc128_decrypt(in, out, len, key, ivec,
                              (block128_f)ossl_sm4_decrypt);
}

static void sm4_cfb128_encrypt(const unsigned char *in, unsigned char *out,
                               size_t length, const SM4_KEY *key,
                               unsigned char *ivec, int *num, const int enc)
{
    CRYPTO_cfb128_encrypt(in, out, length, key, ivec, num, enc,
                          (block128_f)ossl_sm4_encrypt);
}

static void sm4_ecb_encrypt(const unsigned char *in, unsigned char *out,
                            const SM4_KEY *key, const int enc)
{
    if (enc)
        ossl_sm4_encrypt(in, out, key);
    else
        ossl_sm4_decrypt(in, out, key);
}

static void sm4_ofb128_encrypt(const unsigned char *in, unsigned char *out,
                               size_t length, const SM4_KEY *key,
                               unsigned char *ivec, int *num)
{
    CRYPTO_ofb128_encrypt(in, out, length, key, ivec, num,
                          (block128_f)ossl_sm4_encrypt);
}

IMPLEMENT_BLOCK_CIPHER(sm4, ks, sm4, EVP_SM4_KEY, NID_sm4,
                       16, 16, 16, 128, EVP_CIPH_FLAG_DEFAULT_ASN1,
                       sm4_init_key, 0, 0, 0, 0)

static int sm4_ctr_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                          const unsigned char *in, size_t len)
{
    int n = EVP_CIPHER_CTX_get_num(ctx);
    unsigned int num;
    EVP_SM4_KEY *dat = EVP_C_DATA(EVP_SM4_KEY, ctx);

    if (n < 0)
        return 0;
    num = (unsigned int)n;

    CRYPTO_ctr128_encrypt(in, out, len, &dat->ks, ctx->iv,
                          EVP_CIPHER_CTX_buf_noconst(ctx), &num,
                          (block128_f)ossl_sm4_encrypt);
    EVP_CIPHER_CTX_set_num(ctx, num);
    return 1;
}

static const EVP_CIPHER sm4_ctr_mode = {
    NID_sm4_ctr, 1, 16, 16,
    EVP_CIPH_CTR_MODE,
    EVP_ORIG_GLOBAL,
    sm4_init_key,
    sm4_ctr_cipher,
    NULL,
    sizeof(EVP_SM4_KEY),
    NULL, NULL, NULL, NULL
};

const EVP_CIPHER *EVP_sm4_ctr(void)
{
    return &sm4_ctr_mode;
}

#endif
