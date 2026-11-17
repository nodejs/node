/*
 * Copyright 2001-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * IBM S390X support for all hardware accelerated AES modes.
 */

#include "internal/deprecated.h"
#include <openssl/proverr.h>
#include "cipher_aes.h"
#include "cipher_aes_gcm.h"
#include "cipher_aes_ccm.h"
#include "cipher_aes_xts.h"
#include <stdio.h>

#if defined(S390X_aes_128_CAPABLE)

/* MODES: ecb, cfb, ofb */

static int s390x_aes_ecb_initkey(PROV_CIPHER_CTX *dat,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    memcpy(adat->plat.s390x.param.km.k, key, keylen);
    return 1;
}

static int s390x_aes_ecb_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
    const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    unsigned int modifier = adat->base.enc ? 0 : S390X_DECRYPT;

    s390x_km(in, len, out, adat->plat.s390x.fc | modifier,
        &adat->plat.s390x.param.km);
    return 1;
}

static const PROV_CIPHER_HW s390x_aes_ecb = {
    s390x_aes_ecb_initkey,
    s390x_aes_ecb_cipher_hw,
    ossl_cipher_aes_copyctx
};

static int s390x_aes_ofb128_initkey(PROV_CIPHER_CTX *dat,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    return 1;
}

static int s390x_aes_ofb128_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
    const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    int n = dat->num;
    int rem;

    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->ivlen);
    while (n && len) {
        *out = *in ^ adat->plat.s390x.param.kmo_kmf.cv[n];
        n = (n + 1) & 0xf;
        --len;
        ++in;
        ++out;
    }

    rem = len & 0xf;

    len &= ~(size_t)0xf;
    if (len) {
        s390x_kmo(in, len, out, adat->plat.s390x.fc,
            &adat->plat.s390x.param.kmo_kmf);

        out += len;
        in += len;
    }

    if (rem) {
        s390x_km(adat->plat.s390x.param.kmo_kmf.cv, 16,
            adat->plat.s390x.param.kmo_kmf.cv,
            adat->plat.s390x.fc,
            adat->plat.s390x.param.kmo_kmf.k);

        while (rem--) {
            out[n] = in[n] ^ adat->plat.s390x.param.kmo_kmf.cv[n];
            ++n;
        }
    }

    memcpy(dat->iv, adat->plat.s390x.param.kmo_kmf.cv, dat->ivlen);
    dat->num = n;
    return 1;
}

static const PROV_CIPHER_HW s390x_aes_ofb128 = {
    s390x_aes_ofb128_initkey,
    s390x_aes_ofb128_cipher_hw,
    ossl_cipher_aes_copyctx
};

static int s390x_aes_cfb128_initkey(PROV_CIPHER_CTX *dat,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    adat->plat.s390x.fc |= 16 << 24; /* 16 bytes cipher feedback */
    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    return 1;
}

static int s390x_aes_cfb128_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
    const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    unsigned int modifier = adat->base.enc ? 0 : S390X_DECRYPT;
    int n = dat->num;
    int rem;
    unsigned char tmp;

    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->ivlen);
    while (n && len) {
        tmp = *in;
        *out = adat->plat.s390x.param.kmo_kmf.cv[n] ^ tmp;
        adat->plat.s390x.param.kmo_kmf.cv[n] = dat->enc ? *out : tmp;
        n = (n + 1) & 0xf;
        --len;
        ++in;
        ++out;
    }

    rem = len & 0xf;

    len &= ~(size_t)0xf;
    if (len) {
        s390x_kmf(in, len, out, adat->plat.s390x.fc | modifier,
            &adat->plat.s390x.param.kmo_kmf);

        out += len;
        in += len;
    }

    if (rem) {
        s390x_km(adat->plat.s390x.param.kmo_kmf.cv, 16,
            adat->plat.s390x.param.kmo_kmf.cv,
            S390X_AES_FC(dat->keylen),
            adat->plat.s390x.param.kmo_kmf.k);

        while (rem--) {
            tmp = in[n];
            out[n] = adat->plat.s390x.param.kmo_kmf.cv[n] ^ tmp;
            adat->plat.s390x.param.kmo_kmf.cv[n] = dat->enc ? out[n] : tmp;
            ++n;
        }
    }

    memcpy(dat->iv, adat->plat.s390x.param.kmo_kmf.cv, dat->ivlen);
    dat->num = n;
    return 1;
}

