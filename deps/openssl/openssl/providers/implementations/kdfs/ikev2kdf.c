/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/fips.h"
#include "prov/provider_ctx.h"
#include "prov/providercommon.h"
#include "prov/implementations.h"
#include "prov/provider_util.h"
#include "providers/implementations/kdfs/ikev2kdf.inc"

/* The shared secret length: 28 ~ 1024 bytes */
#define IKEV2KDF_MIN_SECRET_LENGTH 28
#define IKEV2KDF_MAX_GROUP19_MODLEN 32 /* ECDH p256 */
#define IKEV2KDF_MAX_GROUP20_MODLEN 48 /* ECDH p384 */
#define IKEV2KDF_MAX_GROUP21_MODLEN 66 /* ECDH p521 */
#define IKEV2KDF_MAX_GROUP2_MODLEN 128 /* DH group 2, no longer secure */
#define IKEV2KDF_MAX_GROUP14_MODLEN 256
#define IKEV2KDF_MAX_GROUP15_MODLEN 384
#define IKEV2KDF_MAX_GROUP16_MODLEN 512
#define IKEV2KDF_MAX_GROUP17_MODLEN 768
#define IKEV2KDF_MAX_GROUP18_MODLEN 1024
#define IKEV2KDF_MIN_NONCE_LENGTH 8
#define IKEV2KDF_MAX_NONCE_LENGTH 256
#define IKEV2KDF_MAX_DKM_LENGTH 2048

static OSSL_FUNC_kdf_newctx_fn kdf_ikev2kdf_new;
static OSSL_FUNC_kdf_dupctx_fn kdf_ikev2kdf_dup;
static OSSL_FUNC_kdf_freectx_fn kdf_ikev2kdf_free;
static OSSL_FUNC_kdf_reset_fn kdf_ikev2kdf_reset;
static OSSL_FUNC_kdf_derive_fn kdf_ikev2kdf_derive;
static OSSL_FUNC_kdf_settable_ctx_params_fn kdf_ikev2kdf_settable_ctx_params;
static OSSL_FUNC_kdf_set_ctx_params_fn kdf_ikev2kdf_set_ctx_params;
static OSSL_FUNC_kdf_gettable_ctx_params_fn kdf_ikev2kdf_gettable_ctx_params;
static OSSL_FUNC_kdf_get_ctx_params_fn kdf_ikev2kdf_get_ctx_params;

static int IKEV2_GEN(OSSL_LIB_CTX *libctx, unsigned char *seedkey, const size_t keylen,
    char *md_name, const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *shared_secret, const size_t shared_secret_len);
static int IKEV2_REKEY(OSSL_LIB_CTX *libctx, unsigned char *seedkey, const size_t keylen,
    char *md_name, const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *shared_secret, const size_t shared_secret_len,
    const unsigned char *sk_d, const size_t skd_len);
static int IKEV2_DKM(OSSL_LIB_CTX *libctx, unsigned char *dkm, const size_t len_out,
    const EVP_MD *evp_md, const unsigned char *seedkey, const size_t seedkey_len,
    const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *spii, const size_t spii_len,
    const unsigned char *spir, const size_t spir_len,
    const unsigned char *shared_secret, const size_t shared_secret_len);

typedef struct {
    OSSL_LIB_CTX *libctx;
    PROV_DIGEST digest;
    uint8_t *secret;
    size_t secret_len;
    uint8_t *seedkey;
    size_t seedkey_len;
    uint8_t *ni;
    size_t ni_len;
    uint8_t *nr;
    size_t nr_len;
    uint8_t *spii;
    size_t spii_len;
    uint8_t *spir;
    size_t spir_len;
    uint8_t *sk_d;
    size_t sk_d_len;
    int mode;
} KDF_IKEV2KDF;

static void *kdf_ikev2kdf_new(void *provctx)
{
    KDF_IKEV2KDF *ctx;

    if (!ossl_prov_is_running())
        return NULL;

#ifdef FIPS_MODULE
    if (!ossl_deferred_self_test(PROV_LIBCTX_OF(provctx),
            ST_ID_KDF_IKEV2KDF_GEN))
        return NULL;
#endif

    if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) != NULL)
        ctx->libctx = PROV_LIBCTX_OF(provctx);
    return ctx;
}

