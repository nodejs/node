/**
 * \file md.c
 *
 * \brief Generic message digest wrapper for Mbed TLS
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

/*
 * Availability of functions in this module is controlled by two
 * feature macros:
 * - MBEDTLS_MD_C enables the whole module;
 * - MBEDTLS_MD_LIGHT enables only functions for hashing and accessing
 * most hash metadata (everything except string names); is it
 * automatically set whenever MBEDTLS_MD_C is defined.
 *
 * In this file, functions from MD_LIGHT are at the top, MD_C at the end.
 *
 * In the future we may want to change the contract of some functions
 * (behaviour with NULL arguments) depending on whether MD_C is defined or
 * only MD_LIGHT. Also, the exact scope of MD_LIGHT might vary.
 *
 * For these reasons, we're keeping MD_LIGHT internal for now.
 */
#if defined(MBEDTLS_MD_LIGHT)

#include "mbedtls/md.h"
#include "md_wrap.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include "mbedtls/md5.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/sha3.h"

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
#include <psa/crypto.h>
#include "md_psa.h"
#include "psa_util_internal.h"
#endif

#if defined(MBEDTLS_MD_SOME_PSA)
#include "psa_crypto_core.h"
#endif

#include "mbedtls/platform.h"

#include <string.h>

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#endif

/* See comment above MBEDTLS_MD_MAX_SIZE in md.h */
#if defined(MBEDTLS_PSA_CRYPTO_C) && MBEDTLS_MD_MAX_SIZE < PSA_HASH_MAX_SIZE
#error "Internal error: MBEDTLS_MD_MAX_SIZE < PSA_HASH_MAX_SIZE"
#endif

#if defined(MBEDTLS_MD_C)
#define MD_INFO(type, out_size, block_size) type, out_size, block_size,
#else
#define MD_INFO(type, out_size, block_size) type, out_size,
#endif

#if defined(MBEDTLS_MD_CAN_MD5)
static const mbedtls_md_info_t mbedtls_md5_info = {
    MD_INFO(MBEDTLS_MD_MD5, 16, 64)
};
#endif

#if defined(MBEDTLS_MD_CAN_RIPEMD160)
static const mbedtls_md_info_t mbedtls_ripemd160_info = {
    MD_INFO(MBEDTLS_MD_RIPEMD160, 20, 64)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA1)
static const mbedtls_md_info_t mbedtls_sha1_info = {
    MD_INFO(MBEDTLS_MD_SHA1, 20, 64)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA224)
static const mbedtls_md_info_t mbedtls_sha224_info = {
    MD_INFO(MBEDTLS_MD_SHA224, 28, 64)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA256)
static const mbedtls_md_info_t mbedtls_sha256_info = {
    MD_INFO(MBEDTLS_MD_SHA256, 32, 64)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA384)
static const mbedtls_md_info_t mbedtls_sha384_info = {
    MD_INFO(MBEDTLS_MD_SHA384, 48, 128)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA512)
static const mbedtls_md_info_t mbedtls_sha512_info = {
    MD_INFO(MBEDTLS_MD_SHA512, 64, 128)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_224)
static const mbedtls_md_info_t mbedtls_sha3_224_info = {
    MD_INFO(MBEDTLS_MD_SHA3_224, 28, 144)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_256)
static const mbedtls_md_info_t mbedtls_sha3_256_info = {
    MD_INFO(MBEDTLS_MD_SHA3_256, 32, 136)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_384)
static const mbedtls_md_info_t mbedtls_sha3_384_info = {
    MD_INFO(MBEDTLS_MD_SHA3_384, 48, 104)
};
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_512)
static const mbedtls_md_info_t mbedtls_sha3_512_info = {
    MD_INFO(MBEDTLS_MD_SHA3_512, 64, 72)
};
#endif