static const PROV_CIPHER_HW s390x_aes_cfb128 = {
    s390x_aes_cfb128_initkey,
    s390x_aes_cfb128_cipher_hw,
    ossl_cipher_aes_copyctx
};

static int s390x_aes_cfb8_initkey(PROV_CIPHER_CTX *dat,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;

    adat->plat.s390x.fc = S390X_AES_FC(keylen);
    adat->plat.s390x.fc |= 1 << 24; /* 1 byte cipher feedback */
    memcpy(adat->plat.s390x.param.kmo_kmf.k, key, keylen);
    return 1;
}

static int s390x_aes_cfb8_cipher_hw(PROV_CIPHER_CTX *dat, unsigned char *out,
    const unsigned char *in, size_t len)
{
    PROV_AES_CTX *adat = (PROV_AES_CTX *)dat;
    unsigned int modifier = adat->base.enc ? 0 : S390X_DECRYPT;

    memcpy(adat->plat.s390x.param.kmo_kmf.cv, dat->iv, dat->ivlen);
    s390x_kmf(in, len, out, adat->plat.s390x.fc | modifier,
        &adat->plat.s390x.param.kmo_kmf);
    memcpy(dat->iv, adat->plat.s390x.param.kmo_kmf.cv, dat->ivlen);
    return 1;
}

static const PROV_CIPHER_HW s390x_aes_cfb8 = {
    s390x_aes_cfb8_initkey,
    s390x_aes_cfb8_cipher_hw,
    ossl_cipher_aes_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_s390x(enum aes_modes mode,
    size_t keybits)
{
    switch (mode) {
    case AES_MODE_ECB:
        if ((keybits == 128 && S390X_aes_128_ecb_CAPABLE)
            || (keybits == 192 && S390X_aes_192_ecb_CAPABLE)
            || (keybits == 256 && S390X_aes_256_ecb_CAPABLE))
            return &s390x_aes_ecb;
        break;
    case AES_MODE_CFB128:
        if ((keybits == 128 && S390X_aes_128_cfb_CAPABLE)
            || (keybits == 192 && S390X_aes_192_cfb_CAPABLE)
            || (keybits == 256 && S390X_aes_256_cfb_CAPABLE))
            return &s390x_aes_cfb128;
        break;
    case AES_MODE_CFB8:
        if ((keybits == 128 && S390X_aes_128_cfb8_CAPABLE)
            || (keybits == 192 && S390X_aes_192_cfb8_CAPABLE)
            || (keybits == 256 && S390X_aes_256_cfb8_CAPABLE))
            return &s390x_aes_cfb8;
        break;
    case AES_MODE_OFB128:
        if ((keybits == 128 && S390X_aes_128_ofb_CAPABLE)
            || (keybits == 192 && S390X_aes_192_ofb_CAPABLE)
            || (keybits == 256 && S390X_aes_256_ofb_CAPABLE))
            return &s390x_aes_ofb128;
        break;
    default:
        break;
    }
    return NULL;
}

/* MODES: GCM */

/* iv + padding length for iv lengths != 12 */
#define S390X_gcm_ivpadlen(i) ((((i) + 15) >> 4 << 4) + 16)

/* Additional flag or'ed to fc for decryption */
#define S390X_gcm_decrypt_flag(ctx) (((ctx)->enc) ? 0 : S390X_DECRYPT)

#define S390X_gcm_fc(A, C) ((A)->plat.s390x.fc | (A)->plat.s390x.hsflag | S390X_gcm_decrypt_flag((C)))

static int s390x_aes_gcm_initkey(PROV_GCM_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;

    ctx->key_set = 1;
    memcpy(&actx->plat.s390x.param.kma.k, key, keylen);
    actx->plat.s390x.fc = S390X_AES_FC(keylen);
    return 1;
}

static int s390x_aes_gcm_setiv(PROV_GCM_CTX *ctx, const unsigned char *iv,
    size_t ivlen)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    S390X_KMA_PARAMS *kma = &actx->plat.s390x.param.kma;

    kma->t.g[0] = 0;
    kma->t.g[1] = 0;
    kma->tpcl = 0;
    kma->taadl = 0;
    actx->plat.s390x.mreslen = 0;
    actx->plat.s390x.areslen = 0;
    actx->plat.s390x.kreslen = 0;

    if (ivlen == GCM_IV_DEFAULT_SIZE) {
        memcpy(&kma->j0, iv, ivlen);
        kma->j0.w[3] = 1;
        kma->cv.w = 1;
        actx->plat.s390x.hsflag = 0;
    } else {
        unsigned long long ivbits = ivlen << 3;
        size_t len = S390X_gcm_ivpadlen(ivlen);
        unsigned char iv_zero_pad[S390X_gcm_ivpadlen(GCM_IV_MAX_SIZE)];
        /*
         * The IV length needs to be zero padded to be a multiple of 16 bytes
         * followed by 8 bytes of zeros and 8 bytes for the IV length.
         * The GHASH of this value can then be calculated.
         */
        memcpy(iv_zero_pad, iv, ivlen);
        memset(iv_zero_pad + ivlen, 0, len - ivlen);
        memcpy(iv_zero_pad + len - sizeof(ivbits), &ivbits, sizeof(ivbits));
        /*
         * Calculate the ghash of the iv - the result is stored into the tag
         * param.
         */
        s390x_kma(iv_zero_pad, len, NULL, 0, NULL, actx->plat.s390x.fc, kma);
        actx->plat.s390x.hsflag = S390X_KMA_HS; /* The hash subkey is set */

        /* Copy the 128 bit GHASH result into J0 and clear the tag */
        kma->j0.g[0] = kma->t.g[0];
        kma->j0.g[1] = kma->t.g[1];
        kma->t.g[0] = 0;
        kma->t.g[1] = 0;
        /* Set the 32 bit counter */
        kma->cv.w = kma->j0.w[3];
    }
    return 1;
}

