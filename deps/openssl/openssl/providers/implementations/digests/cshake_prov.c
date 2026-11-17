/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* including crypto/sha.h requires this for SHA256_CTX */
#include "internal/deprecated.h"
/*
 * NOTE: By default CSHAKE sets secure xof lengths (OSSL_DIGEST_PARAM_XOFLEN)
 * that are used by EVP_DigestFinal_ex(). This differs from SHAKE where the
 * xof length MUST be set (since the initial implementation shipped with BAD
 * defaults - and the only safe way to fix it was to make the user set the value)
 */
#include <string.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include "crypto/sha.h"
#include "prov/provider_ctx.h"
#include "prov/digestcommon.h"
#include "prov/implementations.h"
#include "internal/common.h"
#include "internal/sha3.h"
#include "providers/implementations/digests/cshake_prov.inc"

/*
 * Length encoding will be a 1 byte size + length in bits (3 bytes max)
 * This gives a range of 0..0XFFFFFF bits = 2097151 bytes).
 */
#define CSHAKE_MAX_ENCODED_HEADER_LEN (1 + 3)

/*
 * Restrict the maximum length of the custom strings N & S.
 * This must not exceed 64 bits = 8k bytes.
 */
#define CSHAKE_MAX_STRING 512

/* Maximum size of both the encoded strings (N and S) */
#define CSHAKE_MAX_ENCODED_STRING (CSHAKE_MAX_STRING + CSHAKE_MAX_ENCODED_HEADER_LEN)
#define CSHAKE_FLAGS (PROV_DIGEST_FLAG_XOF | PROV_DIGEST_FLAG_ALGID_ABSENT)

typedef struct cshake_ctx_st {
    OSSL_LIB_CTX *libctx;
    char *propq;
    EVP_MD_CTX *mdctx;
    EVP_MD *md;
    const uint8_t *func; /* encoded N */
    uint8_t custom[CSHAKE_MAX_ENCODED_STRING]; /* encoded S */
    size_t funclen;
    size_t customlen;
    size_t bitlen;
    size_t xoflen;
    int inited;
} CSHAKE_CTX;

static OSSL_FUNC_digest_freectx_fn cshake_freectx;
static OSSL_FUNC_digest_dupctx_fn cshake_dupctx;
static OSSL_FUNC_digest_init_fn cshake_init;
static OSSL_FUNC_digest_update_fn cshake_update;
static OSSL_FUNC_digest_final_fn cshake_final;
static OSSL_FUNC_digest_squeeze_fn cshake_squeeze;
static OSSL_FUNC_digest_set_ctx_params_fn cshake_set_ctx_params;
static OSSL_FUNC_digest_settable_ctx_params_fn cshake_settable_ctx_params;
static OSSL_FUNC_digest_get_ctx_params_fn cshake_get_ctx_params;
static OSSL_FUNC_digest_gettable_ctx_params_fn cshake_gettable_ctx_params;

typedef struct name_encode_map_st {
    const char *name;
    const uint8_t *encoding;
    size_t encodinglen;
} NAME_ENCODE_MAP;

/* Fixed value of encode_string("") */
static const unsigned char empty_encoded_string[] = {
    0x01, 0x00
};

/* Fixed value of encode_string("KMAC") */
static const unsigned char kmac_encoded_string[] = {
    0x01, 0x20, 0x4B, 0x4D, 0x41, 0x43
};

/* Fixed value of encode_string("TupleHash") */
static const unsigned char tuplehash_encoded_string[] = {
    0x01, 0x48, 0x54, 0x75, 0x70, 0x6C, 0x65, 0x48, 0x61, 0x73, 0x68
};

/* Fixed value of encode_string("ParallelHash") */
static const unsigned char parallelhash_encoded_string[] = {
    0x01, 0x60, 0x50, 0x61, 0x72, 0x61, 0x6C, 0x6C, 0x65, 0x6C, 0x48, 0x61, 0x73, 0x68
};