static void *kdf_ikev2kdf_dup(void *vctx)
{
    KDF_IKEV2KDF *src = (KDF_IKEV2KDF *)vctx;
    KDF_IKEV2KDF *dest = NULL;

    dest = OPENSSL_zalloc(sizeof(*src));
    if (dest != NULL) {
        dest->libctx = src->libctx;
        if ((src->secret != NULL)
            && (!ossl_prov_memdup(src->secret, src->secret_len,
                &dest->secret, &dest->secret_len)))
            goto err;

        if ((src->seedkey != NULL)
            && (!ossl_prov_memdup(src->seedkey, src->seedkey_len,
                &dest->seedkey, &dest->seedkey_len)))
            goto err;

        if ((src->ni != NULL)
            && (!ossl_prov_memdup(src->ni, src->ni_len, &dest->ni, &dest->ni_len)))
            goto err;

        if ((src->nr != NULL)
            && (!ossl_prov_memdup(src->nr, src->nr_len, &dest->nr, &dest->nr_len)))
            goto err;
        if ((src->spii != NULL)
            && (!ossl_prov_memdup(src->spii, src->spii_len, &dest->spii, &dest->spii_len)))
            goto err;
        if ((src->spir != NULL)
            && (!ossl_prov_memdup(src->spir, src->spir_len, &dest->spir, &dest->spir_len)))
            goto err;

        if ((src->sk_d != NULL)
            && (!ossl_prov_memdup(src->sk_d, src->sk_d_len,
                &dest->sk_d, &dest->sk_d_len)))
            goto err;

        if (!ossl_prov_digest_copy(&dest->digest, &src->digest))
            goto err;
        dest->mode = src->mode;
    }
    return dest;

err:
    kdf_ikev2kdf_free(dest);
    return NULL;
}
static void kdf_ikev2kdf_free(void *vctx)
{
    KDF_IKEV2KDF *ctx = (KDF_IKEV2KDF *)vctx;

    if (ctx != NULL) {
        kdf_ikev2kdf_reset(ctx);
        OPENSSL_free(ctx);
    }
}

static void kdf_ikev2kdf_reset(void *vctx)
{
    KDF_IKEV2KDF *ctx = (KDF_IKEV2KDF *)vctx;
    OSSL_LIB_CTX *libctx = ctx->libctx;

    ossl_prov_digest_reset(&ctx->digest);
    OPENSSL_clear_free(ctx->secret, ctx->secret_len);
    OPENSSL_clear_free(ctx->seedkey, ctx->seedkey_len);
    OPENSSL_clear_free(ctx->sk_d, ctx->sk_d_len);
    OPENSSL_clear_free(ctx->ni, ctx->ni_len);
    OPENSSL_clear_free(ctx->nr, ctx->nr_len);
    OPENSSL_clear_free(ctx->spii, ctx->spii_len);
    OPENSSL_clear_free(ctx->spir, ctx->spir_len);
    memset(ctx, 0, sizeof(*ctx));
    ctx->libctx = libctx;
}

static int ikev2kdf_set_membuf(unsigned char **dst, size_t *dst_len,
    const OSSL_PARAM *p)
{
    OPENSSL_clear_free(*dst, *dst_len);
    *dst = NULL;
    *dst_len = 0;
    return OSSL_PARAM_get_octet_string(p, (void **)dst, 0, dst_len);
}

static int ikev2_common_check_ctx_params(KDF_IKEV2KDF *ctx)
{
    if (ctx->ni == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_NONCE);
        return 0;
    }

    if (ctx->nr == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_NONCE);
        return 0;
    }
    return 1;
}

/*
 * RFC 7296: section 2.14
 * g^ir is represented as a string of octets in big endian order padded
 * with zeros if necessary to make it the length of the modulus.
 * The secret length is in range of 28 ~ 1024 bytes, and the padding length
 * is determined by the secret length:
 * ecdh group 19 32 bytes
 * ecdh group 20 48 bytes
 * ecdh group 21 66 bytes
 * dh group 2 128 bytes (no longer secure, but still supported for interoperability)
 * dh group 14 256 bytes
 * dh group 15 384 bytes
 * dh group 16 512 bytes
 * dh group 17 768 bytes
 * dh group 18 1024 bytes
 * secret is required for GEN, REKEY and DKM(Child_DH).
 */