static int s390x_aes_gcm_cipher_final(PROV_GCM_CTX *ctx, unsigned char *tag)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    S390X_KMA_PARAMS *kma = &actx->plat.s390x.param.kma;
    unsigned char out[AES_BLOCK_SIZE];
    unsigned int fc;
    int rc;

    kma->taadl <<= 3;
    kma->tpcl <<= 3;
    fc = S390X_gcm_fc(actx, ctx) | S390X_KMA_LAAD | S390X_KMA_LPC;
    s390x_kma(actx->plat.s390x.ares, actx->plat.s390x.areslen,
        actx->plat.s390x.mres, actx->plat.s390x.mreslen, out,
        fc, kma);

    /* gctx->mres already returned to the caller */
    OPENSSL_cleanse(out, actx->plat.s390x.mreslen);

    if (ctx->enc) {
        ctx->taglen = GCM_TAG_MAX_SIZE;
        memcpy(tag, kma->t.b, ctx->taglen);
        rc = 1;
    } else {
        rc = (CRYPTO_memcmp(tag, kma->t.b, ctx->taglen) == 0);
    }
    return rc;
}

static int s390x_aes_gcm_one_shot(PROV_GCM_CTX *ctx,
    unsigned char *aad, size_t aad_len,
    const unsigned char *in, size_t in_len,
    unsigned char *out,
    unsigned char *tag, size_t taglen)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    S390X_KMA_PARAMS *kma = &actx->plat.s390x.param.kma;
    unsigned int fc;
    int rc;

    kma->taadl = aad_len << 3;
    kma->tpcl = in_len << 3;
    fc = S390X_gcm_fc(actx, ctx) | S390X_KMA_LAAD | S390X_KMA_LPC;
    s390x_kma(aad, aad_len, in, in_len, out, fc, kma);

    if (ctx->enc) {
        memcpy(tag, kma->t.b, taglen);
        rc = 1;
    } else {
        rc = (CRYPTO_memcmp(tag, kma->t.b, taglen) == 0);
    }
    return rc;
}

/*
 * Process additional authenticated data. Returns 1 on success. Code is
 * big-endian.
 */
