/*
 * Copyright 2016-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * HMAC low level APIs are deprecated for public use, but still ok for internal
 * use.
 */
#include "internal/deprecated.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/numbers.h"
#include "crypto/evp.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_util.h"
#include "e_os.h"

#define HKDF_MAXBUF 2048

static OSSL_FUNC_kdf_newctx_fn kdf_hkdf_new;
static OSSL_FUNC_kdf_freectx_fn kdf_hkdf_free;
static OSSL_FUNC_kdf_reset_fn kdf_hkdf_reset;
static OSSL_FUNC_kdf_derive_fn kdf_hkdf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_hkdf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_hkdf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_hkdf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_hkdf_get_ctx_params;

static int HKDF(OSSL_LIB_CTX *libctx, const EVP_MD *evp_md,
                const unsigned char *salt, size_t salt_len,
                const unsigned char *key, size_t key_len,
                const unsigned char *info, size_t info_len,
                unsigned char *okm, size_t okm_len);
static int HKDF_Extract(OSSL_LIB_CTX *libctx, const EVP_MD *evp_md,
                        const unsigned char *salt, size_t salt_len,
                        const unsigned char *ikm, size_t ikm_len,
                        unsigned char *prk, size_t prk_len);
static int HKDF_Expand(const EVP_MD *evp_md,
                       const unsigned char *prk, size_t prk_len,
                       const unsigned char *info, size_t info_len,
                       unsigned char *okm, size_t okm_len);

typedef struct {
    void *provctx;
    int mode;
    PROV_DIGEST digest;
    unsigned char *salt;
    size_t salt_len;
    unsigned char *key;
    size_t key_len;
    unsigned char info[HKDF_MAXBUF];
    size_t info_len;
} KDF_HKDF;

static void *kdf_hkdf_new(void *provctx)
{
    KDF_HKDF *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL)
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
    else
        ctx->provctx = provctx;
    return ctx;
}

static void kdf_hkdf_free(void *vctx)
{
    KDF_HKDF *ctx = (KDF_HKDF *)vctx;

    if (ctx != NULL) {
        kdf_hkdf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_hkdf_reset(void *vctx)
{
    KDF_HKDF *ctx = (KDF_HKDF *)vctx;
    void *provctx = ctx->provctx;

    ossl_prov_digest_reset(&ctx->digest);
    OPENSSL_free(ctx->salt);
    OPENSSL_clear_free(ctx->key, ctx->key_len);
    OPENSSL_cleanse(ctx->info, ctx->info_len);
    memset(ctx, 0, sizeof(*ctx));
    ctx->provctx = provctx;
}

static size_t kdf_hkdf_size(KDF_HKDF *ctx)
{
    int sz;
    const EVP_MD *md = ossl_prov_digest_md(&ctx->digest);

    if (ctx->mode != EVP_KDF_HKDF_MODE_EXTRACT_ONLY)
        return SIZE_MAX;

    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    sz = EVP_MD_get_size(md);
    if (sz < 0)
        return 0;

    return sz;
}

static int kdf_hkdf_derive(void *vctx, unsigned char *key, size_t keylen,
                           const OSSL_PARAM params[])
{
    KDF_HKDF *ctx = (KDF_HKDF *)vctx;
    OSSL_LIB_CTX *libctx = PROV_LIBCTX_OF(ctx->provctx);
    const EVP_MD *md;

    if (!ossl_prov_is_running() || !kdf_hkdf_set_ctx_params(ctx, params))
        return 0;

    md = ossl_prov_digest_md(&ctx->digest);
    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    if (ctx->key == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_KEY);
        return 0;
    }
    if (keylen == 0) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
        return 0;
    }

    switch (ctx->mode) {
    case EVP_KDF_HKDF_MODE_EXTRACT_AND_EXPAND:
        return HKDF(libctx, md, ctx->salt, ctx->salt_len,
                    ctx->key, ctx->key_len, ctx->info, ctx->info_len, key, keylen);

    case EVP_KDF_HKDF_MODE_EXTRACT_ONLY:
        return HKDF_Extract(libctx, md, ctx->salt, ctx->salt_len,
                            ctx->key, ctx->key_len, key, keylen);

    case EVP_KDF_HKDF_MODE_EXPAND_ONLY:
        return HKDF_Expand(md, ctx->key, ctx->key_len, ctx->info,
                           ctx->info_len, key, keylen);

    default:
        return 0;
    }
}