static int ikev2_check_secret_and_pad(KDF_IKEV2KDF *ctx)
{
    size_t pad_len = 0;
    uint8_t *new_secret = NULL;

    if (ctx->secret_len == 0)
        return 1;
    if ((ctx->secret_len < IKEV2KDF_MIN_SECRET_LENGTH)
        || (ctx->secret_len > IKEV2KDF_MAX_GROUP18_MODLEN)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_SECRET_LENGTH);
        return 0;
    }
    if ((ctx->secret_len == IKEV2KDF_MAX_GROUP19_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP20_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP21_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP2_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP14_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP15_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP16_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP17_MODLEN)
        || (ctx->secret_len == IKEV2KDF_MAX_GROUP18_MODLEN))
        /* no padding needed if secret_len is already valid */
        return 1;

    if (ctx->secret_len < IKEV2KDF_MAX_GROUP19_MODLEN)
        pad_len = IKEV2KDF_MAX_GROUP19_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP19_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP20_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP20_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP20_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP21_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP21_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP21_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP2_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP2_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP2_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP14_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP14_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP14_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP15_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP15_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP15_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP16_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP16_MODLEN - ctx->secret_len;
    else if ((ctx->secret_len > IKEV2KDF_MAX_GROUP16_MODLEN)
        && (ctx->secret_len < IKEV2KDF_MAX_GROUP17_MODLEN))
        pad_len = IKEV2KDF_MAX_GROUP17_MODLEN - ctx->secret_len;
    else if (ctx->secret_len > IKEV2KDF_MAX_GROUP17_MODLEN)
        pad_len = IKEV2KDF_MAX_GROUP18_MODLEN - ctx->secret_len;

    new_secret = OPENSSL_zalloc(ctx->secret_len + pad_len);
    if (new_secret == NULL) {
        ERR_raise(ERR_LIB_PROV, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    memcpy(new_secret + pad_len, ctx->secret, ctx->secret_len);
    OPENSSL_clear_free(ctx->secret, ctx->secret_len);
    ctx->secret = new_secret;
    ctx->secret_len += pad_len;
    return 1;
}

static int kdf_ikev2kdf_derive(void *vctx, unsigned char *key, size_t keylen,
    const OSSL_PARAM params[])
{
    KDF_IKEV2KDF *ctx = (KDF_IKEV2KDF *)vctx;
    const EVP_MD *md;
    size_t md_size;
    int value = 0;

    if (!ossl_prov_is_running() || !kdf_ikev2kdf_set_ctx_params(ctx, params))
        return 0;

    md = ossl_prov_digest_md(&ctx->digest);
    if (md == NULL) {
        ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
        return 0;
    }
    value = EVP_MD_size(md);
    if (value <= 0)
        return 0;
    md_size = (size_t)value;

    if (!ikev2_common_check_ctx_params(ctx))
        return 0;

    switch (ctx->mode) {
    case EVP_KDF_IKEV2_MODE_GEN:
        if ((ctx->secret == NULL) || (ctx->secret_len == 0)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SECRET);
            return 0;
        }
        if (keylen != md_size) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
            return 0;
        }
        if (!ikev2_check_secret_and_pad(ctx))
            return 0;
        return (IKEV2_GEN(ctx->libctx, key, keylen, (char *)EVP_MD_name(md),
            ctx->ni, ctx->ni_len, ctx->nr, ctx->nr_len,
            ctx->secret, ctx->secret_len));

    case EVP_KDF_IKEV2_MODE_DKM:
        /*
         * if spi_init != NULL and spi_resp != NULL and shared_secret = NULL
         *    and seedkey != NULL
         *    calculate DKM
         * else if spi_init == NULL and spi_resp == NULL and shared_secret != NULL
         *    and sk_d != NULL
         *    calculate DKM(Child_DH)
         * else if spi_init == NULL and spi_resp == NULL and shared_secret == NULL
         *    and sk_d != NULL
         *    calculate DKM(Child_SA)
         * endif
         */
        if ((ctx->spii != NULL) && (ctx->spii_len != 0)
            && (ctx->spir != NULL) && (ctx->spir_len != 0)
            && (ctx->secret == NULL) && (ctx->secret_len == 0)) {
            if ((ctx->seedkey == NULL) || (ctx->seedkey_len == 0)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_KEY);
                return 0;
            }
            if (ctx->seedkey_len != md_size) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
                return 0;
            }
            if ((keylen < md_size) || (keylen > IKEV2KDF_MAX_DKM_LENGTH)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
                return 0;
            }
            /* calculate DKM */
            return (IKEV2_DKM(ctx->libctx, key, keylen, md, ctx->seedkey, ctx->seedkey_len,
                ctx->ni, ctx->ni_len, ctx->nr, ctx->nr_len,
                ctx->spii, ctx->spii_len, ctx->spir, ctx->spir_len,
                NULL, 0));
        } else if ((ctx->spii == NULL) && (ctx->spir == NULL)
            && (ctx->spii_len == 0) && (ctx->spir_len == 0)) {
            if ((ctx->sk_d == NULL) || (ctx->sk_d_len == 0)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_DKM);
                return 0;
            }
            if ((keylen < md_size) || (keylen > IKEV2KDF_MAX_DKM_LENGTH)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
                return 0;
            }
            /* If Child_DH is intended, require secret_len > 0 */
            if ((ctx->secret != NULL && ctx->secret_len == 0)
                || (ctx->secret == NULL && ctx->secret_len != 0)) {
                ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SECRET);
                return 0;
            }
            if (!ikev2_check_secret_and_pad(ctx))
                return 0;
            /* calculate DKM(Child_SA) or DKM(Child_DH) */
            return (IKEV2_DKM(ctx->libctx, key, keylen, md, ctx->sk_d, ctx->sk_d_len,
                ctx->ni, ctx->ni_len, ctx->nr, ctx->nr_len,
                NULL, 0, NULL, 0,
                ctx->secret, ctx->secret_len));
        } else {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_PARAMETERS_FOR_DKM);
            return 0;
        }
    case EVP_KDF_IKEV2_MODE_REKEY:
        if ((ctx->secret == NULL) || (ctx->secret_len == 0)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_SECRET);
            return 0;
        }
        if ((ctx->sk_d == NULL) || (ctx->sk_d_len == 0)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_DKM);
            return 0;
        }
        if (ctx->sk_d_len != md_size) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
            return 0;
        }
        if (keylen != md_size) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_KEY_LENGTH);
            return 0;
        }
        if (!ikev2_check_secret_and_pad(ctx))
            return 0;
        return (IKEV2_REKEY(ctx->libctx, key, keylen, (char *)EVP_MD_name(md),
            ctx->ni, ctx->ni_len, ctx->nr, ctx->nr_len,
            ctx->secret, ctx->secret_len, ctx->sk_d, ctx->sk_d_len));
    default:
        /* This error is already checked in set_ctx_params */
        ;
    }
    return 0;
}