static int s390x_aes_gcm_aad_update(PROV_GCM_CTX *ctx,
    const unsigned char *aad, size_t len)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    S390X_KMA_PARAMS *kma = &actx->plat.s390x.param.kma;
    unsigned long long alen;
    unsigned int fc;
    int n, rem;

    /* If already processed pt/ct then error */
    if (kma->tpcl != 0)
        return 0;

    /* update the total aad length */
    alen = kma->taadl + len;
    if (alen > (U64(1) << 61) || (sizeof(len) == 8 && alen < len))
        return 0;
    kma->taadl = alen;

    /* check if there is any existing aad data from a previous add */
    n = actx->plat.s390x.areslen;
    if (n) {
        /* add additional data to a buffer until it has 16 bytes */
        while (n && len) {
            actx->plat.s390x.ares[n] = *aad;
            ++aad;
            --len;
            n = (n + 1) & 0xf;
        }
        /* ctx->ares contains a complete block if offset has wrapped around */
        if (!n) {
            fc = S390X_gcm_fc(actx, ctx);
            s390x_kma(actx->plat.s390x.ares, 16, NULL, 0, NULL, fc, kma);
            actx->plat.s390x.hsflag = S390X_KMA_HS;
        }
        actx->plat.s390x.areslen = n;
    }

    /* If there are leftover bytes (< 128 bits) save them for next time */
    rem = len & 0xf;
    /* Add any remaining 16 byte blocks (128 bit each) */
    len &= ~(size_t)0xf;
    if (len) {
        fc = S390X_gcm_fc(actx, ctx);
        s390x_kma(aad, len, NULL, 0, NULL, fc, kma);
        actx->plat.s390x.hsflag = S390X_KMA_HS;
        aad += len;
    }

    if (rem) {
        actx->plat.s390x.areslen = rem;

        do {
            --rem;
            actx->plat.s390x.ares[rem] = aad[rem];
        } while (rem);
    }
    return 1;
}

/*-
 * En/de-crypt plain/cipher-text and authenticate ciphertext. Returns 1 for
 * success. Code is big-endian.
 */
static int s390x_aes_gcm_cipher_update(PROV_GCM_CTX *ctx,
    const unsigned char *in, size_t len,
    unsigned char *out)
{
    PROV_AES_GCM_CTX *actx = (PROV_AES_GCM_CTX *)ctx;
    S390X_KMA_PARAMS *kma = &actx->plat.s390x.param.kma;
    const unsigned char *inptr;
    unsigned long long mlen;
    unsigned int fc;
    union {
        unsigned int w[4];
        unsigned char b[16];
    } buf;
    size_t inlen;
    int n, rem, i;

    mlen = kma->tpcl + len;
    if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
        return 0;
    kma->tpcl = mlen;

    fc = S390X_gcm_fc(actx, ctx) | S390X_KMA_LAAD;
    n = actx->plat.s390x.mreslen;
    if (n) {
        inptr = in;
        inlen = len;
        while (n && inlen) {
            actx->plat.s390x.mres[n] = *inptr;
            n = (n + 1) & 0xf;
            ++inptr;
            --inlen;
        }
        /* ctx->mres contains a complete block if offset has wrapped around */
        if (!n) {
            s390x_kma(actx->plat.s390x.ares, actx->plat.s390x.areslen,
                actx->plat.s390x.mres, 16, buf.b, fc, kma);
            actx->plat.s390x.hsflag = S390X_KMA_HS;
            fc |= S390X_KMA_HS;
            actx->plat.s390x.areslen = 0;

            /* previous call already encrypted/decrypted its remainder,
             * see comment below */
            n = actx->plat.s390x.mreslen;
            while (n) {
                *out = buf.b[n];
                n = (n + 1) & 0xf;
                ++out;
                ++in;
                --len;
            }
            actx->plat.s390x.mreslen = 0;
        }
    }

    rem = len & 0xf;

    len &= ~(size_t)0xf;
    if (len) {
        s390x_kma(actx->plat.s390x.ares, actx->plat.s390x.areslen, in, len, out,
            fc, kma);
        in += len;
        out += len;
        actx->plat.s390x.hsflag = S390X_KMA_HS;
        actx->plat.s390x.areslen = 0;
    }

    /*-
     * If there is a remainder, it has to be saved such that it can be
     * processed by kma later. However, we also have to do the for-now
     * unauthenticated encryption/decryption part here and now...
     */
    if (rem) {
        if (!actx->plat.s390x.mreslen) {
            buf.w[0] = kma->j0.w[0];
            buf.w[1] = kma->j0.w[1];
            buf.w[2] = kma->j0.w[2];
            buf.w[3] = kma->cv.w + 1;
            s390x_km(buf.b, 16, actx->plat.s390x.kres,
                fc & 0x1f, &kma->k);
        }

        n = actx->plat.s390x.mreslen;
        for (i = 0; i < rem; i++) {
            actx->plat.s390x.mres[n + i] = in[i];
            out[i] = in[i] ^ actx->plat.s390x.kres[n + i];
        }
        actx->plat.s390x.mreslen += rem;
    }
    return 1;
}

