/*
 * Copyright 2001-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * AES-NI and VAES support for AES CFB mode.
 * This file is included by cipher_aes_cfb_hw.c
 */

#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
    #define cipher_hw_vaes_cfb128 aes_cfb128_vaes_encdec_wrapper
#else
    #define cipher_hw_vaes_cfb128 ossl_cipher_hw_generic_cfb128
    int ossl_aes_cfb128_vaes_eligible() {
        return 0;
    }
#endif
#define cipher_hw_vaes_cfb8    ossl_cipher_hw_generic_cfb8
#define cipher_hw_vaes_cfb1    ossl_cipher_hw_generic_cfb1

#define cipher_hw_aesni_cfb128 ossl_cipher_hw_generic_cfb128
#define cipher_hw_aesni_cfb8   ossl_cipher_hw_generic_cfb8
#define cipher_hw_aesni_cfb1   ossl_cipher_hw_generic_cfb1

static int ossl_aes_cfb8_vaes_eligible(void) { return 0; }
static int ossl_aes_cfb1_vaes_eligible(void) { return 0; }

#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
/* active in 64-bit builds when AES-NI, AVX512F, and VAES are detected */
static int aes_cfb128_vaes_encdec_wrapper(
    PROV_CIPHER_CTX* dat,
    unsigned char *out,
    const unsigned char *in,
    size_t len)
{
    ossl_ssize_t num;

    num = (ossl_ssize_t)dat->num;

    if (num < 0) {
        /* behavior from CRYPTO_cfb128_encrypt */
        dat->num = -1;
        return 1;
    }

    if (dat->enc)
        ossl_aes_cfb128_vaes_enc(in, out, len, dat->ks, dat->iv, &num);
    else
        ossl_aes_cfb128_vaes_dec(in, out, len, dat->ks, dat->iv, &num);

    dat->num = (int)num;

    return 1;
}
#endif
 
/* generates AES round keys for AES-NI and VAES implementations */
static int cipher_hw_aesni_initkey(PROV_CIPHER_CTX *dat,
                                   const unsigned char *key, size_t keylen)
{
    int ret;
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    AES_KEY *ks = &adat->ks.ks;

    dat->ks = ks;

    ret = aesni_set_encrypt_key(key, (int)(keylen * 8), ks);

    dat->block = (block128_f) aesni_encrypt;
    dat->stream.cbc = NULL;

    if (ret < 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SETUP_FAILED);
        return 0;
    }

    return 1;
}

#define PROV_CIPHER_HW_declare(mode)                                           \
static const PROV_CIPHER_HW aesni_##mode = {                                   \
    cipher_hw_aesni_initkey,                                                   \
    cipher_hw_aesni_##mode,                                                    \
    cipher_hw_aes_copyctx                                                      \
};                                                                             \
static const PROV_CIPHER_HW vaes_##mode = {                                    \
    cipher_hw_aesni_initkey,                                                   \
    cipher_hw_vaes_##mode,                                                     \
    cipher_hw_aes_copyctx                                                      \
};
#define PROV_CIPHER_HW_select(mode)                                            \
if (AESNI_CAPABLE) {                                                           \
    if (ossl_aes_##mode##_vaes_eligible())                                     \
        return &vaes_##mode;                                                   \
    return &aesni_##mode;                                                      \
}