static int kdf_ikev2kdf_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    struct ikev2_set_ctx_params_st p;
    KDF_IKEV2KDF *ctx = vctx;
    const EVP_MD *md;

    if (params == NULL)
        return 1;

    if (ctx == NULL || !ikev2_set_ctx_params_decoder(params, &p))
        return 0;

    if (p.digest != NULL) {
        if (!ossl_prov_digest_load(&ctx->digest, p.digest, p.propq, ctx->libctx))
            return 0;
        md = ossl_prov_digest_md(&ctx->digest);
        if (md == NULL) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
            return 0;
        }

        if (!EVP_MD_is_a(md, SN_sha1)
            && !EVP_MD_is_a(md, SN_sha224)
            && !EVP_MD_is_a(md, SN_sha256)
            && !EVP_MD_is_a(md, SN_sha384)
            && !EVP_MD_is_a(md, SN_sha512))
            return 0;
    }
    if (p.ni != NULL) {
        if (!ikev2kdf_set_membuf(&ctx->ni, &ctx->ni_len, p.ni))
            return 0;
        if ((ctx->ni_len < IKEV2KDF_MIN_NONCE_LENGTH)
            || (ctx->ni_len > IKEV2KDF_MAX_NONCE_LENGTH)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_NONCE_LENGTH);
            return 0;
        }
    }
    if (p.nr != NULL) {
        if (!ikev2kdf_set_membuf(&ctx->nr, &ctx->nr_len, p.nr))
            return 0;
        if ((ctx->nr_len < IKEV2KDF_MIN_NONCE_LENGTH)
            || (ctx->nr_len > IKEV2KDF_MAX_NONCE_LENGTH)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_NONCE_LENGTH);
            return 0;
        }
    }
    if (p.spii != NULL)
        if (!ikev2kdf_set_membuf(&ctx->spii, &ctx->spii_len, p.spii))
            return 0;
    if (p.spir != NULL)
        if (!ikev2kdf_set_membuf(&ctx->spir, &ctx->spir_len, p.spir))
            return 0;
    if (p.secret != NULL)
        if (!ikev2kdf_set_membuf(&ctx->secret, &ctx->secret_len, p.secret))
            return 0;
    if (p.seedkey != NULL)
        if (!ikev2kdf_set_membuf(&ctx->seedkey, &ctx->seedkey_len, p.seedkey))
            return 0;
    if (p.sk_d != NULL)
        if (!ikev2kdf_set_membuf(&ctx->sk_d, &ctx->sk_d_len, p.sk_d))
            return 0;
    if (p.mode != NULL) {
        if (!OSSL_PARAM_get_int(p.mode, &ctx->mode))
            return 0;
        if ((ctx->mode != EVP_KDF_IKEV2_MODE_GEN)
            && (ctx->mode != EVP_KDF_IKEV2_MODE_DKM)
            && (ctx->mode != EVP_KDF_IKEV2_MODE_REKEY)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_MODE);
            return 0;
        }
    }
    return 1;
}