static const PROV_GCM_HW s390x_aes_gcm = {
    s390x_aes_gcm_initkey,
    s390x_aes_gcm_setiv,
    s390x_aes_gcm_aad_update,
    s390x_aes_gcm_cipher_update,
    s390x_aes_gcm_cipher_final,
    s390x_aes_gcm_one_shot
};

const PROV_GCM_HW *ossl_prov_aes_hw_gcm_s390x(size_t keybits)
{
    if ((keybits == 128 && S390X_aes_128_gcm_CAPABLE)
        || (keybits == 192 && S390X_aes_192_gcm_CAPABLE)
        || (keybits == 256 && S390X_aes_256_gcm_CAPABLE))
        return &s390x_aes_gcm;
    return NULL;
}

/* MODES: CCM */

#define S390X_CCM_AAD_FLAG 0x40

static int s390x_aes_ccm_initkey(PROV_CCM_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;

    sctx->ccm.s390x.fc = S390X_AES_FC(keylen);
    memcpy(&sctx->ccm.s390x.kmac.k, key, keylen);
    /* Store encoded m and l. */
    sctx->ccm.s390x.nonce.b[0] = ((ctx->l - 1) & 0x7)
        | (((ctx->m - 2) >> 1) & 0x7) << 3;
    memset(sctx->ccm.s390x.nonce.b + 1, 0, sizeof(sctx->ccm.s390x.nonce.b));
    sctx->ccm.s390x.blocks = 0;
    ctx->key_set = 1;
    return 1;
}

static int s390x_aes_ccm_setiv(PROV_CCM_CTX *ctx,
    const unsigned char *nonce, size_t noncelen,
    size_t mlen)
{
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;

    sctx->ccm.s390x.nonce.b[0] &= ~S390X_CCM_AAD_FLAG;
    sctx->ccm.s390x.nonce.g[1] = mlen;
    memcpy(sctx->ccm.s390x.nonce.b + 1, nonce, 15 - ctx->l);
    return 1;
}

/*-
 * Process additional authenticated data. Code is big-endian.
 */
static int s390x_aes_ccm_setaad(PROV_CCM_CTX *ctx,
    const unsigned char *aad, size_t alen)
{
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;
    unsigned char *ptr;
    int i, rem;

    if (!alen)
        return 1;

    sctx->ccm.s390x.nonce.b[0] |= S390X_CCM_AAD_FLAG;

    /* Suppress 'type-punned pointer dereference' warning. */
    ptr = sctx->ccm.s390x.buf.b;

    if (alen < ((1 << 16) - (1 << 8))) {
        *(uint16_t *)ptr = alen;
        i = 2;
    } else if (sizeof(alen) == 8
        && alen >= (size_t)1 << (32 % (sizeof(alen) * 8))) {
        *(uint16_t *)ptr = 0xffff;
        *(uint64_t *)(ptr + 2) = alen;
        i = 10;
    } else {
        *(uint16_t *)ptr = 0xfffe;
        *(uint32_t *)(ptr + 2) = alen;
        i = 6;
    }

    while (i < 16 && alen) {
        sctx->ccm.s390x.buf.b[i] = *aad;
        ++aad;
        --alen;
        ++i;
    }
    while (i < 16) {
        sctx->ccm.s390x.buf.b[i] = 0;
        ++i;
    }

    sctx->ccm.s390x.kmac.icv.g[0] = 0;
    sctx->ccm.s390x.kmac.icv.g[1] = 0;
    s390x_kmac(sctx->ccm.s390x.nonce.b, 32, sctx->ccm.s390x.fc,
        &sctx->ccm.s390x.kmac);
    sctx->ccm.s390x.blocks += 2;

    rem = alen & 0xf;
    alen &= ~(size_t)0xf;
    if (alen) {
        s390x_kmac(aad, alen, sctx->ccm.s390x.fc, &sctx->ccm.s390x.kmac);
        sctx->ccm.s390x.blocks += alen >> 4;
        aad += alen;
    }
    if (rem) {
        for (i = 0; i < rem; i++)
            sctx->ccm.s390x.kmac.icv.b[i] ^= aad[i];

        s390x_km(sctx->ccm.s390x.kmac.icv.b, 16,
            sctx->ccm.s390x.kmac.icv.b, sctx->ccm.s390x.fc,
            sctx->ccm.s390x.kmac.k);
        sctx->ccm.s390x.blocks++;
    }
    return 1;
}

