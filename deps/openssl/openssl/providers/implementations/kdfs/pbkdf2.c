/*
 * Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
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
#include "pbkdf2.h"

/* Constants specified in SP800-132 */
#define KDF_PBKDF2_MIN_KEY_LEN_BITS  112
#define KDF_PBKDF2_MAX_KEY_LEN_DIGEST_RATIO 0xFFFFFFFF
#define KDF_PBKDF2_MIN_ITERATIONS 1000
#define KDF_PBKDF2_MIN_SALT_LEN   (128 / 8)

static OSSL_FUNC_kdf_newctx_fn kdf_pbkdf2_new;
static OSSL_FUNC_kdf_freectx_fn kdf_pbkdf2_free;
static OSSL_FUNC_kdf_reset_fn kdf_pbkdf2_reset;
static OSSL_FUNC_kdf_derive_fn kdf_pbkdf2_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_pbkdf2_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_pbkdf2_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_pbkdf2_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_pbkdf2_get_ctx_params;

static int  pbkdf2_derive(const char *pass, size_t passlen,
                          const unsigned char *salt, int saltlen, uint64_t iter,
                          const EVP_MD *digest, unsigned char *key,
                          size_t keylen, int extra_checks);

typedef struct {
    void *provctx;
    unsigned char *pass;
    size_t pass_len;
    unsigned char *salt;
    size_t salt_len;
    uint64_t iter;
    PROV_DIGEST digest;
    int lower_bound_checks;
} KDF_PBKDF2;

static void kdf_pbkdf2_init(KDF_PBKDF2 *ctx);

static void *kdf_pbkdf2_new(void *provctx)
{
    KDF_PBKDF2 *ctx;

    if (!ossl_prov_is_running())
        return NULL;

    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    ctx->provctx = provctx;
    kdf_pbkdf2_init(ctx);
    return ctx;
}

static void kdf_pbkdf2_cleanup(KDF_PBKDF2 *ctx)
{
    ossl_prov_digest_reset(&ctx->digest);
    OPENSSL_free(ctx->salt);
    OPENSSL_clear_free(ctx->pass, ctx->pass_len);
    memset(ctx, 0, sizeof(*ctx));
}

static void kdf_pbkdf2_free(void *vctx)
{
    KDF_PBKDF2 *ctx = (KDF_PBKDF2 *)vctx;

    if (ctx != NULL) {
        kdf_pbkdf2_cleanup(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_pbkdf2_reset(void *vctx)
{
    KDF_PBKDF2 *ctx = (KDF_PBKDF2 *)vctx;
    void *provctx = ctx->provctx;

    kdf_pbkdf2_cleanup(ctx);
    ctx->provctx = provctx;
    kdf_pbkdf2_init(ctx);
}

static void kdf_pbkdf2_init(KDF_PBKDF2 *ctx)
{
    OSSL_PARAM params[2] = { OSSL_PARAM_END, OSSL_PARAM_END };
    OSSL_LIB_CTX *provctx = PROV_LIBCTX_OF(ctx->provctx);

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 SN_sha1, 0);
    if (!ossl_prov_digest_load_from_params(&ctx->digest, params, provctx))
        /* This is an error, but there is no way to indicate such directly */
        ossl_prov_digest_reset(&ctx->digest);
    ctx->iter = PKCS5_DEFAULT_ITER;
    ctx->lower_bound_checks = ossl_kdf_pbkdf2_default_checks;
}

static int pbkdf2_set_membuf(unsigned char **buffer, size_t *buflen,
                             const OSSL_PARAM *p)
{
    OPENSSL_clear_free(*buffer, *buflen);
    *buffer = NULL;
    *buflen = 0;

    if (p->data_size == 0) {
        if ((*buffer = OPENSSL_malloc(1)) == NULL) {
            ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
            return 0;
        }
    } else if (p->data != NULL) {
        if (!OSSL_PARAM_get_octet_string(p, (void **)buffer, 0, buflen))
            return 0;
    }
    return 1;
}

static int kdf_pbkdf2_derive(void *vctx, unsigned char *key, size_t keylen,
                             const OSSL_PARAM params[])
{
    KDF_PBKDF2 *ctx = (KDF_PBKDF2 *)vctx;
    const EVP_MD *md;

    if (!ossl_prov_is_running() || !kdf_pbkdf2_set_ctx_params(ctx, params))
        return 0;

    if (ctx->pass == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_PASS);
        return 0;
    }

    if (ctx->salt == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SALT);
        return 0;
    }

    md = ossl_prov_digest_md(&ctx->digest);
    return pbkdf2_derive((char *)ctx->pass, ctx->pass_len,
                         ctx->salt, ctx->salt_len, ctx->iter,
                         md, key, keylen, ctx->lower_bound_checks);
}

