/*
 * Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Refer to "The TLS Protocol Version 1.0" Section 5
 * (https://tools.ietf.org/html/rfc2246#section-5) and
 * "The Transport Layer Security (TLS) Protocol Version 1.2" Section 5
 * (https://tools.ietf.org/html/rfc5246#section-5).
 *
 * For TLS v1.0 and TLS v1.1 the TLS PRF algorithm is given by:
 *
 *   PRF(secret, label, seed) = P_MD5(S1, label + seed) XOR
 *                              P_SHA-1(S2, label + seed)
 *
 * where P_MD5 and P_SHA-1 are defined by P_<hash>, below, and S1 and S2 are
 * two halves of the secret (with the possibility of one shared byte, in the
 * case where the length of the original secret is odd).  S1 is taken from the
 * first half of the secret, S2 from the second half.
 *
 * For TLS v1.2 the TLS PRF algorithm is given by:
 *
 *   PRF(secret, label, seed) = P_<hash>(secret, label + seed)
 *
 * where hash is SHA-256 for all cipher suites defined in RFC 5246 as well as
 * those published prior to TLS v1.2 while the TLS v1.2 protocol is in effect,
 * unless defined otherwise by the cipher suite.
 *
 * P_<hash> is an expansion function that uses a single hash function to expand
 * a secret and seed into an arbitrary quantity of output:
 *
 *   P_<hash>(secret, seed) = HMAC_<hash>(secret, A(1) + seed) +
 *                            HMAC_<hash>(secret, A(2) + seed) +
 *                            HMAC_<hash>(secret, A(3) + seed) + ...
 *
 * where + indicates concatenation.  P_<hash> can be iterated as many times as
 * is necessary to produce the required quantity of data.
 *
 * A(i) is defined as:
 *     A(0) = seed
 *     A(i) = HMAC_<hash>(secret, A(i-1))
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/numbers.h"
#include "crypto/evp.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_util.h"
#include "e_os.h"

static OSSL_FUNC_kdf_newctx_fn kdf_tls1_prf_new;
static OSSL_FUNC_kdf_freectx_fn kdf_tls1_prf_free;
static OSSL_FUNC_kdf_reset_fn kdf_tls1_prf_reset;
static OSSL_FUNC_kdf_derive_fn kdf_tls1_prf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_tls1_prf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_tls1_prf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_tls1_prf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_tls1_prf_get_ctx_params;

static int tls1_prf_alg(EVP_MAC_CTX *mdctx, EVP_MAC_CTX *sha1ctx,
                        const unsigned char *sec, size_t slen,
                        const unsigned char *seed, size_t seed_len,
                        unsigned char *out, size_t olen);

#define TLS1_PRF_MAXBUF 1024

/* TLS KDF kdf context structure */
typedef struct {
    void *provctx;

    /* MAC context for the main digest */
    EVP_MAC_CTX *P_hash;
    /* MAC context for SHA1 for the MD5/SHA-1 combined PRF */
    EVP_MAC_CTX *P_sha1;

    /* Secret value to use for PRF */
    unsigned char *sec;
    size_t seclen;
    /* Buffer of concatenated seed data */
    unsigned char seed[TLS1_PRF_MAXBUF];
    size_t seedlen;
} TLS1_PRF;

static void *kdf_tls1_prf_new(void *provctx)
{
    TLS1_PRF *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    ctx->provctx = provctx;
    return ctx;
}

