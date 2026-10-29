/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/deprecated.h" /* PKCS1_MGF1() */

#include <string.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/rsa.h> /* PKCS1_MGF1() */
#include "slh_dsa_local.h"
#include "slh_dsa_key.h"

#include "openssl/sha.h"
#include "internal/sha3.h"
#include "crypto/evp.h"
#include "crypto/sha.h"

#define MAX_DIGEST_SIZE 64 /* SHA-512 is used for security category 3 & 5 */
#define NIBBLE_MASK 15

/* Most hash functions in SLH-DSA truncate the output */
#define sha256_final(ctx, out, outlen)    \
    (ctx)->md_len = (unsigned int)outlen; \
    SHA256_Final(out, ctx)

#define sha512_final(ctx, out, outlen)    \
    (ctx)->md_len = (unsigned int)outlen; \
    SHA512_Final(out, ctx)

static OSSL_SLH_HASHFUNC_PRF slh_prf_sha256;
static OSSL_SLH_HASHFUNC_PRF slh_prf_shake;

static OSSL_SLH_HASHFUNC_F slh_f_sha256;
static OSSL_SLH_HASHFUNC_F slh_f_shake;

static OSSL_SLH_HASHFUNC_PRF_MSG slh_prf_msg_sha2;
static OSSL_SLH_HASHFUNC_PRF_MSG slh_prf_msg_shake;

static OSSL_SLH_HASHFUNC_H_MSG slh_hmsg_sha256;
static OSSL_SLH_HASHFUNC_H_MSG slh_hmsg_sha512;
static OSSL_SLH_HASHFUNC_H_MSG slh_hmsg_shake;

static OSSL_SLH_HASHFUNC_H slh_h_sha256;
static OSSL_SLH_HASHFUNC_H slh_h_sha512;
static OSSL_SLH_HASHFUNC_H slh_h_shake;
static OSSL_SLH_HASHFUNC_T slh_t_sha256;
static OSSL_SLH_HASHFUNC_T slh_t_sha512;
static OSSL_SLH_HASHFUNC_wots_pk_gen slh_wots_pk_gen_sha2;
static OSSL_SLH_HASHFUNC_wots_pk_gen slh_wots_pk_gen_shake;

static const uint8_t zeros[128] = { 0 };

/* See FIPS 205 Section 11.1 */
static int
slh_hmsg_shake(SLH_DSA_HASH_CTX *hctx, const uint8_t *r,
    const uint8_t *pk_seed, const uint8_t *pk_root,
    const uint8_t *msg, size_t msg_len,
    uint8_t *out, size_t out_len)
{
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->shactx);
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t m = params->m;
    size_t n = params->n;

    ossl_sha3_reset(sctx);
    ossl_sha3_absorb(sctx, r, n);
    ossl_sha3_absorb(sctx, pk_seed, n);
    ossl_sha3_absorb(sctx, pk_root, n);
    ossl_sha3_absorb(sctx, msg, msg_len);
    ossl_sha3_squeeze(sctx, out, m);
    return 1;
}

static int
slh_prf_msg_shake(SLH_DSA_HASH_CTX *hctx, const uint8_t *sk_prf,
    const uint8_t *opt_rand, const uint8_t *msg, size_t msg_len,
    WPACKET *pkt)
{
    int ret;
    unsigned char out[SLH_MAX_N];
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t n = params->n;
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->shactx);

    ossl_sha3_reset(sctx);
    ossl_sha3_absorb(sctx, sk_prf, n);
    ossl_sha3_absorb(sctx, opt_rand, n);
    ossl_sha3_absorb(sctx, msg, msg_len);
    ossl_sha3_squeeze(sctx, out, n);
    ret = WPACKET_memcpy(pkt, out, n);
    OPENSSL_cleanse(out, sizeof(out));
    return ret;
}

static int
slh_f_shake(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *m1, size_t m1_len, uint8_t *out, size_t out_len)
{
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t n = params->n;
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->scratch);

    *sctx = *((KECCAK1600_CTX *)(hctx->shactx_pkseed));
    ossl_sha3_absorb(sctx, adrs, SLH_ADRS_SIZE);
    ossl_sha3_absorb(sctx, m1, m1_len);
    ossl_sha3_squeeze(sctx, out, n);
    return 1;
}