static const OSSL_PARAM *kdf_ikev2kdf_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return ikev2_set_ctx_params_list;
}

static int kdf_ikev2kdf_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    struct ikev2_get_ctx_params_st p;
    KDF_IKEV2KDF *ctx = vctx;

    if (ctx == NULL || !ikev2_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.size != NULL) {
        int sz = 0;
        const EVP_MD *md = NULL;

        md = ossl_prov_digest_md(&ctx->digest);
        if (md == NULL) {
            ERR_raise(ERR_LIB_PROV, PROV_R_MISSING_MESSAGE_DIGEST);
            return 0;
        }
        sz = EVP_MD_size(md);
        if (sz <= 0)
            return 0;
        if (!OSSL_PARAM_set_size_t(p.size, (size_t)sz))
            return 0;
    }
    return 1;
}

static const OSSL_PARAM *kdf_ikev2kdf_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *p_ctx)
{
    return ikev2_get_ctx_params_list;
}

const OSSL_DISPATCH ossl_kdf_ikev2kdf_functions[] = {
    { OSSL_FUNC_KDF_NEWCTX, (void (*)(void))kdf_ikev2kdf_new },
    { OSSL_FUNC_KDF_DUPCTX, (void (*)(void))kdf_ikev2kdf_dup },
    { OSSL_FUNC_KDF_FREECTX, (void (*)(void))kdf_ikev2kdf_free },
    { OSSL_FUNC_KDF_RESET, (void (*)(void))kdf_ikev2kdf_reset },
    { OSSL_FUNC_KDF_DERIVE, (void (*)(void))kdf_ikev2kdf_derive },
    { OSSL_FUNC_KDF_SETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_ikev2kdf_settable_ctx_params },
    { OSSL_FUNC_KDF_SET_CTX_PARAMS, (void (*)(void))kdf_ikev2kdf_set_ctx_params },
    { OSSL_FUNC_KDF_GETTABLE_CTX_PARAMS,
        (void (*)(void))kdf_ikev2kdf_gettable_ctx_params },
    { OSSL_FUNC_KDF_GET_CTX_PARAMS, (void (*)(void))kdf_ikev2kdf_get_ctx_params },
    { 0, NULL }
};