static int cshake_set_func_encode_string(const char *in,
    const uint8_t **out, size_t *outlen)
{
    /*
     * A list of valid function names to encoded string mappings
     * See NIST SP800-185 Section 3.4
     */
    static NAME_ENCODE_MAP functionNameMap[] = {
        { "", empty_encoded_string, sizeof(empty_encoded_string) },
        { "KMAC", kmac_encoded_string, sizeof(kmac_encoded_string) },
        { "TupleHash", tuplehash_encoded_string, sizeof(tuplehash_encoded_string) },
        { "ParallelHash", parallelhash_encoded_string, sizeof(parallelhash_encoded_string) },
        { NULL, NULL, 0 }
    };

    *out = NULL;
    *outlen = 0;
    /*
     * Don't encode an empty string here - this is done manually later only when
     * one of the strings is not empty. If both are empty then we don't want it
     * to encode at all.
     */
    if (in == NULL || in[0] == 0)
        return 1;
    for (int i = 1; functionNameMap[i].name != NULL; ++i) {
        if (functionNameMap[i].name[0] == in[0]) {
            if (OPENSSL_strcasecmp(functionNameMap[i].name, in) == 0) {
                *out = functionNameMap[i].encoding;
                *outlen = functionNameMap[i].encodinglen;
                return 1;
            }
            return 0; /* Name does not match a known name */
        }
    }
    return 0; /* Name not found */
}

static int cshake_set_encode_string(const char *in,
    uint8_t *out, size_t outmax, size_t *outlen)
{
    size_t inlen;

    if (*outlen != 0)
        OPENSSL_cleanse(out, outmax);
    *outlen = 0;
    if (in == NULL)
        return 1;

    inlen = strlen(in);
    /*
     * Don't encode an empty string here - this is done manually later only when
     * one of the strings is not empty. If both are empty then we don't want it
     * to encode at all.
     */
    if (inlen == 0)
        return 1;
    if (inlen >= CSHAKE_MAX_STRING)
        return 0;
    return ossl_sp800_185_encode_string(out, outmax, outlen,
        (const unsigned char *)in, inlen);
}

/*
 * Set the xof length, note that if the digest has not been fetched yet then
 * it is just set into a variable and deferred to later.
 */
static int cshake_set_xoflen(CSHAKE_CTX *ctx, size_t xoflen)
{
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_size_t(OSSL_DIGEST_PARAM_XOFLEN, &xoflen);
    params[1] = OSSL_PARAM_construct_end();

    ctx->xoflen = xoflen;
    if (ctx->md != NULL)
        return EVP_MD_CTX_set_params(ctx->mdctx, params);
    return 1;
}

/*
 * Fetch a digest for SHAKE or KECCAK, set its xof len and init it
 * into an mdctx.
 */
static int cshake_set_shake_mode(CSHAKE_CTX *ctx, int shake)
{
    OSSL_PARAM params[2];
    const char *name;

    if (shake)
        name = (ctx->bitlen == 128 ? "SHAKE128" : "SHAKE256");
    else
        name = (ctx->bitlen == 128 ? "CSHAKE-KECCAK-128" : "CSHAKE-KECCAK-256");

    if (ctx->md == NULL || !EVP_MD_is_a(ctx->md, name)) {
        ctx->md = EVP_MD_fetch(ctx->libctx, name, ctx->propq);
        if (ctx->md == NULL)
            return 0;
    }
    params[0] = OSSL_PARAM_construct_size_t(OSSL_DIGEST_PARAM_XOFLEN,
        &ctx->xoflen);
    params[1] = OSSL_PARAM_construct_end();
    return EVP_DigestInit_ex2(ctx->mdctx, ctx->md, params);
}

static void *cshake_newctx(void *provctx, size_t bitlen)
{
    CSHAKE_CTX *ctx;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return NULL;
    ctx = OPENSSL_zalloc(sizeof(*ctx));
    if (ctx != NULL) {
        ctx->mdctx = EVP_MD_CTX_create();
        if (ctx->mdctx == NULL) {
            OPENSSL_free(ctx);
            return NULL;
        }
        ctx->bitlen = bitlen;
        ctx->libctx = PROV_LIBCTX_OF(provctx);
    }
    return ctx;
}

static void cshake_freectx(void *vctx)
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;

    OPENSSL_free(ctx->propq);
    EVP_MD_free(ctx->md);
    EVP_MD_CTX_destroy(ctx->mdctx);
    OPENSSL_clear_free(ctx, sizeof(*ctx));
}

static void *cshake_dupctx(void *ctx)
{
    CSHAKE_CTX *src = (CSHAKE_CTX *)ctx;
    CSHAKE_CTX *ret = ossl_prov_is_running() ? OPENSSL_malloc(sizeof(*ret))
                                             : NULL;

    if (ret != NULL) {
        *ret = *src;
        ret->md = NULL;
        ret->mdctx = NULL;
        ret->propq = NULL;

        if (src->md != NULL && !EVP_MD_up_ref(src->md))
            goto err;
        ret->md = src->md;

        if (src->mdctx != NULL) {
            ret->mdctx = EVP_MD_CTX_new();
            if (ret->mdctx == NULL
                || !EVP_MD_CTX_copy_ex(ret->mdctx, src->mdctx))
                goto err;
        }
        if (src->propq != NULL) {
            ret->propq = OPENSSL_strdup(src->propq);
            if (ret->propq == NULL)
                goto err;
        }
    }
    return ret;
err:
    cshake_freectx(ret);
    return NULL;
}