/*-
 * En/de-crypt plain/cipher-text. Compute tag from plaintext. Returns 1 for
 * success.
 */
static int s390x_aes_ccm_auth_encdec(PROV_CCM_CTX *ctx,
    const unsigned char *in,
    unsigned char *out, size_t len, int enc)
{
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;
    size_t n, rem;
    unsigned int i, l, num;
    unsigned char flags;

    flags = sctx->ccm.s390x.nonce.b[0];
    if (!(flags & S390X_CCM_AAD_FLAG)) {
        s390x_km(sctx->ccm.s390x.nonce.b, 16, sctx->ccm.s390x.kmac.icv.b,
            sctx->ccm.s390x.fc, sctx->ccm.s390x.kmac.k);
        sctx->ccm.s390x.blocks++;
    }
    l = flags & 0x7;
    sctx->ccm.s390x.nonce.b[0] = l;

    /*-
     * Reconstruct length from encoded length field
     * and initialize it with counter value.
     */
    n = 0;
    for (i = 15 - l; i < 15; i++) {
        n |= sctx->ccm.s390x.nonce.b[i];
        sctx->ccm.s390x.nonce.b[i] = 0;
        n <<= 8;
    }
    n |= sctx->ccm.s390x.nonce.b[15];
    sctx->ccm.s390x.nonce.b[15] = 1;

    if (n != len)
        return 0; /* length mismatch */

    if (enc) {
        /* Two operations per block plus one for tag encryption */
        sctx->ccm.s390x.blocks += (((len + 15) >> 4) << 1) + 1;
        if (sctx->ccm.s390x.blocks > (1ULL << 61))
            return 0; /* too much data */
    }

    num = 0;
    rem = len & 0xf;
    len &= ~(size_t)0xf;

    if (enc) {
        /* mac-then-encrypt */
        if (len)
            s390x_kmac(in, len, sctx->ccm.s390x.fc, &sctx->ccm.s390x.kmac);
        if (rem) {
            for (i = 0; i < rem; i++)
                sctx->ccm.s390x.kmac.icv.b[i] ^= in[len + i];

            s390x_km(sctx->ccm.s390x.kmac.icv.b, 16,
                sctx->ccm.s390x.kmac.icv.b,
                sctx->ccm.s390x.fc, sctx->ccm.s390x.kmac.k);
        }

        CRYPTO_ctr128_encrypt_ctr32(in, out, len + rem, &sctx->ccm.ks.ks,
            sctx->ccm.s390x.nonce.b, sctx->ccm.s390x.buf.b,
            &num, (ctr128_f)AES_ctr32_encrypt);
    } else {
        /* decrypt-then-mac */
        CRYPTO_ctr128_encrypt_ctr32(in, out, len + rem, &sctx->ccm.ks.ks,
            sctx->ccm.s390x.nonce.b, sctx->ccm.s390x.buf.b,
            &num, (ctr128_f)AES_ctr32_encrypt);

        if (len)
            s390x_kmac(out, len, sctx->ccm.s390x.fc, &sctx->ccm.s390x.kmac);
        if (rem) {
            for (i = 0; i < rem; i++)
                sctx->ccm.s390x.kmac.icv.b[i] ^= out[len + i];

            s390x_km(sctx->ccm.s390x.kmac.icv.b, 16,
                sctx->ccm.s390x.kmac.icv.b,
                sctx->ccm.s390x.fc, sctx->ccm.s390x.kmac.k);
        }
    }
    /* encrypt tag */
    for (i = 15 - l; i < 16; i++)
        sctx->ccm.s390x.nonce.b[i] = 0;

    s390x_km(sctx->ccm.s390x.nonce.b, 16, sctx->ccm.s390x.buf.b,
        sctx->ccm.s390x.fc, sctx->ccm.s390x.kmac.k);
    sctx->ccm.s390x.kmac.icv.g[0] ^= sctx->ccm.s390x.buf.g[0];
    sctx->ccm.s390x.kmac.icv.g[1] ^= sctx->ccm.s390x.buf.g[1];

    sctx->ccm.s390x.nonce.b[0] = flags; /* restore flags field */
    return 1;
}

