/*
 * Copyright 2001-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * AES-NI support for all hardware accelerated AES modes.
 */

#include "internal/deprecated.h"
#include "cipher_aes.h"
#include "cipher_aes_gcm.h"
#include "cipher_aes_ccm.h"
#include "cipher_aes_xts.h"

#if defined(AESNI_CAPABLE)

/* MODES: ecb, cbc, cfb, ofb, ctr */

/* generates AES round keys for AES-NI and VAES implementations */
static int cipher_hw_aesni_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    if ((ctx->mode == EVP_CIPH_ECB_MODE || ctx->mode == EVP_CIPH_CBC_MODE)
        && !ctx->enc) {
        return ossl_cipher_set_aes_initkey(ctx, key, keylen,
            aesni_set_decrypt_key, aesni_decrypt, NULL,
            (cbc128_f)aesni_cbc_encrypt, NULL);
    } else {
        return ossl_cipher_set_aes_initkey(ctx, key, keylen,
            aesni_set_encrypt_key, aesni_encrypt, NULL,
            (cbc128_f)aesni_cbc_encrypt, (ctr128_f)aesni_ctr32_encrypt_blocks);
    }
}

static int cipher_hw_aesni_ecb(PROV_CIPHER_CTX *ctx, unsigned char *out,
    const unsigned char *in, size_t len)
{
    if (len < ctx->blocksize)
        return 1;

    aesni_ecb_encrypt(in, out, len, ctx->ks, ctx->enc);

    return 1;
}

static const PROV_CIPHER_HW aesni_ecb = {
    cipher_hw_aesni_initkey,
    cipher_hw_aesni_ecb,
    ossl_cipher_aes_copyctx
};

static int cipher_hw_aesni_cbc(PROV_CIPHER_CTX *ctx, unsigned char *out,
    const unsigned char *in, size_t len)
{
    aesni_cbc_encrypt(in, out, len, ctx->ks, ctx->iv, ctx->enc);

    return 1;
}

static const PROV_CIPHER_HW aesni_cbc = {
    cipher_hw_aesni_initkey,
    cipher_hw_aesni_cbc,
    ossl_cipher_aes_copyctx
};

#if VAES_CBC_ELIGIBLE
/*
 * CBC decryption is fully parallel, so VAES accelerates it; CBC encryption is
 * inherently serial and stays on the aesni_cbc_encrypt assembly routine.  VAES
 * is only worthwhile once the payload is large enough to amortize the per-call
 * key broadcast, so small buffers also fall back to the assembly routine.
 */
static int aes_cbc_vaes_wrapper(
    PROV_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    size_t len)
{
    if (!ctx->enc && len >= 256) {
        ossl_aes_cbc_vaes_decrypt(in, out, len, ctx->ks, ctx->iv, ctx->enc);
        return 1;
    }

    aesni_cbc_encrypt(in, out, len, ctx->ks, ctx->iv, ctx->enc);
    return 1;
}

static const PROV_CIPHER_HW aesni_vaes_cbc = {
    cipher_hw_aesni_initkey,
    aes_cbc_vaes_wrapper,
    ossl_cipher_aes_copyctx
};
#endif /* VAES_CBC_ELIGIBLE */

#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
/* active in 64-bit builds when AES-NI, AVX512F, and VAES are detected */
#define VAES_CFB128_ELIGIBLE 1
#else
#define VAES_CFB128_ELIGIBLE 0
#endif

#if VAES_CFB128_ELIGIBLE
static int aes_cfb128_vaes_encdec_wrapper(
    PROV_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    size_t len)
{
    ossl_ssize_t num;

    num = (ossl_ssize_t)ctx->num;

    if (num < 0) {
        /* behavior from CRYPTO_cfb128_encrypt */
        ctx->num = -1;
        return 1;
    }

    if (ctx->enc)
        ossl_aes_cfb128_vaes_enc(in, out, len, ctx->ks, ctx->iv, &num);
    else
        ossl_aes_cfb128_vaes_dec(in, out, len, ctx->ks, ctx->iv, &num);

    ctx->num = (int)num;

    return 1;
}