const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type)
{
    switch (md_type) {
#if defined(MBEDTLS_MD_CAN_MD5)
        case MBEDTLS_MD_MD5:
            return &mbedtls_md5_info;
#endif
#if defined(MBEDTLS_MD_CAN_RIPEMD160)
        case MBEDTLS_MD_RIPEMD160:
            return &mbedtls_ripemd160_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA1)
        case MBEDTLS_MD_SHA1:
            return &mbedtls_sha1_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA224)
        case MBEDTLS_MD_SHA224:
            return &mbedtls_sha224_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA256)
        case MBEDTLS_MD_SHA256:
            return &mbedtls_sha256_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA384)
        case MBEDTLS_MD_SHA384:
            return &mbedtls_sha384_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA512)
        case MBEDTLS_MD_SHA512:
            return &mbedtls_sha512_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_224)
        case MBEDTLS_MD_SHA3_224:
            return &mbedtls_sha3_224_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_256)
        case MBEDTLS_MD_SHA3_256:
            return &mbedtls_sha3_256_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_384)
        case MBEDTLS_MD_SHA3_384:
            return &mbedtls_sha3_384_info;
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_512)
        case MBEDTLS_MD_SHA3_512:
            return &mbedtls_sha3_512_info;
#endif
        default:
            return NULL;
    }
}

#if defined(MBEDTLS_MD_SOME_PSA)
static psa_algorithm_t psa_alg_of_md(const mbedtls_md_info_t *info)
{
    switch (info->type) {
#if defined(MBEDTLS_MD_MD5_VIA_PSA)
        case MBEDTLS_MD_MD5:
            return PSA_ALG_MD5;
#endif
#if defined(MBEDTLS_MD_RIPEMD160_VIA_PSA)
        case MBEDTLS_MD_RIPEMD160:
            return PSA_ALG_RIPEMD160;
#endif
#if defined(MBEDTLS_MD_SHA1_VIA_PSA)
        case MBEDTLS_MD_SHA1:
            return PSA_ALG_SHA_1;
#endif
#if defined(MBEDTLS_MD_SHA224_VIA_PSA)
        case MBEDTLS_MD_SHA224:
            return PSA_ALG_SHA_224;
#endif
#if defined(MBEDTLS_MD_SHA256_VIA_PSA)
        case MBEDTLS_MD_SHA256:
            return PSA_ALG_SHA_256;
#endif
#if defined(MBEDTLS_MD_SHA384_VIA_PSA)
        case MBEDTLS_MD_SHA384:
            return PSA_ALG_SHA_384;
#endif
#if defined(MBEDTLS_MD_SHA512_VIA_PSA)
        case MBEDTLS_MD_SHA512:
            return PSA_ALG_SHA_512;
#endif
#if defined(MBEDTLS_MD_SHA3_224_VIA_PSA)
        case MBEDTLS_MD_SHA3_224:
            return PSA_ALG_SHA3_224;
#endif
#if defined(MBEDTLS_MD_SHA3_256_VIA_PSA)
        case MBEDTLS_MD_SHA3_256:
            return PSA_ALG_SHA3_256;
#endif
#if defined(MBEDTLS_MD_SHA3_384_VIA_PSA)
        case MBEDTLS_MD_SHA3_384:
            return PSA_ALG_SHA3_384;
#endif
#if defined(MBEDTLS_MD_SHA3_512_VIA_PSA)
        case MBEDTLS_MD_SHA3_512:
            return PSA_ALG_SHA3_512;
#endif
        default:
            return PSA_ALG_NONE;
    }
}

static int md_can_use_psa(const mbedtls_md_info_t *info)
{
    psa_algorithm_t alg = psa_alg_of_md(info);
    if (alg == PSA_ALG_NONE) {
        return 0;
    }

    return psa_can_do_hash(alg);
}
#endif /* MBEDTLS_MD_SOME_PSA */

void mbedtls_md_init(mbedtls_md_context_t *ctx)
{
    /* Note: this sets engine (if present) to MBEDTLS_MD_ENGINE_LEGACY */
    memset(ctx, 0, sizeof(mbedtls_md_context_t));
}