static int s390x_aes_ccm_gettag(PROV_CCM_CTX *ctx,
    unsigned char *tag, size_t tlen)
{
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;

    if (tlen > ctx->m)
        return 0;
    memcpy(tag, sctx->ccm.s390x.kmac.icv.b, tlen);
    return 1;
}

static int s390x_aes_ccm_auth_encrypt(PROV_CCM_CTX *ctx,
    const unsigned char *in,
    unsigned char *out, size_t len,
    unsigned char *tag, size_t taglen)
{
    int rv;

    rv = s390x_aes_ccm_auth_encdec(ctx, in, out, len, 1);
    if (rv && tag != NULL)
        rv = s390x_aes_ccm_gettag(ctx, tag, taglen);
    return rv;
}

static int s390x_aes_ccm_auth_decrypt(PROV_CCM_CTX *ctx,
    const unsigned char *in,
    unsigned char *out, size_t len,
    unsigned char *expected_tag,
    size_t taglen)
{
    int rv = 0;
    PROV_AES_CCM_CTX *sctx = (PROV_AES_CCM_CTX *)ctx;

    rv = s390x_aes_ccm_auth_encdec(ctx, in, out, len, 0);
    if (rv) {
        if (CRYPTO_memcmp(sctx->ccm.s390x.kmac.icv.b, expected_tag, ctx->m) != 0)
            rv = 0;
    }
    if (rv == 0)
        OPENSSL_cleanse(out, len);
    return rv;
}

static const PROV_CCM_HW s390x_aes_ccm = {
    s390x_aes_ccm_initkey,
    s390x_aes_ccm_setiv,
    s390x_aes_ccm_setaad,
    s390x_aes_ccm_auth_encrypt,
    s390x_aes_ccm_auth_decrypt,
    s390x_aes_ccm_gettag
};

const PROV_CCM_HW *ossl_prov_aes_hw_ccm_s390x(size_t keybits)
{
    if ((keybits == 128 && S390X_aes_128_ccm_CAPABLE)
        || (keybits == 192 && S390X_aes_192_ccm_CAPABLE)
        || (keybits == 256 && S390X_aes_256_ccm_CAPABLE))
        return &s390x_aes_ccm;
    return NULL;
}

#endif

/* MODES: XTS */

#if defined(AES_XTS_S390X)

int s390x_aes_xts_cipher_stream(PROV_AES_XTS_CTX *xctx,
    unsigned char *out, size_t *outl,
    const unsigned char *in, size_t inl)
{
    S390X_KM_XTS_PARAMS *km = &xctx->plat.s390x.param.km;
    unsigned char *param = (unsigned char *)km + xctx->plat.s390x.offset;
    unsigned int fc = xctx->plat.s390x.fc;
    unsigned char tmp[2][AES_BLOCK_SIZE];
    unsigned char nap_n1[AES_BLOCK_SIZE];
    unsigned char drop[AES_BLOCK_SIZE];
    size_t len_incomplete, len_complete;

    len_incomplete = inl % AES_BLOCK_SIZE;
    len_complete = (len_incomplete == 0) ? inl : (inl / AES_BLOCK_SIZE - 1) * AES_BLOCK_SIZE;

    if (len_complete > 0)
        s390x_km(in, len_complete, out, fc, param);
    if (len_incomplete == 0)
        goto out;

    memcpy(tmp, in + len_complete, AES_BLOCK_SIZE + len_incomplete);
    /* swap NAP for decrypt */
    if (fc & S390X_DECRYPT) {
        memcpy(nap_n1, km->nap, AES_BLOCK_SIZE);
        s390x_km(tmp[0], AES_BLOCK_SIZE, drop, fc, param);
    }
    s390x_km(tmp[0], AES_BLOCK_SIZE, tmp[0], fc, param);
    if (fc & S390X_DECRYPT)
        memcpy(km->nap, nap_n1, AES_BLOCK_SIZE);

    memcpy(tmp[1] + len_incomplete, tmp[0] + len_incomplete,
        AES_BLOCK_SIZE - len_incomplete);
    s390x_km(tmp[1], AES_BLOCK_SIZE, out + len_complete, fc, param);
    memcpy(out + len_complete + AES_BLOCK_SIZE, tmp[0], len_incomplete);

    /* do not expose temporary data */
    OPENSSL_cleanse(tmp, sizeof(tmp));
out:
    memcpy(xctx->base.iv, km->tweak, AES_BLOCK_SIZE);
    *outl = inl;

    return 1;
}