static int cshake_init(void *vctx, const OSSL_PARAM params[])
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;
    ctx->inited = 0;
    ctx->xoflen = (ctx->bitlen == 128) ? 32 : 64; /* Set default values here */
    cshake_set_func_encode_string(NULL, &ctx->func, &ctx->funclen);
    cshake_set_encode_string(NULL, ctx->custom, sizeof(ctx->custom), &ctx->customlen);
    return cshake_set_ctx_params(vctx, params);
}

static const OSSL_PARAM *cshake_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return cshake_set_ctx_params_list;
}

static int set_property_query(CSHAKE_CTX *ctx, const char *propq)
{
    OPENSSL_free(ctx->propq);
    ctx->propq = NULL;
    if (propq != NULL) {
        ctx->propq = OPENSSL_strdup(propq);
        if (ctx->propq == NULL)
            return 0;
    }
    return 1;
}

static int cshake_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;
    struct cshake_set_ctx_params_st p;

    if (ctx == NULL || !cshake_set_ctx_params_decoder(params, &p))
        return 0;

    if (p.xoflen != NULL) {
        size_t xoflen;

        if (!OSSL_PARAM_get_size_t(p.xoflen, &xoflen)
            || !cshake_set_xoflen(ctx, xoflen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
    }
    if (p.func != NULL) {
        if (p.func->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        if (!cshake_set_func_encode_string(p.func->data, &ctx->func, &ctx->funclen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_FUNCTION_NAME);
            return 0;
        }
    }
    if (p.custom != NULL) {
        if (p.custom->data_type != OSSL_PARAM_UTF8_STRING)
            return 0;
        if (!cshake_set_encode_string(p.custom->data, ctx->custom, sizeof(ctx->custom), &ctx->customlen))
            return 0;
    }
    if (p.propq != NULL) {
        if (p.propq->data_type != OSSL_PARAM_UTF8_STRING
            || !set_property_query(ctx, p.propq->data))
            return 0;
    }
    return 1;
}

/*
 * bytepad(encode_string(N) || encode_string(S), w)
 * See SP800-185 Section 2.3.3 Padding.
 *
 * Rather than build an array and do a single keccak operation, we use the
 * internal keccak buffer to simplify the process.
 * Note that if the strings are large enough to fill the buffer, it will handle
 * this internally by absorbing full blocks. The zero padding is also simple
 * as we just fill the buffer with zeros to make it a multiple of the blocksize.
 */
static int cshake_absorb_bytepad_strings(CSHAKE_CTX *ctx)
{
    const uint8_t zeros[SHA3_BLOCKSIZE(128)] = { 0 };
    uint8_t bytepad_header[2] = { 0x01, 0x00 };
    const uint8_t *n = ctx->func, *s = ctx->custom;
    size_t nlen = ctx->funclen, slen = ctx->customlen;
    size_t zlen;
    size_t w = SHA3_BLOCKSIZE(ctx->bitlen); /* w = 168 or 136 */

    bytepad_header[1] = (uint8_t)w;

    /* Empty strings are still encoded */
    if (nlen == 0) {
        n = empty_encoded_string;
        nlen = sizeof(empty_encoded_string);
    }
    if (slen == 0) {
        s = empty_encoded_string;
        slen = sizeof(empty_encoded_string);
    }
    /* Calculate the number of padding zeros to fill up the block */
    zlen = ((sizeof(bytepad_header) + nlen + slen) % w);
    if (zlen != 0)
        zlen = w - zlen;

    /* left encoded(w) || encodestring(n) || encodestring(s) || zero_padding */
    return EVP_DigestUpdate(ctx->mdctx, bytepad_header, sizeof(bytepad_header))
        && EVP_DigestUpdate(ctx->mdctx, n, nlen)
        && EVP_DigestUpdate(ctx->mdctx, s, slen)
        && EVP_DigestUpdate(ctx->mdctx, zeros, zlen);
}

/*
 * The setup of the EVP_MD gets deferred until after the set_ctx_params
 * which means that we need to defer to the functions that may be called
 * afterwards (i.e. The update(), final() or squeeze()).
 *
 */
static int check_init(CSHAKE_CTX *ctx)
{
    /*
     * We have to defer choosing the mode EVP_MD object (SHAKE or KECCAK)
     * until the first call to either update(), final() or squeeze()
     * since the strings can be set at any time before this point.
     */
    if (ctx->inited == 0) {
        if (ctx->funclen != 0 || ctx->customlen != 0) {
            if (!cshake_set_shake_mode(ctx, 0)
                || !cshake_absorb_bytepad_strings(ctx))
                return 0;
        } else {
            /* Use SHAKE if N and S are both empty strings */
            if (!cshake_set_shake_mode(ctx, 1))
                return 0;
        }
        ctx->inited = 1;
    }
    return 1;
}

static int cshake_update(void *vctx, const unsigned char *in, size_t inlen)
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;

    return check_init(ctx)
        && EVP_DigestUpdate(ctx->mdctx, in, inlen);
}

static int cshake_final(void *vctx, uint8_t *out, size_t *outl, size_t outsz)
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;
    unsigned int der = (unsigned int)(*outl);
    int ret = 1;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;

    if (outsz > 0)
        ret = check_init(ctx) && EVP_DigestFinal_ex(ctx->mdctx, out, &der);
    *outl = der;
    return ret;
}