/*
 * IKEV2_GEN - KDF in compliance with SP800-135 for IKEv2,
 *             generate the seedkey.
 *
 * algorithm: HMAC(ni || nr, shared_secret)
 *
 * Inputs:
 *   libctx - provider LIB context
 *   seedkey - pointer to output for seedkey
 *   keylen - length of seedkey(in bytes)
 *   md_name - name of the SHA digest
 *   ni - pointer to initiator nonce input
 *   ni_len - initiator nonce length(in bytes)
 *   nr - pointer to responder nonce input
 *   nr_len - nonce length(in bytes)
 *   shared_secret - pointer to secret input
 *   shared_secret_len - secret length(in bytes)
 * Outputs:
 *   return - 1 pass, 0 fail
 *   seedkey - output seedkey when passing.
 */
static int IKEV2_GEN(OSSL_LIB_CTX *libctx, unsigned char *seedkey, const size_t keylen,
    char *md_name, const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *shared_secret, const size_t shared_secret_len)
{
    EVP_MAC_CTX *ctx = NULL;
    EVP_MAC *mac = NULL;
    size_t outl = 0;
    int ret = 0;
    unsigned char *nonce = NULL;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", md_name, 0),
        OSSL_PARAM_construct_end()
    };

    nonce = OPENSSL_malloc(ni_len + nr_len);
    if (nonce == NULL)
        return ret;
    memcpy(nonce, ni, ni_len);
    memcpy(nonce + ni_len, nr, nr_len);

    mac = EVP_MAC_fetch(libctx, (char *)"HMAC", NULL);
    if ((mac == NULL)
        || ((ctx = EVP_MAC_CTX_new(mac)) == NULL)
        || (!EVP_MAC_init(ctx, nonce, ni_len + nr_len, params))
        || (!EVP_MAC_update(ctx, shared_secret, shared_secret_len))
        || (!EVP_MAC_final(ctx, seedkey, &outl, keylen))
        || (outl != keylen))
        goto err;

    ret = 1;
err:
    OPENSSL_clear_free(nonce, ni_len + nr_len);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ret;
}

/*
 * IKEV2_REKEY - KDF in compliance with SP800-135 for IKEv2,
 *               re-generate the seedkey.
 *
 * algorithm:  HMAC(sk_d, new secret || Ni || Nr )
 *
 * Inputs:
 *   libctx - provider LIB context
 *   seedkey - pointer to output for seedkey
 *   keylen - length of seedkey(in bytes)
 *   md_name - name of the SHA digest
 *   ni - pointer to initiator nonce input
 *   ni_len - initiator nonce length(in bytes)
 *   nr - pointer to responder nonce input
 *   nr_len - responder nonce length(in bytes)
 *   shared_secret - (new) pointer to secret input
 *   shared_secret_len - (new)secret length(in bytes)
 *   sk_d - pointer to sk_d portion of DKM
 *   skd_len - length of sk_d (in bytes)
 * Outputs:
 *   return = 1 pass, 0 fail
 *   seedkey - output seedkey when passing.
 */
static int IKEV2_REKEY(OSSL_LIB_CTX *libctx, unsigned char *seedkey, const size_t keylen,
    char *md_name, const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *shared_secret, const size_t shared_secret_len,
    const unsigned char *sk_d, const size_t sk_d_len)
{
    EVP_MAC_CTX *ctx = NULL;
    EVP_MAC *mac = NULL;
    size_t outl = 0;
    int ret = 0;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", md_name, 0),
        OSSL_PARAM_construct_end()
    };

    mac = EVP_MAC_fetch(libctx, "HMAC", NULL);
    if ((mac == NULL)
        || ((ctx = EVP_MAC_CTX_new(mac)) == NULL)
        || (!EVP_MAC_init(ctx, sk_d, sk_d_len, params))
        || (!EVP_MAC_update(ctx, shared_secret, shared_secret_len))
        || (!EVP_MAC_update(ctx, ni, ni_len))
        || (!EVP_MAC_update(ctx, nr, nr_len))
        || (!EVP_MAC_final(ctx, seedkey, &outl, keylen))
        || (outl != keylen))
        goto err;

    ret = 1;

err:
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ret;
}