static int kdf_hkdf_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    const OSSL_PARAM *p;
    KDF_HKDF *ctx = vctx;
    OSSL_LIB_CTX *libctx = PROV_LIBCTX_OF(ctx->provctx);
    int n;

    if (params == NULL)
        return 1;

    if (!ossl_prov_digest_load_from_params(&ctx->digest, params, libctx))
        return 0;

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_MODE)) != NULL) {
        if (p->data_type == OSSL_PARAM_UTF8_STRING) {
            if (strcasecmp(p->data, "EXTRACT_AND_EXPAND") == 0) {
                ctx->mode = EVP_KDF_HKDF_MODE_EXTRACT_AND_EXPAND;
            } else if (strcasecmp(p->data, "EXTRACT_ONLY") == 0) {
                ctx->mode = EVP_KDF_HKDF_MODE_EXTRACT_ONLY;
            } else if (strcasecmp(p->data, "EXPAND_ONLY") == 0) {
                ctx->mode = EVP_KDF_HKDF_MODE_EXPAND_ONLY;
            } else {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_MODE);
                return 0;
            }
        } else if (OSSL_PARAM_get_int(p, &n)) {
            if (n != EVP_KDF_HKDF_MODE_EXTRACT_AND_EXPAND
                && n != EVP_KDF_HKDF_MODE_EXTRACT_ONLY
                && n != EVP_KDF_HKDF_MODE_EXPAND_ONLY) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_MODE);
                return 0;
            }
            ctx->mode = n;
        } else {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_MODE);
            return 0;
        }
    }

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_KEY)) != NULL) {
        OPENSSL_clear_free(ctx->key, ctx->key_len);
        ctx->key = NULL;
        if (!OSSL_PARAM_get_octet_string(p, (void **)&ctx->key, 0,
                                         &ctx->key_len))
            return 0;
    }

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_SALT)) != NULL) {
        if (p->data_size != 0 && p->data != NULL) {
            OPENSSL_free(ctx->salt);
            ctx->salt = NULL;
            if (!OSSL_PARAM_get_octet_string(p, (void **)&ctx->salt, 0,
                                             &ctx->salt_len))
                return 0;
        }
    }
    /* The info fields concatenate, so process them all */
    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_INFO)) != NULL) {
        ctx->info_len = 0;
        for (; p != NULL; p = OSSL_PARAM_locate_const(p + 1,
                                                      OSSL_KDF_PARAM_INFO)) {
            const void *q = ctx->info + ctx->info_len;
            size_t sz = 0;

            if (p->data_size != 0
                && p->data != NULL
                && !OSSL_PARAM_get_octet_string(p, (void **)&q,
                                                HKDF_MAXBUF - ctx->info_len,
                                                &sz))
                return 0;
            ctx->info_len += sz;
        }
    }
    return 1;
}

static const OSSL_PARAM *kdf_hkdf_settable_ctx_params(ossl_unused void *ctx,
                                                      ossl_unused void *provctx)
{
    static const OSSL_PARAM known_settable_ctx_params[] = {
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_MODE, NULL, 0),
        OSSL_PARAM_int(OSSL_KDF_PARAM_MODE, NULL),
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_PROPERTIES, NULL, 0),
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_DIGEST, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SALT, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_KEY, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_INFO, NULL, 0),
        OSSL_PARAM_END
    };
    return known_settable_ctx_params;
}

static int kdf_hkdf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    KDF_HKDF *ctx = (KDF_HKDF *)vctx;
    OSSL_PARAM *p;

    if ((p = OSSL_PARAM_locate(params, OSSL_KDF_PARAM_SIZE)) != NULL)
        return OSSL_PARAM_set_size_t(p, kdf_hkdf_size(ctx));
    return -2;
}

