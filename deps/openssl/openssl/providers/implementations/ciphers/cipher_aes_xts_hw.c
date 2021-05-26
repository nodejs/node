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

#include "cipher_aes_xts.h"

#define XTS_SET_KEY_FN(fn_set_enc_key, fn_set_dec_key,                         \
                       fn_block_enc, fn_block_dec,                             \
                       fn_stream_enc, fn_stream_dec) {                         \
    size_t bytes = keylen / 2;                                                 \
    size_t bits = bytes * 8;                                                   \
                                                                               \
    if (ctx->enc) {                                                            \
        fn_set_enc_key(key, bits, &xctx->ks1.ks);                              \
        xctx->xts.block1 = (block128_f)fn_block_enc;                           \
    } else {                                                                   \
        fn_set_dec_key(key, bits, &xctx->ks1.ks);                              \
        xctx->xts.block1 = (block128_f)fn_block_dec;                           \
    }                                                                          \
    fn_set_enc_key(key + bytes, bits, &xctx->ks2.ks);                          \
    xctx->xts.block2 = (block128_f)fn_block_enc;                               \
    xctx->xts.key1 = &xctx->ks1;                                               \
    xctx->xts.key2 = &xctx->ks2;                                               \
    xctx->stream = ctx->enc ? fn_stream_enc : fn_stream_dec;                   \
}

static int cipher_hw_aes_xts_generic_initkey(PROV_CIPHER_CTX *ctx,
                                             const unsigned char *key,
                                             size_t keylen)
{
    PROV_AES_XTS_CTX *xctx = (PROV_AES_XTS_CTX *)ctx;
    OSSL_xts_stream_fn stream_enc = NULL;
    OSSL_xts_stream_fn stream_dec = NULL;

#ifdef AES_XTS_ASM
    stream_enc = AES_xts_encrypt;
    stream_dec = AES_xts_decrypt;
#endif /* AES_XTS_ASM */

#ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
# ifdef HWAES_xts_encrypt
        stream_enc = HWAES_xts_encrypt;
# endif /* HWAES_xts_encrypt */
# ifdef HWAES_xts_decrypt
        stream_dec = HWAES_xts_decrypt;
# endif /* HWAES_xts_decrypt */
        XTS_SET_KEY_FN(HWAES_set_encrypt_key, HWAES_set_decrypt_key,
                       HWAES_encrypt, HWAES_decrypt,
                       stream_enc, stream_dec);
        return 1;
    } else
#endif /* HWAES_CAPABLE */

#ifdef BSAES_CAPABLE
    if (BSAES_CAPABLE) {
        stream_enc = bsaes_xts_encrypt;
        stream_dec = bsaes_xts_decrypt;
    } else
#endif /* BSAES_CAPABLE */
#ifdef VPAES_CAPABLE
    if (VPAES_CAPABLE) {
        XTS_SET_KEY_FN(vpaes_set_encrypt_key, vpaes_set_decrypt_key,
                       vpaes_encrypt, vpaes_decrypt, stream_enc, stream_dec);
        return 1;
    } else
#endif /* VPAES_CAPABLE */
    {
        (void)0;
    }
    {
        XTS_SET_KEY_FN(AES_set_encrypt_key, AES_set_decrypt_key,
                       AES_encrypt, AES_decrypt, stream_enc, stream_dec);
    }
    return 1;
}

static void cipher_hw_aes_xts_copyctx(PROV_CIPHER_CTX *dst,
                                      const PROV_CIPHER_CTX *src)
{
    PROV_AES_XTS_CTX *sctx = (PROV_AES_XTS_CTX *)src;
    PROV_AES_XTS_CTX *dctx = (PROV_AES_XTS_CTX *)dst;

    *dctx = *sctx;
    dctx->xts.key1 = &dctx->ks1.ks;
    dctx->xts.key2 = &dctx->ks2.ks;
}

#if defined(AESNI_CAPABLE)

static int cipher_hw_aesni_xts_initkey(PROV_CIPHER_CTX *ctx,
                                       const unsigned char *key, size_t keylen)
{
    PROV_AES_XTS_CTX *xctx = (PROV_AES_XTS_CTX *)ctx;

    XTS_SET_KEY_FN(aesni_set_encrypt_key, aesni_set_decrypt_key,
                   aesni_encrypt, aesni_decrypt,
                   aesni_xts_encrypt, aesni_xts_decrypt);
    return 1;
}

# define PROV_CIPHER_HW_declare_xts()                                          \
static const PROV_CIPHER_HW aesni_xts = {                                      \
    cipher_hw_aesni_xts_initkey,                                               \
    NULL,                                                                      \
    cipher_hw_aes_xts_copyctx                                                  \
};
# define PROV_CIPHER_HW_select_xts()                                           \
if (AESNI_CAPABLE)                                                             \
    return &aesni_xts;

# elif defined(SPARC_AES_CAPABLE)

static int cipher_hw_aes_xts_t4_initkey(PROV_CIPHER_CTX *ctx,
                                        const unsigned char *key, size_t keylen)
{
    PROV_AES_XTS_CTX *xctx = (PROV_AES_XTS_CTX *)ctx;
    OSSL_xts_stream_fn stream_enc = NULL;
    OSSL_xts_stream_fn stream_dec = NULL;

    /* Note: keylen is the size of 2 keys */
    switch (keylen) {
    case 32:
        stream_enc = aes128_t4_xts_encrypt;
        stream_dec = aes128_t4_xts_decrypt;
        break;
    case 64:
        stream_enc = aes256_t4_xts_encrypt;
        stream_dec = aes256_t4_xts_decrypt;
        break;
    default:
        return 0;
    }

    XTS_SET_KEY_FN(aes_t4_set_encrypt_key, aes_t4_set_decrypt_key,
                   aes_t4_encrypt, aes_t4_decrypt,
                   stream_enc, stream_dec);
    return 1;
}

# define PROV_CIPHER_HW_declare_xts()                                          \
static const PROV_CIPHER_HW aes_xts_t4 = {                                     \
    cipher_hw_aes_xts_t4_initkey,                                              \
    NULL,                                                                      \
    cipher_hw_aes_xts_copyctx                                                  \
};
# define PROV_CIPHER_HW_select_xts()                                           \
if (SPARC_AES_CAPABLE)                                                         \
    return &aes_xts_t4;
# else
/* The generic case */
# define PROV_CIPHER_HW_declare_xts()
# define PROV_CIPHER_HW_select_xts()
#endif

static const PROV_CIPHER_HW aes_generic_xts = {
    cipher_hw_aes_xts_generic_initkey,
    NULL,
    cipher_hw_aes_xts_copyctx
};
PROV_CIPHER_HW_declare_xts()
const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts(size_t keybits)
{
    PROV_CIPHER_HW_select_xts()
    return &aes_generic_xts;
}