static int kdf_pbkdf2_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    const OSSL_PARAM *p;
    KDF_PBKDF2 *ctx = vctx;
    OSSL_LIB_CTX *provctx = PROV_LIBCTX_OF(ctx->provctx);
    int pkcs5;
    uint64_t iter, min_iter;

    if (params == NULL)
        return 1;

    if (!ossl_prov_digest_load_from_params(&ctx->digest, params, provctx))
        return 0;

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_PKCS5)) != NULL) {
        if (!OSSL_PARAM_get_int(p, &pkcs5))
            return 0;
        ctx->lower_bound_checks = pkcs5 == 0;
    }

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_PASSWORD)) != NULL)
        if (!pbkdf2_set_membuf(&ctx->pass, &ctx->pass_len, p))
            return 0;

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_SALT)) != NULL) {
        if (ctx->lower_bound_checks != 0
            && p->data_size < KDF_PBKDF2_MIN_SALT_LEN) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_SALT_LENGTH);
            return 0;
        }
        if (!pbkdf2_set_membuf(&ctx->salt, &ctx->salt_len,p))
            return 0;
    }

    if ((p = OSSL_PARAM_locate_const(params, OSSL_KDF_PARAM_ITER)) != NULL) {
        if (!OSSL_PARAM_get_uint64(p, &iter))
            return 0;
        min_iter = ctx->lower_bound_checks != 0 ? KDF_PBKDF2_MIN_ITERATIONS : 1;
        if (iter < min_iter) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_ITERATION_COUNT);
            return 0;
        }
        ctx->iter = iter;
    }
    return 1;
}

static const OSSL_PARAM *kdf_pbkdf2_settable_ctx_params(ossl_unused void *ctx,
                                                        ossl_unused void *p_ctx)
{
    static const OSSL_PARAM known_settable_ctx_params[] = {
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_PROPERTIES, NULL, 0),
        OSSL_PARAM_utf8_string(OSSL_KDF_PARAM_DIGEST, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_PASSWORD, NULL, 0),
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SALT, NULL, 0),
        OSSL_PARAM_uint64(OSSL_KDF_PARAM_ITER, NULL),
        OSSL_PARAM_int(OSSL_KDF_PARAM_PKCS5, NULL),
        OSSL_PARAM_END
    };
    return known_settable_ctx_params;
}

static int kdf_pbkdf2_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    OSSL_PARAM *p;

    if ((p = OSSL_PARAM_locate(params, OSSL_KDF_PARAM_SIZE)) != NULL)
        return OSSL_PARAM_set_size_t(p, SIZE_MAX);
    return -2;
}

static const OSSL_PARAM *kdf_pbkdf2_gettable_ctx_params(ossl_unused void *ctx,
                                                        ossl_unused void *p_ctx)
{
    static const OSSL_PARAM known_gettable_ctx_params[] = {
        OSSL_PARAM_size_t(OSSL_KDF_PARAM_SIZE, NULL),
        OSSL_PARAM_END
    };
    return known_gettable_ctx_params;
}

const OSSL_DISPATCH ossl_kdf_pbkdf2_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void(*)(void))kdf_pbkdf2_new },
    { OSSL_FUNC_KDF_FREECTX, (void(*)(void))kdf_pbkdf2_free },
    { OSSL_FUNC_KDF_RESET, (void(*)(void))kdf_pbkdf2_reset },
    { OSSL_FUNC_KDF_DERIVE, (void(*)(void))kdf_pbkdf2_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_pbkdf2_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS, (void(*)(void))kdf_pbkdf2_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
      (void(*)(void))kdf_pbkdf2_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS, (void(*)(void))kdf_pbkdf2_get_ctx_params },
    { 0, NULL }
};