static int
slh_prf_shake(SLH_DSA_HASH_CTX *hctx,
    const uint8_t *pk_seed, const uint8_t *sk_seed,
    const uint8_t *adrs, uint8_t *out, size_t out_len)
{
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t n = params->n;
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->scratch);

    *sctx = *((KECCAK1600_CTX *)(hctx->shactx_pkseed));
    ossl_sha3_absorb(sctx, adrs, SLH_ADRS_SIZE);
    ossl_sha3_absorb(sctx, sk_seed, n);
    ossl_sha3_squeeze(sctx, out, n);
    return 1;
}

static int
slh_h_shake(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *m1, const uint8_t *m2, uint8_t *out, size_t out_len)
{
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t n = params->n;

    *sctx = *((KECCAK1600_CTX *)(hctx->shactx_pkseed));
    ossl_sha3_absorb(sctx, adrs, SLH_ADRS_SIZE);
    ossl_sha3_absorb(sctx, m1, n);
    ossl_sha3_absorb(sctx, m2, n);
    ossl_sha3_squeeze(sctx, out, n);
    return 1;
}

/* FIPS 205 Section 11.2.1 and 11.2.2 */

static int
slh_hmsg_sha256(SLH_DSA_HASH_CTX *hctx, const uint8_t *r, const uint8_t *pk_seed,
    const uint8_t *pk_root, const uint8_t *msg, size_t msg_len,
    uint8_t *out, size_t out_len)
{
    int ret;
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t m = params->m;
    size_t n = params->n;
    uint8_t seed[2 * SLH_MAX_N + SHA256_DIGEST_LENGTH];
    long seed_len = SHA256_DIGEST_LENGTH + (long)(2 * n);

    memcpy(seed, r, n);
    memcpy(seed + n, pk_seed, n);

    SHA256_Init(sctx);
    SHA256_Update(sctx, r, n);
    SHA256_Update(sctx, pk_seed, n);
    SHA256_Update(sctx, pk_root, n);
    SHA256_Update(sctx, msg, msg_len);
    ret = SHA256_Final(seed + 2 * n, sctx)
        && (PKCS1_MGF1(out, (long)m, seed, seed_len, hctx->key->md) == 0);
    OPENSSL_cleanse(seed, sizeof(seed));
    return ret;
}

static int
slh_hmsg_sha512(SLH_DSA_HASH_CTX *hctx, const uint8_t *r, const uint8_t *pk_seed,
    const uint8_t *pk_root, const uint8_t *msg, size_t msg_len,
    uint8_t *out, size_t out_len)
{
    int ret;
    SHA512_CTX *sctx = (SHA512_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *params = hctx->key->params;
    size_t m = params->m;
    size_t n = params->n;
    uint8_t seed[2 * SLH_MAX_N + SHA512_DIGEST_LENGTH];
    long seed_len = SHA512_DIGEST_LENGTH + (long)(2 * n);

    memcpy(seed, r, n);
    memcpy(seed + n, pk_seed, n);

    SHA512_Init(sctx);
    SHA512_Update(sctx, r, n);
    SHA512_Update(sctx, pk_seed, n);
    SHA512_Update(sctx, pk_root, n);
    SHA512_Update(sctx, msg, msg_len);
    ret = SHA512_Final(seed + 2 * n, sctx)
        && (PKCS1_MGF1(out, (long)m, seed, seed_len, hctx->key->md_sha512) == 0);
    OPENSSL_cleanse(seed, sizeof(seed));
    return ret;
}

static int
slh_prf_msg_sha2(SLH_DSA_HASH_CTX *hctx,
    const uint8_t *sk_prf, const uint8_t *opt_rand,
    const uint8_t *msg, size_t msg_len, WPACKET *pkt)
{
    int ret;
    const SLH_DSA_KEY *key = hctx->key;
    EVP_MAC_CTX *mctx = hctx->hmac_ctx;
    const SLH_DSA_PARAMS *prms = key->params;
    size_t n = prms->n;
    uint8_t mac[MAX_DIGEST_SIZE] = { 0 };
    OSSL_PARAM *p = NULL;
    OSSL_PARAM params[3];

    /*
     * Due to the way HMAC works, it is not possible to do this code early
     * in hmac_ctx_new() since it requires a key in order to set the digest.
     * So we do a lazy update here on the first call.
     */
    if (hctx->hmac_digest_used == 0) {
        const char *nm = EVP_MD_get0_name(key->md_sha512 == NULL ? key->md : key->md_sha512);

        p = params;
        /* The underlying digest to be used */
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char *)nm, 0);
        if (key->propq != NULL)
            *p++ = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_PROPERTIES,
                (char *)key->propq, 0);
        *p = OSSL_PARAM_construct_end();
        p = params;
        hctx->hmac_digest_used = 1;
    }

    ret = EVP_MAC_init(mctx, sk_prf, n, p) == 1
        && EVP_MAC_update(mctx, opt_rand, n) == 1
        && EVP_MAC_update(mctx, msg, msg_len) == 1
        && EVP_MAC_final(mctx, mac, NULL, sizeof(mac)) == 1
        && WPACKET_memcpy(pkt, mac, n); /* Truncate output to n bytes */
    OPENSSL_cleanse(mac, sizeof(mac));
    return ret;
}