static const PROV_CIPHER_HW aesni_vaes_cfb128 = {
    cipher_hw_aesni_initkey,
    aes_cfb128_vaes_encdec_wrapper,
    ossl_cipher_aes_copyctx
};
#endif /* VAES_CFB128_ELIGIBLE */

static const PROV_CIPHER_HW aesni_cfb128 = {
    cipher_hw_aesni_initkey,
    ossl_cipher_hw_generic_cfb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aesni_cfb8 = {
    cipher_hw_aesni_initkey,
    ossl_cipher_hw_generic_cfb8,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aesni_cfb1 = {
    cipher_hw_aesni_initkey,
    ossl_cipher_hw_generic_cfb1,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aesni_ofb128 = {
    cipher_hw_aesni_initkey,
    ossl_cipher_hw_generic_ofb128,
    ossl_cipher_aes_copyctx
};

static const PROV_CIPHER_HW aesni_ctr = {
    cipher_hw_aesni_initkey,
    ossl_cipher_hw_generic_ctr,
    ossl_cipher_aes_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aesni(enum aes_modes mode)
{
    if (AESNI_CAPABLE) {
        switch (mode) {
        case AES_MODE_ECB:
            return &aesni_ecb;
        case AES_MODE_CBC:
#if VAES_CBC_ELIGIBLE
            if (ossl_aes_cbc_vaes_eligible())
                return &aesni_vaes_cbc;
#endif
            return &aesni_cbc;
        case AES_MODE_CFB128:
#if VAES_CFB128_ELIGIBLE
            if (ossl_aes_cfb128_vaes_eligible())
                return &aesni_vaes_cfb128;
#endif
            return &aesni_cfb128;
        case AES_MODE_CFB8:
            return &aesni_cfb8;
        case AES_MODE_CFB1:
            return &aesni_cfb1;
        case AES_MODE_OFB128:
            return &aesni_ofb128;
        case AES_MODE_CTR:
            return &aesni_ctr;
        default:
            return NULL;
        }
    }
    return NULL;
}

/* MODES: GCM */

static int aesni_gcm_initkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    return ossl_aes_gcm_hw_initkey(ctx, key, keylen, aesni_set_encrypt_key,
        aesni_encrypt, aesni_ctr32_encrypt_blocks);
}

static const PROV_GCM_HW aesni_gcm = {
    aesni_gcm_initkey,
    ossl_gcm_setiv,
    ossl_gcm_aad_update,
    ossl_generic_aes_gcm_cipher_update,
    ossl_gcm_cipher_final,
    ossl_gcm_one_shot
};

/*-
 * AVX512 VAES + VPCLMULDQD support for AES GCM.
 */

#undef VAES_GCM_ENABLED
#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
#define VAES_GCM_ENABLED

/* Returns non-zero when AVX512F + VAES + VPCLMULDQD combination is available */
int ossl_vaes_vpclmulqdq_capable(void);

void ossl_aes_gcm_encrypt_avx512(const void *ks, void *gcm128ctx,
    unsigned int *pblocklen, const unsigned char *in, size_t len,
    unsigned char *out);
void ossl_aes_gcm_decrypt_avx512(const void *ks, void *gcm128ctx,
    unsigned int *pblocklen, const unsigned char *in, size_t len,
    unsigned char *out);

void ossl_aes_gcm_init_avx512(const void *ks, void *gcm128ctx);
void ossl_aes_gcm_setiv_avx512(const void *ks, void *gcm128ctx,
    const unsigned char *iv, size_t ivlen);
void ossl_aes_gcm_update_aad_avx512(void *gcm128ctx, const unsigned char *aad,
    size_t aadlen);
void ossl_aes_gcm_finalize_avx512(void *gcm128ctx, unsigned int pblocklen);

void ossl_gcm_gmult_avx512(uint64_t Xi[2], const void *gcm128ctx);

static int vaes_gcm_setkey(PROV_GCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    GCM128_CONTEXT *gcmctx = &ctx->gcm;
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    AES_KEY *ks = &actx->ks.ks;

    aesni_set_encrypt_key(key, (int)(keylen * 8), ks);
    memset(gcmctx, 0, sizeof(*gcmctx));
    gcmctx->key = ks;
    ctx->key_set = 1;

    ossl_aes_gcm_init_avx512(ks, gcmctx);

    return 1;
}

static int vaes_gcm_setiv(PROV_GCM_CTX *ctx, const unsigned char *iv,
    size_t ivlen)
{
    GCM128_CONTEXT *gcmctx = &ctx->gcm;

    gcmctx->Yi.u[0] = 0; /* Current counter */
    gcmctx->Yi.u[1] = 0;
    gcmctx->Xi.u[0] = 0; /* AAD hash */
    gcmctx->Xi.u[1] = 0;
    gcmctx->len.u[0] = 0; /* AAD length */
    gcmctx->len.u[1] = 0; /* Message length */
    gcmctx->ares = 0;
    gcmctx->mres = 0;

    /* IV is limited by 2^64 bits, thus 2^61 bytes */
    if (ivlen > (U64(1) << 61))
        return 0;

    ossl_aes_gcm_setiv_avx512(gcmctx->key, gcmctx, iv, ivlen);

    return 1;
}

static int vaes_gcm_aadupdate(PROV_GCM_CTX *ctx,
    const unsigned char *aad,
    size_t aad_len)
{
    GCM128_CONTEXT *gcmctx = &ctx->gcm;
    uint64_t alen = gcmctx->len.u[0];
    unsigned int ares;
    size_t i, lenBlks;

    /* Bad sequence: call of AAD update after message processing */
    if (gcmctx->len.u[1] > 0)
        return 0;

    alen += aad_len;
    /* AAD is limited by 2^64 bits, thus 2^61 bytes */
    if ((alen > (U64(1) << 61)) || (alen < aad_len))
        return 0;

    gcmctx->len.u[0] = alen;

    ares = gcmctx->ares;
    /* Partial AAD block left from previous AAD update calls */
    if (ares > 0) {
        /*
         * Fill partial block buffer till full block
         * (note, the hash is stored reflected)
         */
        while (ares > 0 && aad_len > 0) {
            gcmctx->Xi.c[15 - ares] ^= *(aad++);
            --aad_len;
            ares = (ares + 1) % AES_BLOCK_SIZE;
        }
        /* Full block gathered */
        if (ares == 0) {
            ossl_gcm_gmult_avx512(gcmctx->Xi.u, gcmctx);
        } else { /* no more AAD */
            gcmctx->ares = ares;
            return 1;
        }
    }

    /* Bulk AAD processing */
    lenBlks = aad_len & ((size_t)(-AES_BLOCK_SIZE));
    if (lenBlks > 0) {
        ossl_aes_gcm_update_aad_avx512(gcmctx, aad, lenBlks);
        aad += lenBlks;
        aad_len -= lenBlks;
    }

    /* Add remaining AAD to the hash (note, the hash is stored reflected) */
    if (aad_len > 0) {
        ares = (unsigned int)aad_len;
        for (i = 0; i < aad_len; i++)
            gcmctx->Xi.c[15 - i] ^= aad[i];
    }

    gcmctx->ares = ares;

    return 1;
}

static int vaes_gcm_cipherupdate(PROV_GCM_CTX *ctx, const unsigned char *in,
    size_t len, unsigned char *out)
{
    GCM128_CONTEXT *gcmctx = &ctx->gcm;
    uint64_t mlen = gcmctx->len.u[1];

    mlen += len;
    if (mlen > ((U64(1) << 36) - 32) || (mlen < len))
        return 0;

    gcmctx->len.u[1] = mlen;

    /* Finalize GHASH(AAD) if AAD partial blocks left unprocessed */
    if (gcmctx->ares > 0) {
        ossl_gcm_gmult_avx512(gcmctx->Xi.u, gcmctx);
        gcmctx->ares = 0;
    }

    if (ctx->enc)
        ossl_aes_gcm_encrypt_avx512(gcmctx->key, gcmctx, &gcmctx->mres, in, len, out);
    else
        ossl_aes_gcm_decrypt_avx512(gcmctx->key, gcmctx, &gcmctx->mres, in, len, out);

    return 1;
}

static int vaes_gcm_cipherfinal(PROV_GCM_CTX *ctx, unsigned char *tag)
{
    GCM128_CONTEXT *gcmctx = &ctx->gcm;
    unsigned int *res = &gcmctx->mres;

    /* Finalize AAD processing */
    if (gcmctx->ares > 0)
        res = &gcmctx->ares;

    ossl_aes_gcm_finalize_avx512(gcmctx, *res);

    if (ctx->enc) {
        ctx->taglen = GCM_TAG_MAX_SIZE;
        memcpy(tag, gcmctx->Xi.c,
            ctx->taglen <= sizeof(gcmctx->Xi.c) ? ctx->taglen : sizeof(gcmctx->Xi.c));
        *res = 0;
    } else {
        return !CRYPTO_memcmp(gcmctx->Xi.c, tag, ctx->taglen);
    }

    return 1;
}

static const PROV_GCM_HW vaes_gcm = {
    vaes_gcm_setkey,
    vaes_gcm_setiv,
    vaes_gcm_aadupdate,
    vaes_gcm_cipherupdate,
    vaes_gcm_cipherfinal,
    ossl_gcm_one_shot
};

#endif

const PROV_GCM_HW *ossl_prov_aes_hw_gcm_aesni(void)
{
#ifdef VAES_GCM_ENABLED
    if (ossl_vaes_vpclmulqdq_capable())
        return &vaes_gcm;
#endif
    if (AESNI_CAPABLE)
        return &aesni_gcm;

    return NULL;
}

/* MODES: CCM */

static int ccm_aesni_initkey(PROV_CCM_CTX *ctx, const unsigned char *key,
    size_t keylen)
{
    return ossl_cipher_set_ccm_aes_initkey(ctx, key, keylen,
        aesni_set_encrypt_key, aesni_encrypt, aesni_ccm64_encrypt_blocks,
        aesni_ccm64_decrypt_blocks);
}

static const PROV_CCM_HW aesni_ccm = {
    ccm_aesni_initkey,
    ossl_ccm_generic_setiv,
    ossl_ccm_generic_setaad,
    ossl_ccm_generic_auth_encrypt,
    ossl_ccm_generic_auth_decrypt,
    ossl_ccm_generic_gettag
};

const PROV_CCM_HW *ossl_prov_aes_hw_ccm_aesni(void)
{
    if (AESNI_CAPABLE)
        return &aesni_ccm;
    return NULL;
}

/* MODES: XTS */

static int cipher_hw_aesni_xts_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    void (*aesni_xts_enc)(const unsigned char *in,
        unsigned char *out,
        size_t length,
        const AES_KEY *key1, const AES_KEY *key2,
        const unsigned char iv[16]);
    void (*aesni_xts_dec)(const unsigned char *in,
        unsigned char *out,
        size_t length,
        const AES_KEY *key1, const AES_KEY *key2,
        const unsigned char iv[16]);

    aesni_xts_enc = aesni_xts_encrypt;
    aesni_xts_dec = aesni_xts_decrypt;

#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
    if (aesni_xts_avx512_eligible()) {
        if (keylen == 64) {
            aesni_xts_enc = aesni_xts_256_encrypt_avx512;
            aesni_xts_dec = aesni_xts_256_decrypt_avx512;
        } else if (keylen == 32) {
            aesni_xts_enc = aesni_xts_128_encrypt_avx512;
            aesni_xts_dec = aesni_xts_128_decrypt_avx512;
        }
    }
#endif

    return ossl_cipher_set_aes_xts_initkey(ctx, key, keylen,
        aesni_set_encrypt_key, aesni_set_decrypt_key,
        aesni_encrypt, aesni_decrypt, aesni_xts_enc, aesni_xts_dec);
}

static const PROV_CIPHER_HW aesni_xts = {
    cipher_hw_aesni_xts_initkey,
    NULL,
    ossl_cipher_hw_aes_xts_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_aesni(void)
{
    if (AESNI_CAPABLE)
        return &aesni_xts;
    return NULL;
}

#endif