/*
 * This is an implementation of PKCS#5 v2.0 password based encryption key
 * derivation function PBKDF2. SHA1 version verified against test vectors
 * posted by Peter Gutmann to the PKCS-TNG mailing list.
 *
 * The constraints specified by SP800-132 have been added i.e.
 *  - Check the range of the key length.
 *  - Minimum iteration count of 1000.
 *  - Randomly-generated portion of the salt shall be at least 128 bits.
 */
static int pbkdf2_derive(const char *pass, size_t passlen,
                         const unsigned char *salt, int saltlen, uint64_t iter,
                         const EVP_MD *digest, unsigned char *key,
                         size_t keylen, int lower_bound_checks)
{
    int ret = 0;
    unsigned char digtmp[EVP_MAX_MD_SIZE], *p, itmp[4];
    int cplen, k, tkeylen, mdlen;
    uint64_t j;
    unsigned long i = 1;
    HMAC_CTX *hctx_tpl = NULL, *hctx = NULL;

    mdlen = EVP_MD_get_size(digest);
    if (mdlen <= 0)
        return 0;

    /*
     * This check should always be done because keylen / mdlen >= (2^32 - 1)
     * results in an overflow of the loop counter 'i'.
     */
    if ((keylen / mdlen) >= KDF_PBKDF2_MAX_KEY_LEN_DIGEST_RATIO) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
        return 0;
    }

    if (lower_bound_checks) {
        if ((keylen * 8) < KDF_PBKDF2_MIN_KEY_LEN_BITS) {
            ERR_raise(ERR_LIB_PROV, PROV_R_KEY_SIZE_TOO_SMALL);
            return 0;
        }
        if (saltlen < KDF_PBKDF2_MIN_SALT_LEN) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_SALT_LENGTH);
            return 0;
        }
        if (iter < KDF_PBKDF2_MIN_ITERATIONS) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_ITERATION_COUNT);
            return 0;
        }
    }

    hctx_tpl = HMAC_CTX_new();
    if (hctx_tpl == NULL)
        return 0;
    p = key;
    tkeylen = keylen;
    if (!HMAC_Init_ex(hctx_tpl, pass, passlen, digest, NULL))
        goto err;
    hctx = HMAC_CTX_new();
    if (hctx == NULL)
        goto err;
    while (tkeylen) {
        if (tkeylen > mdlen)
            cplen = mdlen;
        else
            cplen = tkeylen;
        /*
         * We are unlikely to ever use more than 256 blocks (5120 bits!) but
         * just in case...
         */
        itmp[0] = (unsigned char)((i >> 24) & 0xff);
        itmp[1] = (unsigned char)((i >> 16) & 0xff);
        itmp[2] = (unsigned char)((i >> 8) & 0xff);
        itmp[3] = (unsigned char)(i & 0xff);
        if (!HMAC_CTX_copy(hctx, hctx_tpl))
            goto err;
        if (!HMAC_Update(hctx, salt, saltlen)
                || !HMAC_Update(hctx, itmp, 4)
                || !HMAC_Final(hctx, digtmp, NULL))
            goto err;
        memcpy(p, digtmp, cplen);
        for (j = 1; j < iter; j++) {
            if (!HMAC_CTX_copy(hctx, hctx_tpl))
                goto err;
            if (!HMAC_Update(hctx, digtmp, mdlen)
                    || !HMAC_Final(hctx, digtmp, NULL))
                goto err;
            for (k = 0; k < cplen; k++)
                p[k] ^= digtmp[k];
        }
        tkeylen -= cplen;
        i++;
        p += cplen;
    }
    ret = 1;

err:
    HMAC_CTX_free(hctx);
    HMAC_CTX_free(hctx_tpl);
    return ret;
}