static const OSSL_PARAM *kdf_hkdf_gettable_ctx_params(ossl_unused void *ctx,
                                                      ossl_unused void *provctx)
{
    static const OSSL_PARAM known_gettable_ctx_params[] = {
        OSSL_PARAM_size_t(OSSL_KDF_PARAM_SIZE, NULL),
        OSSL_PARAM_END
    };
    return known_gettable_ctx_params;
}

const OSSL_DISPATCH ossl_kdf_hkdf_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void(*)(void))kdf_hkdf_new },
    { OSSL_FUNC_KDF_FREECTX, (void(*)(void))kdf_hkdf_free },
    { OSSL_FUNC_KDF_RESET, (void(*)(void))kdf_hkdf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void(*)(void))kdf_hkdf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_hkdf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS, (void(*)(void))kdf_hkdf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_hkdf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS, (void(*)(void))kdf_hkdf_get_ctx_params },
    { 0, NULL }
};

/*
 * Refer to "HMAC-based Extract-and-Expand Key Derivation Function (HKDF)"
 * Section 2 (https://tools.ietf.org/html/rfc5869#section-2) and
 * "Cryptographic Extraction and Key Derivation: The HKDF Scheme"
 * Section 4.2 (https://eprint.iacr.org/2010/264.pdf).
 *
 * From the paper:
 *   The scheme HKDF is specified as:
 *     HKDF(XTS, SKM, CTXinfo, L) = K(1) | K(2) | ... | K(t)
 *
 *     where:
 *       SKM is source key material
 *       XTS is extractor salt (which may be null or constant)
 *       CTXinfo is context information (may be null)
 *       L is the number of key bits to be produced by KDF
 *       k is the output length in bits of the hash function used with HMAC
 *       t = ceil(L/k)
 *       the value K(t) is truncated to its first d = L mod k bits.
 *
 * From RFC 5869:
 *   2.2.  Step 1: Extract
 *     HKDF-Extract(salt, IKM) -> PRK
 *   2.3.  Step 2: Expand
 *     HKDF-Expand(PRK, info, L) -> OKM
 */
static int HKDF(OSSL_LIB_CTX *libctx, const EVP_MD *evp_md,
                const unsigned char *salt, size_t salt_len,
                const unsigned char *ikm, size_t ikm_len,
                const unsigned char *info, size_t info_len,
                unsigned char *okm, size_t okm_len)
{
    unsigned char prk[EVP_MAX_MD_SIZE];
    int ret, sz;
    size_t prk_len;

    sz = EVP_MD_get_size(evp_md);
    if (sz < 0)
        return 0;
    prk_len = (size_t)sz;

    /* Step 1: HKDF-Extract(salt, IKM) -> PRK */
    if (!HKDF_Extract(libctx, evp_md,
                      salt, salt_len, ikm, ikm_len, prk, prk_len))
        return 0;

    /* Step 2: HKDF-Expand(PRK, info, L) -> OKM */
    ret = HKDF_Expand(evp_md, prk, prk_len, info, info_len, okm, okm_len);
    OPENSSL_cleanse(prk, sizeof(prk));

    return ret;
}

/*
 * Refer to "HMAC-based Extract-and-Expand Key Derivation Function (HKDF)"
 * Section 2.2 (https://tools.ietf.org/html/rfc5869#section-2.2).
 *
 * 2.2.  Step 1: Extract
 *
 *   HKDF-Extract(salt, IKM) -> PRK
 *
 *   Options:
 *      Hash     a hash function; HashLen denotes the length of the
 *               hash function output in octets
 *
 *   Inputs:
 *      salt     optional salt value (a non-secret random value);
 *               if not provided, it is set to a string of HashLen zeros.
 *      IKM      input keying material
 *
 *   Output:
 *      PRK      a pseudorandom key (of HashLen octets)
 *
 *   The output PRK is calculated as follows:
 *
 *   PRK = HMAC-Hash(salt, IKM)
 */