void mbedtls_md_free(mbedtls_md_context_t *ctx)
{
    if (ctx == NULL || ctx->md_info == NULL) {
        return;
    }

    if (ctx->md_ctx != NULL) {
#if defined(MBEDTLS_MD_SOME_PSA)
        if (ctx->engine == MBEDTLS_MD_ENGINE_PSA) {
            psa_hash_abort(ctx->md_ctx);
        } else
#endif
        switch (ctx->md_info->type) {
#if defined(MBEDTLS_MD5_C)
            case MBEDTLS_MD_MD5:
                mbedtls_md5_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_RIPEMD160_C)
            case MBEDTLS_MD_RIPEMD160:
                mbedtls_ripemd160_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA1_C)
            case MBEDTLS_MD_SHA1:
                mbedtls_sha1_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA224_C)
            case MBEDTLS_MD_SHA224:
                mbedtls_sha256_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA256_C)
            case MBEDTLS_MD_SHA256:
                mbedtls_sha256_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA384_C)
            case MBEDTLS_MD_SHA384:
                mbedtls_sha512_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA512_C)
            case MBEDTLS_MD_SHA512:
                mbedtls_sha512_free(ctx->md_ctx);
                break;
#endif
#if defined(MBEDTLS_SHA3_C)
            case MBEDTLS_MD_SHA3_224:
            case MBEDTLS_MD_SHA3_256:
            case MBEDTLS_MD_SHA3_384:
            case MBEDTLS_MD_SHA3_512:
                mbedtls_sha3_free(ctx->md_ctx);
                break;
#endif
            default:
                /* Shouldn't happen */
                break;
        }
        mbedtls_free(ctx->md_ctx);
    }

#if defined(MBEDTLS_MD_C)
    if (ctx->hmac_ctx != NULL) {
        mbedtls_zeroize_and_free(ctx->hmac_ctx,
                                 2 * ctx->md_info->block_size);
    }
#endif

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_md_context_t));
}

int mbedtls_md_clone(mbedtls_md_context_t *dst,
                     const mbedtls_md_context_t *src)
{
    if (dst == NULL || dst->md_info == NULL ||
        src == NULL || src->md_info == NULL ||
        dst->md_info != src->md_info) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_MD_SOME_PSA)
    if (src->engine != dst->engine) {
        /* This can happen with src set to legacy because PSA wasn't ready
         * yet, and dst to PSA because it became ready in the meantime.
         * We currently don't support that case (we'd need to re-allocate
         * md_ctx to the size of the appropriate MD context). */
        return MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;
    }

    if (src->engine == MBEDTLS_MD_ENGINE_PSA) {
        psa_status_t status = psa_hash_clone(src->md_ctx, dst->md_ctx);
        return mbedtls_md_error_from_psa(status);
    }
#endif

    switch (src->md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            mbedtls_md5_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            mbedtls_ripemd160_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            mbedtls_sha1_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            mbedtls_sha256_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            mbedtls_sha256_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            mbedtls_sha512_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            mbedtls_sha512_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
        case MBEDTLS_MD_SHA3_256:
        case MBEDTLS_MD_SHA3_384:
        case MBEDTLS_MD_SHA3_512:
            mbedtls_sha3_clone(dst->md_ctx, src->md_ctx);
            break;
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    return 0;
}