static int cshake_squeeze(void *vctx, uint8_t *out, size_t *outl, size_t outsz)
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;
    int ret = 1;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;

    if (outsz > 0)
        ret = check_init(ctx) && EVP_DigestSqueeze(ctx->mdctx, out, outsz);
    if (ret && outl != NULL)
        *outl = outsz;
    return ret;
}

static const OSSL_PARAM *cshake_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return cshake_get_ctx_params_list;
}

static int cshake_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    CSHAKE_CTX *ctx = (CSHAKE_CTX *)vctx;
    struct cshake_get_ctx_params_st p;

    if (ctx == NULL || !cshake_get_ctx_params_decoder(params, &p))
        return 0;

    /* Size is an alias of xoflen */
    if (p.xoflen != NULL || p.size != NULL) {
        size_t xoflen = ctx->xoflen;

        if (ctx->md != NULL)
            xoflen = EVP_MD_CTX_get_size_ex(ctx->mdctx);

        if (p.size != NULL && !OSSL_PARAM_set_size_t(p.size, xoflen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
        if (p.xoflen != NULL && !OSSL_PARAM_set_size_t(p.xoflen, xoflen)) {
            ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
            return 0;
        }
    }
    return 1;
}

#define IMPLEMENT_CSHAKE_functions(bitlen)                                          \
    static OSSL_FUNC_digest_newctx_fn cshake_##bitlen##_newctx;                     \
    static void *cshake_##bitlen##_newctx(void *provctx)                            \
    {                                                                               \
        return cshake_newctx(provctx, bitlen);                                      \
    }                                                                               \
    PROV_FUNC_DIGEST_GET_PARAM(cshake_##bitlen, SHA3_BLOCKSIZE(bitlen),             \
        CSHAKE_KECCAK_MDSIZE(bitlen), CSHAKE_FLAGS)                                 \
    const OSSL_DISPATCH ossl_cshake_##bitlen##_functions[] = {                      \
        { OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))cshake_##bitlen##_newctx },      \
        { OSSL_FUNC_DIGEST_INIT, (void (*)(void))cshake_init },                     \
        { OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))cshake_update },                 \
        { OSSL_FUNC_DIGEST_FINAL, (void (*)(void))cshake_final },                   \
        { OSSL_FUNC_DIGEST_SQUEEZE, (void (*)(void))cshake_squeeze },               \
        { OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))cshake_freectx },               \
        { OSSL_FUNC_DIGEST_DUPCTX, (void (*)(void))cshake_dupctx },                 \
        { OSSL_FUNC_DIGEST_SET_CTX_PARAMS, (void (*)(void))cshake_set_ctx_params }, \
        { OSSL_FUNC_DIGEST_SETTABLE_CTX_PARAMS,                                     \
            (void (*)(void))cshake_settable_ctx_params },                           \
        { OSSL_FUNC_DIGEST_GET_CTX_PARAMS, (void (*)(void))cshake_get_ctx_params }, \
        { OSSL_FUNC_DIGEST_GETTABLE_CTX_PARAMS,                                     \
            (void (*)(void))cshake_gettable_ctx_params },                           \
        PROV_DISPATCH_FUNC_DIGEST_GET_PARAMS(cshake_##bitlen),                      \
        PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END

/* ossl_cshake_128_functions */
IMPLEMENT_CSHAKE_functions(128)
    /* ossl_cshake_256_functions */
    IMPLEMENT_CSHAKE_functions(256)
