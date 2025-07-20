/*
 * Copyright 2019-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/sha3.h"
#include "prov/digestcommon.h"
#include "prov/implementations.h"

#define SHA3_FLAGS PROV_DIGEST_FLAG_ALGID_ABSENT
#define SHAKE_FLAGS (PROV_DIGEST_FLAG_XOF | PROV_DIGEST_FLAG_ALGID_ABSENT)
#define KMAC_FLAGS PROV_DIGEST_FLAG_XOF

/*
 * Forward declaration of any unique methods implemented here. This is not strictly
 * necessary for the compiler, but provides an assurance that the signatures
 * of the functions in the dispatch table are correct.
 */
static OSSL_FUNC_digest_init_fn keccak_init;
static OSSL_FUNC_digest_init_fn keccak_init_params;
static OSSL_FUNC_digest_update_fn keccak_update;
static OSSL_FUNC_digest_final_fn keccak_final;
static OSSL_FUNC_digest_freectx_fn keccak_freectx;
static OSSL_FUNC_digest_dupctx_fn keccak_dupctx;
static OSSL_FUNC_digest_set_ctx_params_fn shake_set_ctx_params;
static OSSL_FUNC_digest_settable_ctx_params_fn shake_settable_ctx_params;
static sha3_absorb_fn generic_sha3_absorb;
static sha3_final_fn generic_sha3_final;

#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__) && defined(KECCAK1600_ASM)
/*
 * IBM S390X support
 */