static int
slh_prf_sha256(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed,
    const uint8_t *sk_seed, const uint8_t *adrs,
    uint8_t *out, size_t out_len)
{
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->scratch);
    size_t n = hctx->key->params->n;

    *sctx = *((SHA256_CTX *)hctx->shactx_pkseed);
    SHA256_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA256_Update(sctx, sk_seed, n);
    sha256_final(sctx, out, n);
    return 1;
}

static int
slh_wots_pk_gen_sha2(SLH_DSA_HASH_CTX *hctx,
    const uint8_t *sk_seed, const uint8_t *pk_seed,
    uint8_t *adrs, uint8_t *pk_out, size_t pk_out_len)
{
    int ret = 0;
    size_t n = hctx->key->params->n;
    size_t i, j = 0, len = SLH_WOTS_LEN(n);
    uint8_t sk[SLH_MAX_N];
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->shactx_pkseed);
    SHA256_CTX *ctx = (SHA256_CTX *)(hctx->scratch);
    const SLH_ADRS_FUNC *adrsf = hctx->key->adrs_func;
    SLH_ADRS_DECLARE(sk_adrs);
    SLH_ADRS_FN_DECLARE(adrsf, set_chain_address);
    SLH_ADRS_FN_DECLARE(adrsf, set_hash_address);

    adrsf->copy(sk_adrs, adrs);
    adrsf->set_type_and_clear(sk_adrs, SLH_ADRS_TYPE_WOTS_PRF);
    adrsf->copy_keypair_address(sk_adrs, adrs);

    for (i = 0; i < len; ++i) { /* len = 2n + 3 */
        set_chain_address(sk_adrs, (uint32_t)i);

        /* PRF */
        *ctx = *sctx;
        SHA256_Update(ctx, sk_adrs, SLH_ADRSC_SIZE);
        SHA256_Update(ctx, sk_seed, n);
        sha256_final(ctx, sk, n);

        set_chain_address(adrs, (uint32_t)i);
        for (j = 0; j < NIBBLE_MASK; ++j) {
            set_hash_address(adrs, (uint32_t)j);
            /* F */
            *ctx = *sctx;
            SHA256_Update(ctx, adrs, SLH_ADRSC_SIZE);
            SHA256_Update(ctx, sk, n);
            sha256_final(ctx, sk, n);
        }
        memcpy(pk_out, sk, n);
        pk_out += n;
    }
    ret = 1;
    OPENSSL_cleanse(sk, sizeof(sk));
    return ret;
}

int slh_wots_pk_gen_shake(SLH_DSA_HASH_CTX *hctx,
    const uint8_t *sk_seed, const uint8_t *pk_seed,
    uint8_t *adrs, uint8_t *pk_out, size_t pk_out_len)
{
    int ret = 0;
    size_t n = hctx->key->params->n;
    size_t i, j = 0, len = SLH_WOTS_LEN(n);
    uint8_t sk[SLH_MAX_N];
    const SLH_ADRS_FUNC *adrsf = hctx->key->adrs_func;
    SLH_ADRS_DECLARE(sk_adrs);
    SLH_ADRS_FN_DECLARE(adrsf, set_chain_address);
    SLH_ADRS_FN_DECLARE(adrsf, set_hash_address);
    KECCAK1600_CTX *sctx = (KECCAK1600_CTX *)(hctx->shactx_pkseed);
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)(hctx->scratch);

    adrsf->copy(sk_adrs, adrs);
    adrsf->set_type_and_clear(sk_adrs, SLH_ADRS_TYPE_WOTS_PRF);
    adrsf->copy_keypair_address(sk_adrs, adrs);

    for (i = 0; i < len; ++i) { /* len = 2n + 3 */
        set_chain_address(sk_adrs, (uint32_t)i);

        /* PRF */
        *ctx = *sctx;
        ossl_sha3_absorb(ctx, sk_adrs, SLH_ADRS_SIZE);
        ossl_sha3_absorb(ctx, sk_seed, n);
        ossl_sha3_squeeze(ctx, sk, n);

        set_chain_address(adrs, (uint32_t)i);
        for (j = 0; j < NIBBLE_MASK; ++j) {
            set_hash_address(adrs, (uint32_t)j);
            /* F */
            *ctx = *sctx;
            ossl_sha3_absorb(ctx, adrs, SLH_ADRS_SIZE);
            ossl_sha3_absorb(ctx, sk, n);
            ossl_sha3_squeeze(ctx, sk, n);
        }
        memcpy(pk_out, sk, n);
        pk_out += n;
    }
    ret = 1;
    OPENSSL_cleanse(sk, sizeof(sk));
    return ret;
}