static int cipher_hw_aes_xts_s390x_initkey(PROV_CIPHER_CTX *ctx,
    const unsigned char *key, size_t keylen)
{
    PROV_AES_XTS_CTX *xctx = (PROV_AES_XTS_CTX *)ctx;
    S390X_KM_XTS_PARAMS *km = &xctx->plat.s390x.param.km;
    unsigned int fc, offs;
    unsigned int dec = 0;
    int supported = 0;

    if (key != NULL) {
        switch (keylen) {
        case 128 / 8 * 2:
            fc = S390X_XTS_AES_128_MSA10;
            offs = 32;
            break;
        case 256 / 8 * 2:
            fc = S390X_XTS_AES_256_MSA10;
            offs = 0;
            break;
        default:
            fc = 0;
            break;
        }
    } else {
        fc = xctx->plat.s390x.fc & ~S390X_DECRYPT;
        offs = xctx->plat.s390x.offset;
    }

    if (fc != 0)
        supported = (OPENSSL_s390xcap_P.km[1] && S390X_CAPBIT(fc));
    if (!supported) {
        xctx->plat.s390x.fc = 0;
        xctx->plat.s390x.offset = 0;
        return 0;
    }

    if (xctx->base.iv_set) {
        if (xctx->base.ivlen > sizeof(km->tweak)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_IV_LENGTH);
            return 0;
        }
        memcpy(km->tweak, xctx->base.iv, xctx->base.ivlen);
        xctx->plat.s390x.iv_set = 1;
    }

    if (key != NULL) {
        memcpy(km->key + offs, key, keylen);
        xctx->plat.s390x.key_set = 1;
    }

    if (xctx->base.enc == 0)
        dec = S390X_DECRYPT;

    xctx->plat.s390x.fc = fc | dec;
    xctx->plat.s390x.offset = offs;

    memset(km->nap, 0, sizeof(km->nap));
    km->nap[0] = 0x1;

    return 1;
}

static void cipher_hw_aes_xts_s390x_copyctx(PROV_CIPHER_CTX *dst,
    const PROV_CIPHER_CTX *src)
{
    PROV_AES_XTS_CTX *sctx = (PROV_AES_XTS_CTX *)src;
    PROV_AES_XTS_CTX *dctx = (PROV_AES_XTS_CTX *)dst;

    *dctx = *sctx;
    dctx->xts.key1 = NULL;
    dctx->xts.key2 = NULL;
}

static const PROV_CIPHER_HW aes_xts_s390x = {
    cipher_hw_aes_xts_s390x_initkey,
    NULL,
    cipher_hw_aes_xts_s390x_copyctx
};

const PROV_CIPHER_HW *ossl_prov_cipher_hw_aes_xts_s390x(size_t keybits)
{
    switch (keybits) {
    case (128 * 2):
        if (OPENSSL_s390xcap_P.km[1] && S390X_CAPBIT(S390X_XTS_AES_128_MSA10))
            return &aes_xts_s390x;
        break;
    case (256 * 2):
        if (OPENSSL_s390xcap_P.km[1] && S390X_CAPBIT(S390X_XTS_AES_256_MSA10))
            return &aes_xts_s390x;
        break;
    default:
        break;
    }

    return NULL;
}

#endif