/*
 * IKEV2_DKM - KDF in compliance with SP800-135 for IKEv2,
 *             generate the Derived Keying Material(DKM),
 *             DKM(Child SA) and DKM(Child SA DH).
 * algorithm:
 *   if spii != NULL and spir != NULL and shared_secret == NULL
 *     and seedkey != NULL
 *     calculate DKM:
 *       HMAC(seedkey, ni || nr || spii || spir)
 *   else if spii == NULL and spir == NULL and shared_secret == NULL
 *     calculate DKM(Child_SA):
 *       HMAC(sk_d, ni || nr)
 *   else if spii == NULL and spir == NULL and shared_secret != NULL
 *     calculate DKM(Child_DH):
 *       HMAC(sk_d, ni || nr || new_shared_secret)
 *   endif
 *
 * Inputs:
 *   libctx - provider LIB context
 *   dkm - pointer to output dkm
 *   len_out - output length(in bytes)
 *   evp_md - pointer to SHA digest
 *   seekkey - pointer to seedkey (seekkey for DKM, sk_d for Child_SA/DH)
 *   seedkey_len - length of seedkey(in bytes)
 *   ni - pointer to initiator nonce
 *   ni_len - initiator nonce length(in bytes)
 *   nr - pointer to responder nonce
 *   nr_len - responder nonce length(in bytes)
 *   shared_secret - pointer to secret input
 *   shared_secret_len - secret length(in bytes)
 * Outputs:
 *   return - 1 pass, 0 fail
 *   dkm - output dkm when passing.
 */
static int IKEV2_DKM(OSSL_LIB_CTX *libctx, unsigned char *dkm, const size_t len_out,
    const EVP_MD *evp_md,
    const unsigned char *seedkey, const size_t seedkey_len,
    const unsigned char *ni, const size_t ni_len,
    const unsigned char *nr, const size_t nr_len,
    const unsigned char *spii, const size_t spii_len,
    const unsigned char *spir, const size_t spir_len,
    const unsigned char *shared_secret, const size_t shared_secret_len)
{
    EVP_MAC_CTX *ctx = NULL;
    EVP_MAC *mac = NULL;
    size_t outl = 0, hmac_len = 0, ii;
    unsigned char *hmac = NULL;
    int ret = 0;
    size_t md_size = 0;
    int value = 0;
    unsigned char counter = 1;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("digest", (char *)EVP_MD_name(evp_md), 0),
        OSSL_PARAM_construct_end()
    };

    value = EVP_MD_size(evp_md);
    if (value <= 0)
        return 0;
    md_size = (size_t)value;
    /* len_out may not fit the last hmac, round up */
    hmac_len = ((len_out + md_size - 1) / md_size) * md_size;
    hmac = OPENSSL_malloc(hmac_len);
    if (hmac == NULL)
        return 0;

    mac = EVP_MAC_fetch(libctx, "HMAC", NULL);
    if ((mac == NULL)
        || ((ctx = EVP_MAC_CTX_new(mac)) == NULL))
        goto err;

    /*
     * len_out <= IKEV2_MAX_DKM_LEN
     * loop count will fit in 1 byte value
     */
    for (ii = 0; ii < len_out; ii += md_size) {
        if (!EVP_MAC_init(ctx, seedkey, seedkey_len, params))
            goto err;
        if (ii != 0)
            if (!EVP_MAC_update(ctx, &hmac[ii - md_size], md_size))
                goto err;
        if (shared_secret != NULL)
            if (!EVP_MAC_update(ctx, shared_secret, shared_secret_len))
                goto err;
        if (!EVP_MAC_update(ctx, ni, ni_len)
            || !EVP_MAC_update(ctx, nr, nr_len))
            goto err;
        if (spii != NULL)
            if (!EVP_MAC_update(ctx, spii, spii_len)
                || !EVP_MAC_update(ctx, spir, spir_len))
                goto err;
        if (!EVP_MAC_update(ctx, &counter, 1))
            goto err;
        if (!EVP_MAC_final(ctx, &hmac[ii], &outl, md_size))
            goto err;
        counter++;
    }

    memcpy(dkm, hmac, len_out);
    ret = 1;
err:
    OPENSSL_clear_free(hmac, hmac_len);
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return ret;
}
