/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
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

int ossl_cipher_set_aes_xts_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen,
    aes_set_encrypt_key_fn fn_set_enc_key,
    aes_set_encrypt_key_fn fn_set_dec_key,
    aes_block128_f fn_block_enc, aes_block128_f fn_block_dec,
    OSSL_xts_stream_fn fn_stream_enc, OSSL_xts_stream_fn fn_stream_dec)
{
    PROV_AES_XTS_CTX *xctx = (PROV_AES_XTS_CTX *)ctx;
    size_t bytes = keylen / 2;
    size_t bits = bytes * 8;

    if (ctx->enc) {
        fn_set_enc_key(key, (int)bits, &xctx->ks1.ks);
        xctx->xts.block1 = (block128_f)fn_block_enc;
    } else {
        fn_set_dec_key(key, (int)bits, &xctx->ks1.ks);
        xctx->xts.block1 = (block128_f)fn_block_dec;
    }
    fn_set_enc_key(key + bytes, (int)bits, &xctx->ks2.ks);
    xctx->xts.block2 = (block128_f)fn_block_enc;
    xctx->xts.key1 = &xctx->ks1;
    xctx->xts.key2 = &xctx->ks2;
    xctx->stream = ctx->enc ? fn_stream_enc : fn_stream_dec;

    return 1;
}

static int cipher_hw_aes_xts_generic_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key,
    size_t keylen)
{
    OSSL_xts_stream_fn stream_enc = NULL;
    OSSL_xts_stream_fn stream_dec = NULL;

#ifdef AES_XTS_ASM
    stream_enc = AES_xts_encrypt;
    stream_dec = AES_xts_decrypt;
#endif /* AES_XTS_ASM */

#ifdef HWAES_CAPABLE
    if (HWAES_CAPABLE) {
#ifdef HWAES_xts_encrypt
        stream_enc = HWAES_xts_encrypt;
#endif /* HWAES_xts_encrypt */
#ifdef HWAES_xts_decrypt
        stream_dec = HWAES_xts_decrypt;
#endif /* HWAES_xts_decrypt */
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            HWAES_set_encrypt_key, HWAES_set_decrypt_key,
            HWAES_encrypt, HWAES_decrypt, stream_enc, stream_dec);
    }
#endif /* HWAES_CAPABLE */

#ifdef BSAES_CAPABLE
    if (BSAES_CAPABLE) {
        stream_enc = ossl_bsaes_xts_encrypt;
        stream_dec = ossl_bsaes_xts_decrypt;
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            AES_set_encrypt_key, AES_set_decrypt_key,
            AES_encrypt, AES_decrypt, stream_enc, stream_dec);
    }
#endif /* BSAES_CAPABLE */

#ifdef VPAES_CAPABLE
    if (VPAES_CAPABLE) {
        return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
            vpaes_set_encrypt_key, vpaes_set_decrypt_key,
            vpaes_encrypt, vpaes_decrypt, stream_enc, stream_dec);
    }
#endif /* VPAES_CAPABLE */

    return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
        AES_set_encrypt_key, AES_set_decrypt_key,
        AES_encrypt, AES_decrypt, stream_enc, stream_dec);
}

void ossl_cipher_hw_aes_xts_copyctx(PROV_CIPHER_CTX *dst,
    const PROV_CIPHER_CTX *src)
{
    PROV_AES_XTS_CTX *sctx = (PROV_AES_XTS_CTX *)src;
    PROV_AES_XTS_CTX *dctx = (PROV_AES_XTS_CTX *)dst;

    *dctx = *sctx;
    dctx->xts.key1 = &dctx->ks1.ks;
    dctx->xts.key2 = &dctx->ks2.ks;
}

static const PROV_CIPHER_HW aes_generic_xts = {
    cipher_hw_aes_xts_generic_initkey,
    NULL,
    ossl_cipher_hw_aes_xts_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts(size_t keybits)
{
    const PROV_CIPHER_HW *aes_xts_hw = NULL;

#if defined(AESNI_CAPABLE)
    aes_xts_hw = ossl_prov_cipher_hw_aes_xts_aesni();
#elif defined(SPARC_AES_CAPABLE)
    aes_xts_hw = ossl_prov_cipher_hw_aes_xts_t4();
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 32
    aes_xts_hw = ossl_prov_cipher_hw_aes_xts_rv32i();
#elif defined(OPENSSL_CPUID_OBJ) && defined(__riscv) && __riscv_xlen == 64
    aes_xts_hw = ossl_prov_cipher_hw_aes_xts_rv64i();
#elif defined(AES_XTS_S390X)
    aes_xts_hw = ossl_prov_cipher_hw_aes_xts_s390x(keybits);
#endif

    if (aes_xts_hw == NULL)
        return &aes_generic_xts;

    return aes_xts_hw;
}