static int HKDF_Extract(OSSL_LIB_CTX *libctx, const EVP_MD *evp_md,
                        const unsigned char *salt, size_t salt_len,
                        const unsigned char *ikm, size_t ikm_len,
                        unsigned char *prk, size_t prk_len)
{
    int sz = EVP_MD_get_size(evp_md);

    if (sz < 0)
        return 0;
    if (prk_len != (size_t)sz) {
        ERR_raise(ERR_LIB_PROV, PROV_R_WRONG_OUTPUT_BUFFER_SIZE);
        return 0;
    }
    /* calc: PRK = HMAC-Hash(salt, IKM) */
    return
        EVP_Q_mac(libctx, "HMAC", NULL, EVP_MD_get0_name(evp_md), NULL, salt,
                  salt_len, ikm, ikm_len, prk, EVP_MD_get_size(evp_md), NULL)
        != NULL;
}

/*
 * Refer to "HMAC-based Extract-and-Expand Key Derivation Function (HKDF)"
 * Section 2.3 (https://tools.ietf.org/html/rfc5869#section-2.3).
 *
 * 2.3.  Step 2: Expand
 *
 *   HKDF-Expand(PRK, info, L) -> OKM
 *
 *   Options:
 *      Hash     a hash function; HashLen denotes the length of the
 *               hash function output in octets
 *
 *   Inputs:
 *      PRK      a pseudorandom key of at least HashLen octets
 *               (usually, the output from the extract step)
 *      info     optional context and application specific information
 *               (can be a zero-length string)
 *      L        length of output keying material in octets
 *               (<= 255*HashLen)
 *
 *   Output:
 *      OKM      output keying material (of L octets)
 *
 *   The output OKM is calculated as follows:
 *
 *   N = ceil(L/HashLen)
 *   T = T(1) | T(2) | T(3) | ... | T(N)
 *   OKM = first L octets of T
 *
 *   where:
 *   T(0) = empty string (zero length)
 *   T(1) = HMAC-Hash(PRK, T(0) | info | 0x01)
 *   T(2) = HMAC-Hash(PRK, T(1) | info | 0x02)
 *   T(3) = HMAC-Hash(PRK, T(2) | info | 0x03)
 *   ...
 *
 *   (where the constant concatenated to the end of each T(n) is a
 *   single octet.)
 */
static int HKDF_Expand(const EVP_MD *evp_md,
                       const unsigned char *prk, size_t prk_len,
                       const unsigned char *info, size_t info_len,
                       unsigned char *okm, size_t okm_len)
{
    HMAC_CTX *hmac;
    int ret = 0, sz;
    unsigned int i;
    unsigned char prev[EVP_MAX_MD_SIZE];
    size_t done_len = 0, dig_len, n;

    sz = EVP_MD_get_size(evp_md);
    if (sz <= 0)
        return 0;
    dig_len = (size_t)sz;

    /* calc: N = ceil(L/HashLen) */
    n = okm_len / dig_len;
    if (okm_len % dig_len)
        n++;

    if (n > 255 || okm == NULL)
        return 0;

    if ((hmac = HMAC_CTX_new()) == NULL)
        return 0;

    if (!HMAC_Init_ex(hmac, prk, prk_len, evp_md, NULL))
        goto err;

    for (i = 1; i <= n; i++) {
        size_t copy_len;
        const unsigned char ctr = i;

        /* calc: T(i) = HMAC-Hash(PRK, T(i - 1) | info | i) */
        if (i > 1) {
            if (!HMAC_Init_ex(hmac, NULL, 0, NULL, NULL))
                goto err;

            if (!HMAC_Update(hmac, prev, dig_len))
                goto err;
        }

        if (!HMAC_Update(hmac, info, info_len))
            goto err;

        if (!HMAC_Update(hmac, &ctr, 1))
            goto err;

        if (!HMAC_Final(hmac, prev, NULL))
            goto err;

        copy_len = (done_len + dig_len > okm_len) ?
                       okm_len - done_len :
                       dig_len;

        memcpy(okm + done_len, prev, copy_len);

        done_len += copy_len;
    }
    ret = 1;

 err:
    OPENSSL_cleanse(prev, sizeof(prev));
    HMAC_CTX_free(hmac);
    return ret;
}