static int
slh_f_sha256(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *m1, size_t m1_len, uint8_t *out, size_t out_len)
{
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->scratch);

    *sctx = *((SHA256_CTX *)hctx->shactx_pkseed);
    SHA256_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA256_Update(sctx, m1, m1_len);
    sha256_final(sctx, out, hctx->key->params->n);
    return 1;
}

static int
slh_h_sha256(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *m1, const uint8_t *m2, uint8_t *out, size_t out_len)
{
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *prms = hctx->key->params;
    size_t n = prms->n;

    *sctx = *((SHA256_CTX *)hctx->shactx_pkseed);
    SHA256_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA256_Update(sctx, m1, n);
    SHA256_Update(sctx, m2, n);
    sha256_final(sctx, out, n);
    return 1;
}

static int
slh_h_sha512(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *m1, const uint8_t *m2, uint8_t *out, size_t out_len)
{
    SHA512_CTX *sctx = (SHA512_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *prms = hctx->key->params;
    size_t n = prms->n;

    SHA512_Init(sctx);
    SHA512_Update(sctx, pk_seed, n);
    SHA512_Update(sctx, zeros, 128 - n);
    SHA512_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA512_Update(sctx, m1, n);
    SHA512_Update(sctx, m2, n);
    sha512_final(sctx, out, n);
    return 1;
}

static int
slh_t_sha256(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *ml, size_t ml_len, uint8_t *out, size_t out_len)
{
    SHA256_CTX *sctx = (SHA256_CTX *)(hctx->scratch);

    *sctx = *((SHA256_CTX *)hctx->shactx_pkseed);
    SHA256_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA256_Update(sctx, ml, ml_len);
    sha256_final(sctx, out, hctx->key->params->n);
    return 1;
}

static int
slh_t_sha512(SLH_DSA_HASH_CTX *hctx, const uint8_t *pk_seed, const uint8_t *adrs,
    const uint8_t *ml, size_t ml_len, uint8_t *out, size_t out_len)
{
    SHA512_CTX *sctx = (SHA512_CTX *)(hctx->scratch);
    const SLH_DSA_PARAMS *prms = hctx->key->params;
    size_t n = prms->n;

    SHA512_Init(sctx);
    SHA512_Update(sctx, pk_seed, n);
    SHA512_Update(sctx, zeros, 128 - n);
    SHA512_Update(sctx, adrs, SLH_ADRSC_SIZE);
    SHA512_Update(sctx, ml, ml_len);
    sha512_final(sctx, out, hctx->key->params->n);
    return 1;
}

static int slh_hash_shake_precache(SLH_DSA_HASH_CTX *hctx, const uint8_t *pkseed, size_t n)
{
    KECCAK1600_CTX *ctx = NULL, *seedctx = NULL, *scratch = NULL;

    ctx = ossl_shake256_new();
    if (ctx == NULL)
        return 0;
    seedctx = OPENSSL_memdup(ctx, sizeof(*ctx));
    scratch = OPENSSL_malloc(sizeof(*scratch));
    if (seedctx == NULL || scratch == NULL) {
        OPENSSL_free(ctx);
        OPENSSL_free(seedctx);
        OPENSSL_free(scratch);
        return 0;
    }
    ossl_sha3_absorb(seedctx, pkseed, n);
    hctx->shactx = (void *)ctx;
    hctx->shactx_pkseed = (void *)seedctx;
    hctx->shactx_len = sizeof(*ctx);
    hctx->scratch = (void *)scratch;
    hctx->scratch_len = sizeof(*scratch);
    return 1;
}

static int slh_hash_shake_dup(SLH_DSA_HASH_CTX *dst, const SLH_DSA_HASH_CTX *src)
{
    if (src->shactx != NULL) {
        dst->shactx = OPENSSL_memdup(src->shactx, sizeof(KECCAK1600_CTX));
        if (dst->shactx == NULL)
            return 0;
    }
    if (src->shactx_pkseed != NULL) {
        dst->shactx_pkseed = OPENSSL_memdup(src->shactx_pkseed, sizeof(KECCAK1600_CTX));
        if (dst->shactx_pkseed == NULL) {
            OPENSSL_free(dst->shactx);
            dst->shactx = NULL;
            return 0;
        }
    }
    dst->shactx_len = src->shactx_len;
    /* A prehashed context needs a scratch context, its content is transient */
    if (dst->shactx_pkseed != NULL) {
        dst->scratch = OPENSSL_malloc(sizeof(KECCAK1600_CTX));
        if (dst->scratch == NULL)
            return 0;
        dst->scratch_len = sizeof(KECCAK1600_CTX);
    } else {
        dst->scratch = NULL;
        dst->scratch_len = 0;
    }
    return 1;
}

static int slh_hash_sha256_precache(SLH_DSA_HASH_CTX *hctx, const uint8_t *pkseed, size_t n)
{
    SHA256_CTX *ctx = OPENSSL_zalloc(sizeof(*ctx));
    /* The scratch context is also used as a SHA512_CTX by category 3 and 5 */
    size_t scratch_len = sizeof(SHA512_CTX);

    if (ctx == NULL)
        return 0;
    if ((hctx->scratch = OPENSSL_malloc(scratch_len)) == NULL) {
        OPENSSL_free(ctx);
        return 0;
    }
    hctx->scratch_len = scratch_len;
    SHA256_Init(ctx);
    SHA256_Update(ctx, pkseed, n);
    SHA256_Update(ctx, zeros, 64 - n);
    hctx->shactx_pkseed = (void *)ctx;
    hctx->shactx_len = sizeof(*ctx);
    return 1;
}

static int slh_hash_sha256_dup(SLH_DSA_HASH_CTX *dst, const SLH_DSA_HASH_CTX *src)
{
    if (src->shactx_pkseed != NULL) {
        dst->shactx_pkseed = OPENSSL_memdup(src->shactx_pkseed, sizeof(SHA256_CTX));
        if (dst->shactx_pkseed == NULL)
            return 0;
    }
    /*
     * A prehashed context needs a scratch context, its content is transient.
     * As in slh_hash_sha256_precache() the scratch context is sized to also
     * serve as a SHA512_CTX for security categories 3 and 5.
     */
    dst->shactx_len = src->shactx_len;
    if (dst->shactx_pkseed != NULL) {
        dst->scratch = OPENSSL_malloc(sizeof(SHA512_CTX));
        if (dst->scratch == NULL)
            return 0;
        dst->scratch_len = sizeof(SHA512_CTX);
    } else {
        dst->scratch = NULL;
        dst->scratch_len = 0;
    }
    return 1;
}

const SLH_HASH_FUNC *ossl_slh_get_hash_fn(int is_shake, int security_category)
{
    static const SLH_HASH_FUNC methods[] = {
        { slh_hash_shake_precache,
            slh_hash_shake_dup,
            slh_hmsg_shake,
            slh_prf_shake,
            slh_prf_msg_shake,
            slh_f_shake,
            slh_h_shake,
            slh_f_shake,
            slh_wots_pk_gen_shake },
        { slh_hash_sha256_precache,
            slh_hash_sha256_dup,
            slh_hmsg_sha256,
            slh_prf_sha256,
            slh_prf_msg_sha2,
            slh_f_sha256,
            slh_h_sha256,
            slh_t_sha256,
            slh_wots_pk_gen_sha2 },
        { slh_hash_sha256_precache,
            slh_hash_sha256_dup,
            slh_hmsg_sha512,
            slh_prf_sha256,
            slh_prf_msg_sha2,
            slh_f_sha256,
            slh_h_sha512,
            slh_t_sha512,
            slh_wots_pk_gen_sha2 }
    };
    return &methods[is_shake ? 0 : (security_category == 1 ? 1 : 2)];
}