# include "s390x_arch.h"
# define S390_SHA3 1
# define S390_SHA3_CAPABLE(name) \
    ((OPENSSL_s390xcap_P.kimd[0] & S390X_CAPBIT(S390X_##name)) && \
     (OPENSSL_s390xcap_P.klmd[0] & S390X_CAPBIT(S390X_##name)))

#endif

static int keccak_init(void *vctx, ossl_unused const OSSL_PARAM params[])
{
    if (!ossl_prov_is_running())
        return 0;
    /* The newctx() handles most of the ctx fixed setup. */
    ossl_sha3_reset((KECCAK1600_CTX *)vctx);
    return 1;
}

static int keccak_init_params(void *vctx, const OSSL_PARAM params[])
{
    return keccak_init(vctx, NULL)
            && shake_set_ctx_params(vctx, params);
}

static int keccak_update(void *vctx, const unsigned char *inp, size_t len)
{
    KECCAK1600_CTX *ctx = vctx;
    const size_t bsz = ctx->block_size;
    size_t num, rem;

    if (len == 0)
        return 1;

    /* Is there anything in the buffer already ? */
    if ((num = ctx->bufsz) != 0) {
        /* Calculate how much space is left in the buffer */
        rem = bsz - num;
        /* If the new input does not fill the buffer then just add it */
        if (len < rem) {
            memcpy(ctx->buf + num, inp, len);
            ctx->bufsz += len;
            return 1;
        }
        /* otherwise fill up the buffer and absorb the buffer */
        memcpy(ctx->buf + num, inp, rem);
        /* Update the input pointer */
        inp += rem;
        len -= rem;
        ctx->meth.absorb(ctx, ctx->buf, bsz);
        ctx->bufsz = 0;
    }
    /* Absorb the input - rem = leftover part of the input < blocksize) */
    rem = ctx->meth.absorb(ctx, inp, len);
    /* Copy the leftover bit of the input into the buffer */
    if (rem) {
        memcpy(ctx->buf, inp + len - rem, rem);
        ctx->bufsz = rem;
    }
    return 1;
}

static int keccak_final(void *vctx, unsigned char *out, size_t *outl,
                        size_t outsz)
{
    int ret = 1;
    KECCAK1600_CTX *ctx = vctx;

    if (!ossl_prov_is_running())
        return 0;
    if (outsz > 0)
        ret = ctx->meth.final(out, ctx);

    *outl = ctx->md_size;
    return ret;
}

/*-
 * Generic software version of the absorb() and final().
 */
static size_t generic_sha3_absorb(void *vctx, const void *inp, size_t len)
{
    KECCAK1600_CTX *ctx = vctx;

    return SHA3_absorb(ctx->A, inp, len, ctx->block_size);
}

static int generic_sha3_final(unsigned char *md, void *vctx)
{
    return ossl_sha3_final(md, (KECCAK1600_CTX *)vctx);
}

static PROV_SHA3_METHOD sha3_generic_md =
{
    generic_sha3_absorb,
    generic_sha3_final
};

#if defined(S390_SHA3)

static sha3_absorb_fn s390x_sha3_absorb;
static sha3_final_fn s390x_sha3_final;
static sha3_final_fn s390x_shake_final;

/*-
 * The platform specific parts of the absorb() and final() for S390X.
 */
static size_t s390x_sha3_absorb(void *vctx, const void *inp, size_t len)
{
    KECCAK1600_CTX *ctx = vctx;
    size_t rem = len % ctx->block_size;

    s390x_kimd(inp, len - rem, ctx->pad, ctx->A);
    return rem;
}

static int s390x_sha3_final(unsigned char *md, void *vctx)
{
    KECCAK1600_CTX *ctx = vctx;

    if (!ossl_prov_is_running())
        return 0;
    s390x_klmd(ctx->buf, ctx->bufsz, NULL, 0, ctx->pad, ctx->A);
    memcpy(md, ctx->A, ctx->md_size);
    return 1;
}

static int s390x_shake_final(unsigned char *md, void *vctx)
{
    KECCAK1600_CTX *ctx = vctx;

    if (!ossl_prov_is_running())
        return 0;
    s390x_klmd(ctx->buf, ctx->bufsz, md, ctx->md_size, ctx->pad, ctx->A);
    return 1;
}

static PROV_SHA3_METHOD sha3_s390x_md =
{
    s390x_sha3_absorb,
    s390x_sha3_final
};

static PROV_SHA3_METHOD shake_s390x_md =
{
    s390x_sha3_absorb,
    s390x_shake_final
};

# define SHA3_SET_MD(uname, typ)                                               \
    if (S390_SHA3_CAPABLE(uname)) {                                            \
        ctx->pad = S390X_##uname;                                              \
        ctx->meth = typ##_s390x_md;                                            \
    } else {                                                                   \
        ctx->meth = sha3_generic_md;                                           \
    }
#else
# define SHA3_SET_MD(uname, typ) ctx->meth = sha3_generic_md;
#endif /* S390_SHA3 */

#define SHA3_newctx(typ, uname, name, bitlen, pad)                             \
static OSSL_FUNC_digest_newctx_fn name##_newctx;                               \
static void *name##_newctx(void *provctx)                                      \
{                                                                              \
    KECCAK1600_CTX *ctx = ossl_prov_is_running() ? OPENSSL_zalloc(sizeof(*ctx)) \
                                                : NULL;                        \
                                                                               \
    if (ctx == NULL)                                                           \
        return NULL;                                                           \
    ossl_sha3_init(ctx, pad, bitlen);                                          \
    SHA3_SET_MD(uname, typ)                                                    \
    return ctx;                                                                \
}

#define KMAC_newctx(uname, bitlen, pad)                                        \
static OSSL_FUNC_digest_newctx_fn uname##_newctx;                              \
static void *uname##_newctx(void *provctx)                                     \
{                                                                              \
    KECCAK1600_CTX *ctx = ossl_prov_is_running() ? OPENSSL_zalloc(sizeof(*ctx)) \
                                                : NULL;                        \
                                                                               \
    if (ctx == NULL)                                                           \
        return NULL;                                                           \
    ossl_keccak_kmac_init(ctx, pad, bitlen);                                   \
    ctx->meth = sha3_generic_md;                                               \
    return ctx;                                                                \
}

#define PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags)   \
PROV_FUNC_DIGEST_GET_PARAM(name, blksize, dgstsize, flags)                     \
const OSSL_DISPATCH ossl_##name##_functions[] = {                              \
    { OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))name##_newctx },                \
    { OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))keccak_update },                \
    { OSSL_FUNC_DIGEST_FINAL, (void (*)(void))keccak_final },                  \
    { OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))keccak_freectx },              \
    { OSSL_FUNC_DIGEST_DUPCTX, (void (*)(void))keccak_dupctx },                \
    PROV_DISPATCH_FUNC_DIGEST_GET_PARAMS(name)

