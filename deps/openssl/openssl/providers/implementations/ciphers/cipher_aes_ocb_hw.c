/*
 * Copyright 2019-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This file uses the low level AES functions (which are deprecated for
 * non-internal use) in order to implement provider AES ciphers.
 */
#include "internal/deprecated.h"

#include "cipher_aes_ocb.h"

#define OCB_SET_KEY_FN(fn_set_enc_key, fn_set_dec_key,                         \
                       fn_block_enc, fn_block_dec,                             \
                       fn_stream_enc, fn_stream_dec)                           \
CRYPTO_ocb128_cleanup(&ctx->ocb);                                              \
fn_set_enc_key(key, keylen * 8, &ctx->ksenc.ks);                               \
fn_set_dec_key(key, keylen * 8, &ctx->ksdec.ks);                               \
if (!CRYPTO_ocb128_init(&ctx->ocb, &ctx->ksenc.ks, &ctx->ksdec.ks,             \
                        (block128_f)fn_block_enc, (block128_f)fn_block_dec,    \
                        ctx->base.enc ? (ocb128_f)fn_stream_enc :              \
                                        (ocb128_f)fn_stream_dec))              \
    return 0;                                                                  \
ctx->key_set = 1


static int cipher_hw_aes_ocb_generic_initkey(PROV_CIPHER_CTX *vctx,
                                             const unsigned char *key,
                                             size_t keylen)
{
    PROV_AES_OCB_CTX *ctx = (PROV_AES_OCB_CTX *)vctx;

/*
 * We set both the encrypt and decrypt key here because decrypt
 * needs both. (i.e- AAD uses encrypt).
 */
# ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
        OCB_SET_KEY_FN(HWAES_set_encrypt_key, HWAES_set_decrypt_key,
                       HWAES_encrypt, HWAES_decrypt,
                       HWAES_ocb_encrypt, HWAES_ocb_decrypt);
    } else
# endif
# ifdef VPAES_CAPABLE
    if (VPAES_CAPABLE) {
        OCB_SET_KEY_FN(vpaes_set_encrypt_key, vpaes_set_decrypt_key,
                       vpaes_encrypt, vpaes_decrypt, NULL, NULL);
    } else
# endif
    {
        OCB_SET_KEY_FN(AES_set_encrypt_key, AES_set_decrypt_key,
                       AES_encrypt, AES_decrypt, NULL, NULL);
    }
    return 1;
}

# if defined(AESNI_CAPABLE)

static int cipher_hw_aes_ocb_aesni_initkey(PROV_CIPHER_CTX *vctx,
                                           const unsigned char *key,
                                           size_t keylen)
{
    PROV_AES_OCB_CTX *ctx = (PROV_AES_OCB_CTX *)vctx;

    OCB_SET_KEY_FN(aesni_set_encrypt_key, aesni_set_decrypt_key,
                   aesni_encrypt, aesni_decrypt,
                   aesni_ocb_encrypt, aesni_ocb_decrypt);
    return 1;
}

# define PROV_CIPHER_HW_declare()                                              \
static const PROV_CIPHER_HW aesni_ocb = {                                      \
    cipher_hw_aes_ocb_aesni_initkey,                                           \
    NULL                                                                       \
};
# define PROV_CIPHER_HW_select()                                               \
    if (AESNI_CAPABLE)                                                         \
        return &aesni_ocb;

#elif defined(SPARC_AES_CAPABLE)

static int cipher_hw_aes_ocb_t4_initkey(PROV_CIPHER_CTX *vctx,
                                        const unsigned char *key,
                                        size_t keylen)
{
    PROV_AES_OCB_CTX *ctx = (PROV_AES_OCB_CTX *)vctx;

    OCB_SET_KEY_FN(aes_t4_set_encrypt_key, aes_t4_set_decrypt_key,
                   aes_t4_encrypt, aes_t4_decrypt, NULL, NULL);
    return 1;
}

# define PROV_CIPHER_HW_declare()                                              \
static const PROV_CIPHER_HW aes_t4_ocb = {                                     \
    cipher_hw_aes_ocb_t4_initkey,                                              \
    NULL                                                                       \
};
# define PROV_CIPHER_HW_select()                                               \
    if (SPARC_AES_CAPABLE)                                                     \
        return &aes_t4_ocb;
#else
# define PROV_CIPHER_HW_declare()
# define PROV_CIPHER_HW_select()
# endif

static const PROV_CIPHER_HW aes_generic_ocb = {
    cipher_hw_aes_ocb_generic_initkey,
    NULL
};
PROV_CIPHER_HW_declare()
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_ocb(size_t keybits)
{
    PROV_CIPHER_HW_select()
    return &aes_generic_ocb;
}