static void kdf_tls1_prf_free(void *vctx)
{
    TLS1_PRF *ctx = (TLS1_PRF *)vctx;

    if (ctx != NULL) {
        kdf_tls1_prf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_tls1_prf_reset(void *vctx)
{
    TLS1_PRF *ctx = (TLS1_PRF *)vctx;
    void *provctx = ctx->provctx;

    EVP_MAC_CTX_free(ctx->P_hash);
    EVP_MAC_CTX_free(ctx->P_sha1);
    OPENSSL_clear_free(ctx->sec, ctx->seclen);
    OPENSSL_cleanse(ctx->seed, ctx->seedlen);
    memset(ctx, 0, sizeof(*ctx));
    ctx->provctx = provctx;
}

static int kdf_tls1_prf_derive(void *vctx, unsigned char *key, size_t keylen,
                               const OSSL_PARAM params[])
{
    TLS1_PRF *ctx = (TLS1_PRF *)vctx;

    if (!ossl_prov_is_running() || !kdf_tls1_prf_set_ctx_params(ctx, params))
        return 0;

    if (ctx->P_hash == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    if (ctx->sec == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SECRET);
        return 0;
    }
    if (ctx->seedlen == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SEED);
        return 0;
    }
    if (keylen == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
        return 0;
    }

    return tls1_prf_alg(ctx->P_hash, ctx->P_sha1,
                        ctx->sec, ctx->seclen,
                        ctx->seed, ctx->seedlen,
                        key, keylen);
}

static int kdf_tls1_prf_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    const OSSL_PARAM *p;
    TLS1_PRF *ctx = vctx;
    OSSL_LIB_CTX *libctx = PROV_LIBCTX_OF(ctx->provctx);

    if (params == NULL)
        return 1;

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_DIGEST)) != NULL) {
        if (OPENSSL_strcasecmp(p->data, SN_md5_sha1) == 0) {
            if (!ossl_prov_macctx_load_from_params(&ctx->P_hash, params,
                                                   OSSL_MAC_NAME_HMAC,
                                                   NULL, SN_md5, libctx)
                || !ossl_prov_macctx_load_from_params(&ctx->P_sha1, params,
                                                      OSSL_MAC_NAME_HMAC,
                                                      NULL, SN_sha1, libctx))
                return 0;
        } else {
            EVP_MAC_CTX_free(ctx->P_sha1);
            if (!ossl_prov_macctx_load_from_params(&ctx->P_hash, params,
                                                   OSSL_MAC_NAME_HMAC,
                                                   NULL, NULL, libctx))
                return 0;
        }
    }

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_SECRET)) != NULL) {
        OPENSSL_clear_free(ctx->sec, ctx->seclen);
        ctx->sec = NULL;
        if (!OSSL_PARAM_get_octet_string(p, (void **)&ctx->sec, 0, &ctx->seclen))
            return 0;
    }
    /* The seed fields concatenate, so process them all */
    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_SEED)) != NULL) {
        for (; p != NULL; p = OSSL_PARAM_locate_const(p + 1,
                                                      OSSL_KDF_PARAM_SEED)) {
            const void *q = ctx->seed + ctx->seedlen;
            size_t sz = 0;

            if (p->data_size != 0
                && p->data != NULL
                && !OSSL_PARAM_get_octet_string(p, (void **)&q,
                                                TLS1_PRF_MAXBUF - ctx->seedlen,
                                                &sz))
                return 0;
            ctx->seedlen += sz;
        }
    }
    return 1;
}

static const OSSL_PARAM *kdf_tls1_prf_settable_ctx_params(
        ossl_unused void *ctx, ossl_unused void *provctx)
{
    static const OSSL_PARAM known_settable_ctx_params[] = {
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_PROPERTIES, NULL, 0),
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_DIGEST, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SECRET, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SEED, NULL, 0),
        OSSL_PARAM_END
    };
    return known_settable_ctx_params;
}

static int kdf_tls1_prf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    OSSL_PARAM *p;

    if ((p = OSSL_PARAM_locate(params, OSSL_KDF_PARAM_SIZE)) != NULL)
        return OSSL_PARAM_set_size_t(p, SIZE_MAX);
    return -2;
}

static const OSSL_PARAM *kdf_tls1_prf_gettable_ctx_params(
        ossl_unused void *ctx, ossl_unused void *provctx)
{
    static const OSSL_PARAM known_gettable_ctx_params[] = {
        OSSL_PARAM_size_t(OSSL_KDF_PARAM_SIZE, NULL),
        OSSL_PARAM_END
    };
    return known_gettable_ctx_params;
}

const OSSL_DISPATCH ossl_kdf_tls1_prf_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void(*)(void))kdf_tls1_prf_new },
    { OSSL_FUNC_KDF_FREECTX, (void(*)(void))kdf_tls1_prf_free },
    { OSSL_FUNC_KDF_RESET, (void(*)(void))kdf_tls1_prf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void(*)(void))kdf_tls1_prf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_tls1_prf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS,
      (void(*)(void))kdf_tls1_prf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_tls1_prf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS,
      (void(*)(void))kdf_tls1_prf_get_ctx_params },
    { 0, NULL }
};

/*
 * Refer to "The TLS Protocol Version 1.0" Section 5
 * (https://tools.ietf.org/html/rfc2246#section-5) and
 * "The Transport Layer Security (TLS) Protocol Version 1.2" Section 5
 * (https://tools.ietf.org/html/rfc5246#section-5).
 *
 * P_<hash> is an expansion function that uses a single hash function to expand
 * a secret and seed into an arbitrary quantity of output:
 *
 *   P_<hash>(secret, seed) = HMAC_<hash>(secret, A(1) + seed) +
 *                            HMAC_<hash>(secret, A(2) + seed) +
 *                            HMAC_<hash>(secret, A(3) + seed) + ...
 *
 * where + indicates concatenation.  P_<hash> can be iterated as many times as
 * is necessary to produce the required quantity of data.
 *
 * A(i) is defined as:
 *     A(0) = seed
 *     A(i) = HMAC_<hash>(secret, A(i-1))
 */
