/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Dispatch functions for AES CBC CTS ciphers */

#include <openssl/proverr.h>
#include "cipher_aes_cts.h"

#define AES_CTS_FLAGS PROV_CIPHER_FLAG_CTS

static OSSL_FUNC_cipher_encrypt_init_fn aes_cbc_cts_einit;
static OSSL_FUNC_cipher_decrypt_init_fn aes_cbc_cts_dinit;
static OSSL_FUNC_cipher_get_ctx_params_fn aes_cbc_cts_get_ctx_params;
static OSSL_FUNC_cipher_set_ctx_params_fn aes_cbc_cts_set_ctx_params;
static OSSL_FUNC_cipher_gettable_ctx_params_fn aes_cbc_cts_gettable_ctx_params;
static OSSL_FUNC_cipher_settable_ctx_params_fn aes_cbc_cts_settable_ctx_params;

CIPHER_DEFAULT_GETTABLE_CTX_PARAMS_START(aes_cbc_cts)
OSSL_PARAM_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE, NULL, 0),
CIPHER_DEFAULT_GETTABLE_CTX_PARAMS_END(aes_cbc_cts)

static int aes_cbc_cts_einit(void *ctx, const unsigned char *key, size_t keylen,
                             const unsigned char *iv, size_t ivlen,
                             const OSSL_PARAM params[])
{
    if (!ossl_cipher_generic_einit(ctx, key, keylen, iv, ivlen, NULL))
        return 0;
    return aes_cbc_cts_set_ctx_params(ctx, params);
}

static int aes_cbc_cts_dinit(void *ctx, const unsigned char *key, size_t keylen,
                             const unsigned char *iv, size_t ivlen,
                             const OSSL_PARAM params[])
{
    if (!ossl_cipher_generic_dinit(ctx, key, keylen, iv, ivlen, NULL))
        return 0;
    return aes_cbc_cts_set_ctx_params(ctx, params);
}

static int aes_cbc_cts_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    PROV_CIPHER_CTX *ctx = (PROV_CIPHER_CTX *)vctx;
    OSSL_PARAM *p;

    p = OSSL_PARAM_locate(params, OSSL_CIPHER_PARAM_CTS_MODE);
    if (p != NULL) {
        const char *name = ossl_aes_cbc_cts_mode_id2name(ctx->cts_mode);

        if (name == NULL || !OSSL_PARAM_set_utf8_string(p, name)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
            return 0;
        }
    }
    return ossl_cipher_generic_get_ctx_params(vctx, params);
}

CIPHER_DEFAULT_SETTABLE_CTX_PARAMS_START(aes_cbc_cts)
OSSL_PARAM_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE, NULL, 0),
CIPHER_DEFAULT_SETTABLE_CTX_PARAMS_END(aes_cbc_cts)

static int aes_cbc_cts_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    PROV_CIPHER_CTX *ctx = (PROV_CIPHER_CTX *)vctx;
    const OSSL_PARAM *p;
    int id;

    p = OSSL_PARAM_locate_const(params, OSSL_CIPHER_PARAM_CTS_MODE);
    if (p != NULL) {
        if (p->data_type != OSSL_PARAM_UTF8_STRING)
            goto err;
        id = ossl_aes_cbc_cts_mode_name2id(p->data);
        if (id < 0)
            goto err;

        ctx->cts_mode = (unsigned int)id;
    }
    return ossl_cipher_generic_set_ctx_params(vctx, params);
err:
    ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_SET_PARAMETER);
    return 0;
}

/* NOTE: The underlying block cipher is AES CBC so we reuse most of the code */
#define IMPLEMENT_cts_cipher(alg, UCALG, lcmode, UCMODE, flags, kbits,         \
                             blkbits, ivbits, typ)                             \
static OSSL_FUNC_cipher_get_params_fn alg##_##kbits##_##lcmode##_get_params;   \
static int alg##_cts_##kbits##_##lcmode##_get_params(OSSL_PARAM params[])      \
{                                                                              \
    return ossl_cipher_generic_get_params(params, EVP_CIPH_##UCMODE##_MODE, flags, \
                                          kbits, blkbits, ivbits);             \
}                                                                              \
const OSSL_DISPATCH ossl_##alg##kbits##lcmode##_cts_functions[] = {            \
    { OSSL_FUNC_CIPHER_NEWCTX,                                                 \
      (void (*)(void)) alg##_##kbits##_##lcmode##_newctx },                    \
    { OSSL_FUNC_CIPHER_FREECTX, (void (*)(void)) alg##_freectx },              \
    { OSSL_FUNC_CIPHER_DUPCTX, (void (*)(void)) alg##_dupctx },                \
    { OSSL_FUNC_CIPHER_ENCRYPT_INIT, (void (*)(void))aes_cbc_cts_einit },      \
    { OSSL_FUNC_CIPHER_DECRYPT_INIT, (void (*)(void))aes_cbc_cts_dinit },      \
    { OSSL_FUNC_CIPHER_UPDATE,                                                 \
      (void (*)(void)) ossl_##alg##_##lcmode##_cts_block_update },             \
    { OSSL_FUNC_CIPHER_FINAL,                                                  \
      (void (*)(void)) ossl_##alg##_##lcmode##_cts_block_final },              \
    { OSSL_FUNC_CIPHER_CIPHER, (void (*)(void))ossl_cipher_generic_cipher },   \
    { OSSL_FUNC_CIPHER_GET_PARAMS,                                             \
      (void (*)(void)) alg##_cts_##kbits##_##lcmode##_get_params },            \
    { OSSL_FUNC_CIPHER_GETTABLE_PARAMS,                                        \
      (void (*)(void))ossl_cipher_generic_gettable_params },                   \
    { OSSL_FUNC_CIPHER_GET_CTX_PARAMS,                                         \
      (void (*)(void))aes_cbc_cts_get_ctx_params },                            \
    { OSSL_FUNC_CIPHER_SET_CTX_PARAMS,                                         \
      (void (*)(void))aes_cbc_cts_set_ctx_params },                            \
    { OSSL_FUNC_CIPHER_GETTABLE_CTX_PARAMS,                                    \
      (void (*)(void))aes_cbc_cts_gettable_ctx_params },                       \
    { OSSL_FUNC_CIPHER_SETTABLE_CTX_PARAMS,                                    \
     (void (*)(void))aes_cbc_cts_settable_ctx_params },                        \
    { 0, NULL }                                                                \
};

/* ossl_aes256cbc_cts_functions */
IMPLEMENT_cts_cipher(aes, AES, cbc, CBC, AES_CTS_FLAGS, 256, 128, 128, block)
/* ossl_aes192cbc_cts_functions */
IMPLEMENT_cts_cipher(aes, AES, cbc, CBC, AES_CTS_FLAGS, 192, 128, 128, block)
/* ossl_aes128cbc_cts_functions */
IMPLEMENT_cts_cipher(aes, AES, cbc, CBC, AES_CTS_FLAGS, 128, 128, 128, block)