#define PROV_FUNC_SHA3_DIGEST(name, bitlen, blksize, dgstsize, flags)          \
    PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags),      \
    { OSSL_FUNC_DIGEST_INIT, (void (*)(void))keccak_init },                    \
    PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END

#define PROV_FUNC_SHAKE_DIGEST(name, bitlen, blksize, dgstsize, flags)         \
    PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags),      \
    { OSSL_FUNC_DIGEST_INIT, (void (*)(void))keccak_init_params },             \
    { OSSL_FUNC_DIGEST_SET_CTX_PARAMS, (void (*)(void))shake_set_ctx_params }, \
    { OSSL_FUNC_DIGEST_SETTABLE_CTX_PARAMS,                                    \
     (void (*)(void))shake_settable_ctx_params },                              \
    PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END

static void keccak_freectx(void *vctx)
{
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)vctx;

    OPENSSL_clear_free(ctx,  sizeof(*ctx));
}

static void *keccak_dupctx(void *ctx)
{
    KECCAK1600_CTX *in = (KECCAK1600_CTX *)ctx;
    KECCAK1600_CTX *ret = ossl_prov_is_running() ? OPENSSL_malloc(sizeof(*ret))
                                                : NULL;

    if (ret != NULL)
        *ret = *in;
    return ret;
}

static const OSSL_PARAM known_shake_settable_ctx_params[] = {
    {OSSL_DIGEST_PARAM_XOFLEN, OSSL_PARAM_UNSIGNED_INTEGER, NULL, 0, 0},
    OSSL_PARAM_END
};
static const OSSL_PARAM *shake_settable_ctx_params(ossl_unused void *ctx,
                                                   ossl_unused void *provctx)
{
    return known_shake_settable_ctx_params;
}

static int shake_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    const OSSL_PARAM *p;
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)vctx;

    if (ctx == NULL)
        return 0;
    if (params == NULL)
        return 1;

    p = OSSL_PARAM_locate_const(params, OSSL_DIGEST_PARAM_XOFLEN);
    if (p != NULL && !OSSL_PARAM_get_size_t(p, &ctx->md_size)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }
    return 1;
}

#define IMPLEMENT_SHA3_functions(bitlen)                                       \
    SHA3_newctx(sha3, SHA3_##bitlen, sha3_##bitlen, bitlen, '\x06')            \
    PROV_FUNC_SHA3_DIGEST(sha3_##bitlen, bitlen,                               \
                          SHA3_BLOCKSIZE(bitlen), SHA3_MDSIZE(bitlen),         \
                          SHA3_FLAGS)

#define IMPLEMENT_SHAKE_functions(bitlen)                                      \
    SHA3_newctx(shake, SHAKE_##bitlen, shake_##bitlen, bitlen, '\x1f')         \
    PROV_FUNC_SHAKE_DIGEST(shake_##bitlen, bitlen,                             \
                          SHA3_BLOCKSIZE(bitlen), SHA3_MDSIZE(bitlen),         \
                          SHAKE_FLAGS)
#define IMPLEMENT_KMAC_functions(bitlen)                                       \
    KMAC_newctx(keccak_kmac_##bitlen, bitlen, '\x04')                          \
    PROV_FUNC_SHAKE_DIGEST(keccak_kmac_##bitlen, bitlen,                       \
                           SHA3_BLOCKSIZE(bitlen), KMAC_MDSIZE(bitlen),        \
                           KMAC_FLAGS)

/* ossl_sha3_224_functions */
IMPLEMENT_SHA3_functions(224)
/* ossl_sha3_256_functions */
IMPLEMENT_SHA3_functions(256)
/* ossl_sha3_384_functions */
IMPLEMENT_SHA3_functions(384)
/* ossl_sha3_512_functions */
IMPLEMENT_SHA3_functions(512)
/* ossl_shake_128_functions */
IMPLEMENT_SHAKE_functions(128)
/* ossl_shake_256_functions */
IMPLEMENT_SHAKE_functions(256)
/* ossl_keccak_kmac_128_functions */
IMPLEMENT_KMAC_functions(128)
/* ossl_keccak_kmac_256_functions */
IMPLEMENT_KMAC_functions(256)