static int tls1_prf_P_hash(EVP_MAC_CTX *ctx_init,
                           const unsigned char *sec, size_t sec_len,
                           const unsigned char *seed, size_t seed_len,
                           unsigned char *out, size_t olen)
{
    size_t chunk;
    EVP_MAC_CTX *ctx = NULL, *ctx_Ai = NULL;
    unsigned char Ai[EVP_MAX_MD_SIZE];
    size_t Ai_len;
    int ret = 0;

    if (!EVP_MAC_init(ctx_init, sec, sec_len, NULL))
        goto err;
    chunk = EVP_MAC_CTX_get_mac_size(ctx_init);
    if (chunk == 0)
        goto err;
    /* A(0) = seed */
    ctx_Ai = EVP_MAC_CTX_dup(ctx_init);
    if (ctx_Ai == NULL)
        goto err;
    if (seed != NULL && !EVP_MAC_update(ctx_Ai, seed, seed_len))
        goto err;

    for (;;) {
        /* calc: A(i) = HMAC_<hash>(secret, A(i-1)) */
        if (!EVP_MAC_final(ctx_Ai, Ai, &Ai_len, sizeof(Ai)))
            goto err;
        EVP_MAC_CTX_free(ctx_Ai);
        ctx_Ai = NULL;

        /* calc next chunk: HMAC_<hash>(secret, A(i) + seed) */
        ctx = EVP_MAC_CTX_dup(ctx_init);
        if (ctx == NULL)
            goto err;
        if (!EVP_MAC_update(ctx, Ai, Ai_len))
            goto err;
        /* save state for calculating next A(i) value */
        if (olen > chunk) {
            ctx_Ai = EVP_MAC_CTX_dup(ctx);
            if (ctx_Ai == NULL)
                goto err;
        }
        if (seed != NULL && !EVP_MAC_update(ctx, seed, seed_len))
            goto err;
        if (olen <= chunk) {
            /* last chunk - use Ai as temp bounce buffer */
            if (!EVP_MAC_final(ctx, Ai, &Ai_len, sizeof(Ai)))
                goto err;
            memcpy(out, Ai, olen);
            break;
        }
        if (!EVP_MAC_final(ctx, out, NULL, olen))
            goto err;
        EVP_MAC_CTX_free(ctx);
        ctx = NULL;
        out += chunk;
        olen -= chunk;
    }
    ret = 1;
 err:
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_CTX_free(ctx_Ai);
    OPENSSL_cleanse(Ai, sizeof(Ai));
    return ret;
}

/*
 * Refer to "The TLS Protocol Version 1.0" Section 5
 * (https://tools.ietf.org/html/rfc2246#section-5) and
 * "The Transport Layer Security (TLS) Protocol Version 1.2" Section 5
 * (https://tools.ietf.org/html/rfc5246#section-5).
 *
 * For TLS v1.0 and TLS v1.1:
 *
 *   PRF(secret, label, seed) = P_MD5(S1, label + seed) XOR
 *                              P_SHA-1(S2, label + seed)
 *
 * S1 is taken from the first half of the secret, S2 from the second half.
 *
 *   L_S = length in bytes of secret;
 *   L_S1 = L_S2 = ceil(L_S / 2);
 *
 * For TLS v1.2:
 *
 *   PRF(secret, label, seed) = P_<hash>(secret, label + seed)
 */
static int tls1_prf_alg(EVP_MAC_CTX *mdctx, EVP_MAC_CTX *sha1ctx,
                        const unsigned char *sec, size_t slen,
                        const unsigned char *seed, size_t seed_len,
                        unsigned char *out, size_t olen)
{
    if (sha1ctx != NULL) {
        /* TLS v1.0 and TLS v1.1 */
        size_t i;
        unsigned char *tmp;
        /* calc: L_S1 = L_S2 = ceil(L_S / 2) */
        size_t L_S1 = (slen + 1) / 2;
        size_t L_S2 = L_S1;

        if (!tls1_prf_P_hash(mdctx, sec, L_S1,
                             seed, seed_len, out, olen))
            return 0;

        if ((tmp = OPENSSL_malloc(olen)) == NULL) {
            ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
            return 0;
        }

        if (!tls1_prf_P_hash(sha1ctx, sec + slen - L_S2, L_S2,
                             seed, seed_len, tmp, olen)) {
            OPENSSL_clear_free(tmp, olen);
            return 0;
        }
        for (i = 0; i < olen; i++)
            out[i] ^= tmp[i];
        OPENSSL_clear_free(tmp, olen);
        return 1;
    }

    /* TLS v1.2 */
    if (!tls1_prf_P_hash(mdctx, sec, slen, seed, seed_len, out, olen))
        return 0;

    return 1;
}