#define ALLOC(type)                                                   \
    do {                                                                \
        ctx->md_ctx = mbedtls_calloc(1, sizeof(mbedtls_##type##_context)); \
        if (ctx->md_ctx == NULL)                                       \
        return MBEDTLS_ERR_MD_ALLOC_FAILED;                      \
        mbedtls_##type##_init(ctx->md_ctx);                           \
    }                                                                   \
    while (0)

int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *md_info, int hmac)
{
#if defined(MBEDTLS_MD_C)
    if (ctx == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
#endif
    if (md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    ctx->md_info = md_info;
    ctx->md_ctx = NULL;
#if defined(MBEDTLS_MD_C)
    ctx->hmac_ctx = NULL;
#else
    if (hmac != 0) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
#endif

#if defined(MBEDTLS_MD_SOME_PSA)
    if (md_can_use_psa(ctx->md_info)) {
        ctx->md_ctx = mbedtls_calloc(1, sizeof(psa_hash_operation_t));
        if (ctx->md_ctx == NULL) {
            return MBEDTLS_ERR_MD_ALLOC_FAILED;
        }
        ctx->engine = MBEDTLS_MD_ENGINE_PSA;
    } else
#endif
    switch (md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            ALLOC(md5);
            break;
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            ALLOC(ripemd160);
            break;
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            ALLOC(sha1);
            break;
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            ALLOC(sha256);
            break;
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            ALLOC(sha256);
            break;
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            ALLOC(sha512);
            break;
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            ALLOC(sha512);
            break;
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
        case MBEDTLS_MD_SHA3_256:
        case MBEDTLS_MD_SHA3_384:
        case MBEDTLS_MD_SHA3_512:
            ALLOC(sha3);
            break;
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_MD_C)
    if (hmac != 0) {
        ctx->hmac_ctx = mbedtls_calloc(2, md_info->block_size);
        if (ctx->hmac_ctx == NULL) {
            mbedtls_md_free(ctx);
            return MBEDTLS_ERR_MD_ALLOC_FAILED;
        }
    }
#endif

    return 0;
}
#undef ALLOC

int mbedtls_md_starts(mbedtls_md_context_t *ctx)
{
#if defined(MBEDTLS_MD_C)
    if (ctx == NULL || ctx->md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
#endif

#if defined(MBEDTLS_MD_SOME_PSA)
    if (ctx->engine == MBEDTLS_MD_ENGINE_PSA) {
        psa_algorithm_t alg = psa_alg_of_md(ctx->md_info);
        psa_hash_abort(ctx->md_ctx);
        psa_status_t status = psa_hash_setup(ctx->md_ctx, alg);
        return mbedtls_md_error_from_psa(status);
    }
#endif

    switch (ctx->md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            return mbedtls_md5_starts(ctx->md_ctx);
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            return mbedtls_ripemd160_starts(ctx->md_ctx);
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            return mbedtls_sha1_starts(ctx->md_ctx);
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            return mbedtls_sha256_starts(ctx->md_ctx, 1);
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            return mbedtls_sha256_starts(ctx->md_ctx, 0);
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            return mbedtls_sha512_starts(ctx->md_ctx, 1);
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            return mbedtls_sha512_starts(ctx->md_ctx, 0);
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
            return mbedtls_sha3_starts(ctx->md_ctx, MBEDTLS_SHA3_224);
        case MBEDTLS_MD_SHA3_256:
            return mbedtls_sha3_starts(ctx->md_ctx, MBEDTLS_SHA3_256);
        case MBEDTLS_MD_SHA3_384:
            return mbedtls_sha3_starts(ctx->md_ctx, MBEDTLS_SHA3_384);
        case MBEDTLS_MD_SHA3_512:
            return mbedtls_sha3_starts(ctx->md_ctx, MBEDTLS_SHA3_512);
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
}

int mbedtls_md_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen)
{
#if defined(MBEDTLS_MD_C)
    if (ctx == NULL || ctx->md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
#endif

#if defined(MBEDTLS_MD_SOME_PSA)
    if (ctx->engine == MBEDTLS_MD_ENGINE_PSA) {
        psa_status_t status = psa_hash_update(ctx->md_ctx, input, ilen);
        return mbedtls_md_error_from_psa(status);
    }
#endif

    switch (ctx->md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            return mbedtls_md5_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            return mbedtls_ripemd160_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            return mbedtls_sha1_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            return mbedtls_sha256_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            return mbedtls_sha256_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            return mbedtls_sha512_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            return mbedtls_sha512_update(ctx->md_ctx, input, ilen);
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
        case MBEDTLS_MD_SHA3_256:
        case MBEDTLS_MD_SHA3_384:
        case MBEDTLS_MD_SHA3_512:
            return mbedtls_sha3_update(ctx->md_ctx, input, ilen);
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
}

int mbedtls_md_finish(mbedtls_md_context_t *ctx, unsigned char *output)
{
#if defined(MBEDTLS_MD_C)
    if (ctx == NULL || ctx->md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
#endif

#if defined(MBEDTLS_MD_SOME_PSA)
    if (ctx->engine == MBEDTLS_MD_ENGINE_PSA) {
        size_t size = ctx->md_info->size;
        psa_status_t status = psa_hash_finish(ctx->md_ctx,
                                              output, size, &size);
        return mbedtls_md_error_from_psa(status);
    }
#endif

    switch (ctx->md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            return mbedtls_md5_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            return mbedtls_ripemd160_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            return mbedtls_sha1_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            return mbedtls_sha256_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            return mbedtls_sha256_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            return mbedtls_sha512_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            return mbedtls_sha512_finish(ctx->md_ctx, output);
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
        case MBEDTLS_MD_SHA3_256:
        case MBEDTLS_MD_SHA3_384:
        case MBEDTLS_MD_SHA3_512:
            return mbedtls_sha3_finish(ctx->md_ctx, output, ctx->md_info->size);
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
}

int mbedtls_md(const mbedtls_md_info_t *md_info, const unsigned char *input, size_t ilen,
               unsigned char *output)
{
    if (md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_MD_SOME_PSA)
    if (md_can_use_psa(md_info)) {
        size_t size = md_info->size;
        psa_status_t status = psa_hash_compute(psa_alg_of_md(md_info),
                                               input, ilen,
                                               output, size, &size);
        return mbedtls_md_error_from_psa(status);
    }
#endif

    switch (md_info->type) {
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            return mbedtls_md5(input, ilen, output);
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            return mbedtls_ripemd160(input, ilen, output);
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            return mbedtls_sha1(input, ilen, output);
#endif
#if defined(MBEDTLS_SHA224_C)
        case MBEDTLS_MD_SHA224:
            return mbedtls_sha256(input, ilen, output, 1);
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA256:
            return mbedtls_sha256(input, ilen, output, 0);
#endif
#if defined(MBEDTLS_SHA384_C)
        case MBEDTLS_MD_SHA384:
            return mbedtls_sha512(input, ilen, output, 1);
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA512:
            return mbedtls_sha512(input, ilen, output, 0);
#endif
#if defined(MBEDTLS_SHA3_C)
        case MBEDTLS_MD_SHA3_224:
            return mbedtls_sha3(MBEDTLS_SHA3_224, input, ilen, output, md_info->size);
        case MBEDTLS_MD_SHA3_256:
            return mbedtls_sha3(MBEDTLS_SHA3_256, input, ilen, output, md_info->size);
        case MBEDTLS_MD_SHA3_384:
            return mbedtls_sha3(MBEDTLS_SHA3_384, input, ilen, output, md_info->size);
        case MBEDTLS_MD_SHA3_512:
            return mbedtls_sha3(MBEDTLS_SHA3_512, input, ilen, output, md_info->size);
#endif
        default:
            return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
}

unsigned char mbedtls_md_get_size(const mbedtls_md_info_t *md_info)
{
    if (md_info == NULL) {
        return 0;
    }

    return md_info->size;
}

mbedtls_md_type_t mbedtls_md_get_type(const mbedtls_md_info_t *md_info)
{
    if (md_info == NULL) {
        return MBEDTLS_MD_NONE;
    }

    return md_info->type;
}

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)
int mbedtls_md_error_from_psa(psa_status_t status)
{
    return PSA_TO_MBEDTLS_ERR_LIST(status, psa_to_md_errors,
                                   psa_generic_status_to_mbedtls);
}
#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */


/************************************************************************
 * Functions above this separator are part of MBEDTLS_MD_LIGHT,         *
 * functions below are only available when MBEDTLS_MD_C is set.         *
 ************************************************************************/
#if defined(MBEDTLS_MD_C)

/*
 * Reminder: update profiles in x509_crt.c when adding a new hash!
 */
static const int supported_digests[] = {

#if defined(MBEDTLS_MD_CAN_SHA512)
    MBEDTLS_MD_SHA512,
#endif

#if defined(MBEDTLS_MD_CAN_SHA384)
    MBEDTLS_MD_SHA384,
#endif

#if defined(MBEDTLS_MD_CAN_SHA256)
    MBEDTLS_MD_SHA256,
#endif
#if defined(MBEDTLS_MD_CAN_SHA224)
    MBEDTLS_MD_SHA224,
#endif

#if defined(MBEDTLS_MD_CAN_SHA1)
    MBEDTLS_MD_SHA1,
#endif

#if defined(MBEDTLS_MD_CAN_RIPEMD160)
    MBEDTLS_MD_RIPEMD160,
#endif

#if defined(MBEDTLS_MD_CAN_MD5)
    MBEDTLS_MD_MD5,
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_224)
    MBEDTLS_MD_SHA3_224,
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_256)
    MBEDTLS_MD_SHA3_256,
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_384)
    MBEDTLS_MD_SHA3_384,
#endif

#if defined(MBEDTLS_MD_CAN_SHA3_512)
    MBEDTLS_MD_SHA3_512,
#endif

    MBEDTLS_MD_NONE
};

const int *mbedtls_md_list(void)
{
    return supported_digests;
}

typedef struct {
    const char *md_name;
    mbedtls_md_type_t md_type;
} md_name_entry;

static const md_name_entry md_names[] = {
#if defined(MBEDTLS_MD_CAN_MD5)
    { "MD5", MBEDTLS_MD_MD5 },
#endif
#if defined(MBEDTLS_MD_CAN_RIPEMD160)
    { "RIPEMD160", MBEDTLS_MD_RIPEMD160 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA1)
    { "SHA1", MBEDTLS_MD_SHA1 },
    { "SHA", MBEDTLS_MD_SHA1 }, // compatibility fallback
#endif
#if defined(MBEDTLS_MD_CAN_SHA224)
    { "SHA224", MBEDTLS_MD_SHA224 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA256)
    { "SHA256", MBEDTLS_MD_SHA256 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA384)
    { "SHA384", MBEDTLS_MD_SHA384 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA512)
    { "SHA512", MBEDTLS_MD_SHA512 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_224)
    { "SHA3-224", MBEDTLS_MD_SHA3_224 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_256)
    { "SHA3-256", MBEDTLS_MD_SHA3_256 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_384)
    { "SHA3-384", MBEDTLS_MD_SHA3_384 },
#endif
#if defined(MBEDTLS_MD_CAN_SHA3_512)
    { "SHA3-512", MBEDTLS_MD_SHA3_512 },
#endif
    { NULL, MBEDTLS_MD_NONE },
};

const mbedtls_md_info_t *mbedtls_md_info_from_string(const char *md_name)
{
    if (NULL == md_name) {
        return NULL;
    }

    const md_name_entry *entry = md_names;
    while (entry->md_name != NULL &&
           strcmp(entry->md_name, md_name) != 0) {
        ++entry;
    }

    return mbedtls_md_info_from_type(entry->md_type);
}

const char *mbedtls_md_get_name(const mbedtls_md_info_t *md_info)
{
    if (md_info == NULL) {
        return NULL;
    }

    const md_name_entry *entry = md_names;
    while (entry->md_type != MBEDTLS_MD_NONE &&
           entry->md_type != md_info->type) {
        ++entry;
    }

    return entry->md_name;
}

const mbedtls_md_info_t *mbedtls_md_info_from_ctx(
    const mbedtls_md_context_t *ctx)
{
    if (ctx == NULL) {
        return NULL;
    }

    return ctx->MBEDTLS_PRIVATE(md_info);
}

#if defined(MBEDTLS_FS_IO)
int mbedtls_md_file(const mbedtls_md_info_t *md_info, const char *path, unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    FILE *f;
    size_t n;
    mbedtls_md_context_t ctx;
    unsigned char buf[1024];

    if (md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if ((f = fopen(path, "rb")) == NULL) {
        return MBEDTLS_ERR_MD_FILE_IO_ERROR;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(f, NULL);

    mbedtls_md_init(&ctx);

    if ((ret = mbedtls_md_setup(&ctx, md_info, 0)) != 0) {
        goto cleanup;
    }

    if ((ret = mbedtls_md_starts(&ctx)) != 0) {
        goto cleanup;
    }

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if ((ret = mbedtls_md_update(&ctx, buf, n)) != 0) {
            goto cleanup;
        }
    }

    if (ferror(f) != 0) {
        ret = MBEDTLS_ERR_MD_FILE_IO_ERROR;
    } else {
        ret = mbedtls_md_finish(&ctx, output);
    }

cleanup:
    mbedtls_platform_zeroize(buf, sizeof(buf));
    fclose(f);
    mbedtls_md_free(&ctx);

    return ret;
}
#endif /* MBEDTLS_FS_IO */

int mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key, size_t keylen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char sum[MBEDTLS_MD_MAX_SIZE];
    unsigned char *ipad, *opad;

    if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if (keylen > (size_t) ctx->md_info->block_size) {
        if ((ret = mbedtls_md_starts(ctx)) != 0) {
            goto cleanup;
        }
        if ((ret = mbedtls_md_update(ctx, key, keylen)) != 0) {
            goto cleanup;
        }
        if ((ret = mbedtls_md_finish(ctx, sum)) != 0) {
            goto cleanup;
        }

        keylen = ctx->md_info->size;
        key = sum;
    }

    ipad = (unsigned char *) ctx->hmac_ctx;
    opad = (unsigned char *) ctx->hmac_ctx + ctx->md_info->block_size;

    memset(ipad, 0x36, ctx->md_info->block_size);
    memset(opad, 0x5C, ctx->md_info->block_size);

    mbedtls_xor(ipad, ipad, key, keylen);
    mbedtls_xor(opad, opad, key, keylen);

    if ((ret = mbedtls_md_starts(ctx)) != 0) {
        goto cleanup;
    }
    if ((ret = mbedtls_md_update(ctx, ipad,
                                 ctx->md_info->block_size)) != 0) {
        goto cleanup;
    }

cleanup:
    mbedtls_platform_zeroize(sum, sizeof(sum));

    return ret;
}

int mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen)
{
    if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    return mbedtls_md_update(ctx, input, ilen);
}

int mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char tmp[MBEDTLS_MD_MAX_SIZE];
    unsigned char *opad;

    if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    opad = (unsigned char *) ctx->hmac_ctx + ctx->md_info->block_size;

    if ((ret = mbedtls_md_finish(ctx, tmp)) != 0) {
        return ret;
    }
    if ((ret = mbedtls_md_starts(ctx)) != 0) {
        return ret;
    }
    if ((ret = mbedtls_md_update(ctx, opad,
                                 ctx->md_info->block_size)) != 0) {
        return ret;
    }
    if ((ret = mbedtls_md_update(ctx, tmp,
                                 ctx->md_info->size)) != 0) {
        return ret;
    }
    return mbedtls_md_finish(ctx, output);
}

int mbedtls_md_hmac_reset(mbedtls_md_context_t *ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *ipad;

    if (ctx == NULL || ctx->md_info == NULL || ctx->hmac_ctx == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    ipad = (unsigned char *) ctx->hmac_ctx;

    if ((ret = mbedtls_md_starts(ctx)) != 0) {
        return ret;
    }
    return mbedtls_md_update(ctx, ipad, ctx->md_info->block_size);
}

int mbedtls_md_hmac(const mbedtls_md_info_t *md_info,
                    const unsigned char *key, size_t keylen,
                    const unsigned char *input, size_t ilen,
                    unsigned char *output)
{
    mbedtls_md_context_t ctx;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (md_info == NULL) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    mbedtls_md_init(&ctx);

    if ((ret = mbedtls_md_setup(&ctx, md_info, 1)) != 0) {
        goto cleanup;
    }

    if ((ret = mbedtls_md_hmac_starts(&ctx, key, keylen)) != 0) {
        goto cleanup;
    }
    if ((ret = mbedtls_md_hmac_update(&ctx, input, ilen)) != 0) {
        goto cleanup;
    }
    if ((ret = mbedtls_md_hmac_finish(&ctx, output)) != 0) {
        goto cleanup;
    }

cleanup:
    mbedtls_md_free(&ctx);

    return ret;
}

#endif /* MBEDTLS_MD_C */

#endif /* MBEDTLS_MD_LIGHT */
