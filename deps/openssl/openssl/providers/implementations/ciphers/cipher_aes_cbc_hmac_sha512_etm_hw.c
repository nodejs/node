/*
 * Copyright 2023-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * All low level APIs are deprecated for public use, but still ok for internal
 * use where we're using them to implement the higher level EVP interface, as is
 * the case here.
 */
#include "internal/deprecated.h"
#include "cipher_aes_cbc_hmac_sha_etm.h"

#if !defined(AES_CBC_HMAC_SHA_ETM_CAPABLE)
int ossl_cipher_capable_aes_cbc_hmac_sha512_etm(void)
{
    return 0;
}

const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *ossl_prov_cipher_hw_aes_cbc_hmac_sha512_etm(void)
{
    return NULL;
}
#else
# if defined(__aarch64__) || defined(_M_ARM64)
void asm_aescbc_sha512_hmac(const uint8_t *csrc, uint8_t *cdst, uint64_t clen,
                            uint8_t *dsrc, uint8_t *ddst, uint64_t dlen,
                            CIPH_DIGEST *arg);
void asm_sha512_hmac_aescbc_dec(const uint8_t *csrc, uint8_t *cdst, uint64_t clen,
                                uint8_t *dsrc, uint8_t *ddst, uint64_t dlen,
                                CIPH_DIGEST *arg);
#  define HWAES_ENC_CBC_SHA512_ETM asm_aescbc_sha512_hmac
#  define HWAES_DEC_CBC_SHA512_ETM asm_sha512_hmac_aescbc_dec
# endif

int ossl_cipher_capable_aes_cbc_hmac_sha512_etm(void)
{
    return HWAES_CBC_HMAC_SHA512_ETM_CAPABLE;
}

static int hwaes_cbc_hmac_sha512_init_key(PROV_CIPHER_CTX *vctx,
                                          const unsigned char *key,
                                          size_t keylen)
{
    int ret;
    PROV_AES_HMAC_SHA_ETM_CTX *ctx = (PROV_AES_HMAC_SHA_ETM_CTX *)vctx;
    PROV_AES_HMAC_SHA512_ETM_CTX *sctx = (PROV_AES_HMAC_SHA512_ETM_CTX *)vctx;

    if (ctx->base.enc)
        ret = aes_v8_set_encrypt_key(key, ctx->base.keylen * 8, &ctx->ks);
    else
        ret = aes_v8_set_decrypt_key(key, ctx->base.keylen * 8, &ctx->ks);

    SHA512_Init(&sctx->head);    /* handy when benchmarking */
    sctx->tail = sctx->head;

    return ret < 0 ? 0 : 1;
}

void sha512_block_data_order(void *c, const void *p, size_t len);

static void sha512_update(SHA512_CTX *c, const void *data, size_t len)
{
    const unsigned char *ptr = data;
    size_t res;

    if ((res = c->num)) {
        res = SHA512_CBLOCK - res;
        if (len < res)
            res = len;
        SHA512_Update(c, ptr, res);
        ptr += res;
        len -= res;
    }

    res = len % SHA512_CBLOCK;
    len -= res;

    if (len) {
        sha512_block_data_order(c, ptr, len / SHA512_CBLOCK);

        ptr += len;
        c->Nh += len >> 61;
        c->Nl += len <<= 3;
        if (c->Nl < (unsigned int)len)
            c->Nh++;
    }

    if (res)
        SHA512_Update(c, ptr, res);
}

static void ciph_digest_arg_init(CIPH_DIGEST *arg, PROV_CIPHER_CTX *vctx)
{
    PROV_AES_HMAC_SHA_ETM_CTX *ctx = (PROV_AES_HMAC_SHA_ETM_CTX *)vctx;
    PROV_AES_HMAC_SHA512_ETM_CTX *sctx = (PROV_AES_HMAC_SHA512_ETM_CTX *)vctx;

    arg->cipher.key = (uint8_t *)&(ctx->ks);
    arg->cipher.key_rounds = ctx->ks.rounds;
    arg->cipher.iv = (uint8_t *)&(ctx->base.iv);
    arg->digest.hmac.i_key_pad = (uint8_t *)&(sctx->head);
    arg->digest.hmac.o_key_pad = (uint8_t *)&(sctx->tail);
}

static int hwaes_cbc_hmac_sha512_etm(PROV_CIPHER_CTX *vctx,
                                     unsigned char *out,
                                     const unsigned char *in, size_t len)
{
    PROV_AES_HMAC_SHA_ETM_CTX *ctx = (PROV_AES_HMAC_SHA_ETM_CTX *)vctx;
    CIPH_DIGEST arg = {0};

    ciph_digest_arg_init(&arg, vctx);

    if (len % AES_BLOCK_SIZE) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_INPUT_LENGTH);
        return 0;
    }

    if (ctx->base.enc) {
        HWAES_ENC_CBC_SHA512_ETM(in, out, len, out, ctx->tag, len, &arg);
        return 1;
    } else {
        if (ctx->taglen == 0) {
            ERR_raise(ERR_LIB_PROV, PROV_R_TAG_NOT_SET);
            return 0;
        }
        HWAES_DEC_CBC_SHA512_ETM(in, out, len, out, ctx->tag, len, &arg);
        if (CRYPTO_memcmp(ctx->exp_tag, ctx->tag, ctx->taglen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_TAG);
            return 0;
        }
        return 1;
    }
}

static int hwaes_cbc_hmac_sha512_cipher(PROV_CIPHER_CTX *vctx,
                                        unsigned char *out,
                                        const unsigned char *in, size_t len)
{
    return hwaes_cbc_hmac_sha512_etm(vctx, out, in, len);
}

static void hwaes_cbc_hmac_sha512_set_mac_key(void *vctx,
                                              const unsigned char *mackey,
                                              size_t len)
{
    PROV_AES_HMAC_SHA512_ETM_CTX *ctx = (PROV_AES_HMAC_SHA512_ETM_CTX *)vctx;
    unsigned int i;
    unsigned char hmac_key[128];

    memset(hmac_key, 0, sizeof(hmac_key));

    if (len > sizeof(hmac_key)) {
        SHA512_Init(&ctx->head);
        sha512_update(&ctx->head, mackey, len);
        SHA512_Final(hmac_key, &ctx->head);
    } else {
        memcpy(hmac_key, mackey, len);
    }

    for (i = 0; i < sizeof(hmac_key); i++)
        hmac_key[i] ^= 0x36; /* ipad */
    SHA512_Init(&ctx->head);
    sha512_update(&ctx->head, hmac_key, sizeof(hmac_key));

    for (i = 0; i < sizeof(hmac_key); i++)
        hmac_key[i] ^= 0x36 ^ 0x5c; /* opad */
    SHA512_Init(&ctx->tail);
    sha512_update(&ctx->tail, hmac_key, sizeof(hmac_key));

    OPENSSL_cleanse(hmac_key, sizeof(hmac_key));
}

static const PROV_CIPHER_HW_AES_HMAC_SHA_ETM cipher_hw_aes_hmac_sha512_etm = {
    {
     hwaes_cbc_hmac_sha512_init_key,
     hwaes_cbc_hmac_sha512_cipher
    },
    hwaes_cbc_hmac_sha512_set_mac_key
};

const PROV_CIPHER_HW_AES_HMAC_SHA_ETM *ossl_prov_cipher_hw_aes_cbc_hmac_sha512_etm(void)
{
    return &cipher_hw_aes_hmac_sha512_etm;
}

#endif /* !defined(AES_CBC_HMAC_SHA_CAPABLE) */
