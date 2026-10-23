/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/byteorder.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/proverr.h>
#include "internal/cryptlib.h"
#include "internal/numbers.h"
#include "internal/sha3.h"
#include "prov/digestcommon.h"
#include "prov/implementations.h"
#include "internal/common.h"
#include "providers/implementations/digests/sha3_prov.inc"

#define SHA3_FLAGS PROV_DIGEST_FLAG_ALGID_ABSENT
#define SHAKE_FLAGS (PROV_DIGEST_FLAG_XOF | PROV_DIGEST_FLAG_ALGID_ABSENT)
#define CSHAKE_KECCAK_FLAGS PROV_DIGEST_FLAG_XOF

/*
 * FIPS 202 Section 5.1 Specifies a padding mode that is added to the last
 * block that consists of a 1 bit followed by padding zero bits and a trailing
 * 1 bit (where the bits are in LSB order)
 *
 * For a given input message special algorithm context bits are appended:
 * i.e.
 *   KECCAK[c] = (No tag is used)
 *   SHA3   = 01
 *   SHAKE  = 1111
 *   CSHAKE_KECCAK = 00 (See NIST SP800-185 3.3 : i.e. it has 2 trailing zero bits)
 * Note that KMAC and TupleHash use CSHAKE_KECCAK.
 * The OpenSSL implementation only allows input messages that are in bytes,
 * so the above concatenated bits will start on a byte boundary.
 * Following these bits will be a 1 bit then the padding zeros which gives
 *
 *   KECCAK[c] = 1000
 *   SHA3   = 0110
 *   SHAKE  = 11111000
 *   CSHAKE_KECCAK = 0010 (See NIST SP800-185 3.3 : i.e. KMAC uses cSHAKE with a fixed string)
 *
 *   Which gives the following padding values as bytes.
 */
#define KECCAK_PADDING 0x01
#define SHA3_PADDING 0x06
#define SHAKE_PADDING 0x1f
#define CSHAKE_KECCAK_PADDING 0x04

#if defined(OPENSSL_CPUID_OBJ) && defined(__s390__) && defined(KECCAK1600_ASM)
/*
 * IBM S390X support
 */
#include "arch/s390x_arch.h"
#define S390_SHA3 1
#define S390_SHA3_CAPABLE(name) \
    ((OPENSSL_s390xcap_P.kimd[0] & S390X_CAPBIT(S390X_##name)) && (OPENSSL_s390xcap_P.klmd[0] & S390X_CAPBIT(S390X_##name)))
#endif

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
static OSSL_FUNC_digest_copyctx_fn keccak_copyctx;
static OSSL_FUNC_digest_dupctx_fn keccak_dupctx;
static OSSL_FUNC_digest_squeeze_fn shake_squeeze;

static OSSL_FUNC_digest_get_ctx_params_fn shake_get_ctx_params;
static OSSL_FUNC_digest_gettable_ctx_params_fn shake_gettable_ctx_params;
static OSSL_FUNC_digest_set_ctx_params_fn shake_set_ctx_params;
static OSSL_FUNC_digest_settable_ctx_params_fn shake_settable_ctx_params;

static PROV_SHA3_METHOD sha3_generic_md = {
    ossl_sha3_absorb_default,
    ossl_sha3_final_default,
    NULL
};

static PROV_SHA3_METHOD shake_generic_md = {
    ossl_sha3_absorb_default,
    ossl_sha3_final_default,
    ossl_shake_squeeze_default
};

static int keccak_init(void *vctx, ossl_unused const OSSL_PARAM params[])
{
    if (ossl_unlikely(!ossl_prov_is_running()))
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
    return ossl_sha3_absorb((KECCAK1600_CTX *)vctx, inp, len);
}

static int keccak_final(void *vctx, unsigned char *out, size_t *outl,
    size_t outlen)
{
    int ret = 1;
    KECCAK1600_CTX *ctx = vctx;

    if (ossl_unlikely(!ossl_prov_is_running()))
        return 0;
    if (ossl_unlikely(ctx->md_size == SIZE_MAX)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_INVALID_DIGEST_LENGTH);
        return 0;
    }
    ret = ossl_sha3_final(ctx, out, ctx->md_size);
    *outl = ctx->md_size;
    return ret;
}

static int shake_squeeze(void *vctx, unsigned char *out, size_t *outl,
    size_t outlen)
{
    int ret = 1;
    KECCAK1600_CTX *ctx = vctx;

    if (!ossl_prov_is_running())
        return 0;
    if (ctx->meth.squeeze == NULL)
        return 0;
    if (outlen > 0)
        ret = ossl_sha3_squeeze(ctx, out, outlen);
    if (outl != NULL)
        *outl = outlen;
    return ret;
}

#if defined(S390_SHA3)

static sha3_absorb_fn s390x_sha3_absorb;
static sha3_final_fn s390x_sha3_final;
static sha3_final_fn s390x_shake_final;

/*-
 * The platform specific parts of the absorb() and final() for S390X.
 */
static size_t s390x_sha3_absorb(KECCAK1600_CTX *ctx, const unsigned char *inp, size_t len)
{
    size_t rem = len % ctx->block_size;
    unsigned int fc;

    if (len - rem > 0) {
        fc = ctx->pad;
        fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KIMD_NIP : 0;
        s390x_kimd(inp, len - rem, fc, ctx->A);
    }
    return rem;
}

static int s390x_sha3_final(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    unsigned int fc;

    fc = ctx->pad | S390X_KLMD_DUFOP;
    fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KLMD_NIP : 0;
    s390x_klmd(ctx->buf, ctx->bufsz, NULL, 0, fc, ctx->A);
    memcpy(out, ctx->A, outlen);
    return 1;
}

static int s390x_shake_final(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    unsigned int fc;

    fc = ctx->pad | S390X_KLMD_DUFOP;
    fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KLMD_NIP : 0;
    s390x_klmd(ctx->buf, ctx->bufsz, out, outlen, fc, ctx->A);
    return 1;
}

static int s390x_shake_squeeze(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    unsigned int fc;
    size_t len;

    /*
     * On the first squeeze call, finish the absorb process (incl. padding).
     */
    if (ctx->xof_state != XOF_STATE_SQUEEZE) {
        fc = ctx->pad;
        fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KLMD_NIP : 0;
        s390x_klmd(ctx->buf, ctx->bufsz, out, outlen, fc, ctx->A);
        ctx->bufsz = outlen % ctx->block_size;
        /* reuse ctx->bufsz to count bytes squeezed from current sponge */
        return 1;
    }
    if (ctx->bufsz != 0) {
        len = ctx->block_size - ctx->bufsz;
        if (outlen < len)
            len = outlen;
        memcpy(out, (char *)ctx->A + ctx->bufsz, len);
        out += len;
        outlen -= len;
        ctx->bufsz += len;
        if (ctx->bufsz == ctx->block_size)
            ctx->bufsz = 0;
    }
    if (outlen == 0)
        return 1;
    s390x_klmd(NULL, 0, out, outlen, ctx->pad | S390X_KLMD_PS, ctx->A);
    ctx->bufsz = outlen % ctx->block_size;

    return 1;
}

static int s390x_keccakc_final(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen,
    int padding)
{
    size_t bsz = ctx->block_size;
    size_t num = ctx->bufsz;
    size_t needed = outlen;
    unsigned int fc;

    fc = ctx->pad;
    fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KIMD_NIP : 0;
    if (outlen == 0)
        return 1;
    memset(ctx->buf + num, 0, bsz - num);
    ctx->buf[num] = padding;
    ctx->buf[bsz - 1] |= 0x80;
    s390x_kimd(ctx->buf, bsz, fc, ctx->A);
    num = needed > bsz ? bsz : needed;
    memcpy(out, ctx->A, num);
    needed -= num;
    if (needed > 0)
        s390x_klmd(NULL, 0, out + bsz, needed,
            ctx->pad | S390X_KLMD_PS | S390X_KLMD_DUFOP, ctx->A);

    return 1;
}

static int s390x_keccak_final(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    return s390x_keccakc_final(ctx, out, outlen, 0x01);
}

static int s390x_cshake_keccak_final(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    return s390x_keccakc_final(ctx, out, outlen, 0x04);
}

static int s390x_keccakc_squeeze(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen,
    int padding)
{
    size_t len;
    unsigned int fc;

    /*
     * On the first squeeze call, finish the absorb process
     * by adding the trailing padding and then doing
     * a final absorb.
     */
    if (ctx->xof_state != XOF_STATE_SQUEEZE) {
        len = ctx->block_size - ctx->bufsz;
        memset(ctx->buf + ctx->bufsz, 0, len);
        ctx->buf[ctx->bufsz] = padding;
        ctx->buf[ctx->block_size - 1] |= 0x80;
        fc = ctx->pad;
        fc |= ctx->xof_state == XOF_STATE_INIT ? S390X_KIMD_NIP : 0;
        s390x_kimd(ctx->buf, ctx->block_size, fc, ctx->A);
        ctx->bufsz = 0;
        /* reuse ctx->bufsz to count bytes squeezed from current sponge */
    }
    if (ctx->bufsz != 0 || ctx->xof_state != XOF_STATE_SQUEEZE) {
        len = ctx->block_size - ctx->bufsz;
        if (outlen < len)
            len = outlen;
        memcpy(out, (char *)ctx->A + ctx->bufsz, len);
        out += len;
        outlen -= len;
        ctx->bufsz += len;
        if (ctx->bufsz == ctx->block_size)
            ctx->bufsz = 0;
    }
    if (outlen == 0)
        return 1;
    s390x_klmd(NULL, 0, out, outlen, ctx->pad | S390X_KLMD_PS, ctx->A);
    ctx->bufsz = outlen % ctx->block_size;

    return 1;
}

static int s390x_keccak_squeeze(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    return s390x_keccakc_squeeze(ctx, out, outlen, KECCAK_PADDING);
}

static int s390x_cshake_keccak_squeeze(KECCAK1600_CTX *ctx, unsigned char *out, size_t outlen)
{
    return s390x_keccakc_squeeze(ctx, out, outlen, CSHAKE_KECCAK_PADDING);
}

static PROV_SHA3_METHOD sha3_s390x_md = {
    s390x_sha3_absorb,
    s390x_sha3_final,
    NULL,
};

static PROV_SHA3_METHOD keccak_s390x_md = {
    s390x_sha3_absorb,
    s390x_keccak_final,
    s390x_keccak_squeeze,
};

static PROV_SHA3_METHOD shake_s390x_md = {
    s390x_sha3_absorb,
    s390x_shake_final,
    s390x_shake_squeeze,
};

static PROV_SHA3_METHOD cshake_keccak_s390x_md = {
    s390x_sha3_absorb,
    s390x_cshake_keccak_final,
    s390x_cshake_keccak_squeeze,
};

#define SHAKE_SET_MD(uname, typ)      \
    if (S390_SHA3_CAPABLE(uname)) {   \
        ctx->pad = S390X_##uname;     \
        ctx->meth = typ##_s390x_md;   \
    } else {                          \
        ctx->meth = shake_generic_md; \
    }

#define SHA3_SET_MD(uname, typ)      \
    if (S390_SHA3_CAPABLE(uname)) {  \
        ctx->pad = S390X_##uname;    \
        ctx->meth = typ##_s390x_md;  \
    } else {                         \
        ctx->meth = sha3_generic_md; \
    }
#define CSHAKE_KECCAK_SET_MD(bitlen)         \
    if (S390_SHA3_CAPABLE(SHAKE_##bitlen)) { \
        ctx->pad = S390X_SHAKE_##bitlen;     \
        ctx->meth = cshake_keccak_s390x_md;  \
    } else {                                 \
        ctx->meth = shake_generic_md;        \
    }
#elif defined(__aarch64__) && defined(KECCAK1600_ASM)
#include "arch/arm_arch.h"

static sha3_absorb_fn armsha3_sha3_absorb;

size_t SHA3_absorb_cext(uint64_t A[5][5], const unsigned char *inp, size_t len,
    size_t r);
/*-
 * Hardware-assisted ARMv8.2 SHA3 extension version of the absorb()
 */
static size_t armsha3_sha3_absorb(KECCAK1600_CTX *ctx, const unsigned char *inp, size_t len)
{
    return SHA3_absorb_cext(ctx->A, inp, len, ctx->block_size);
}

static PROV_SHA3_METHOD sha3_ARMSHA3_md = {
    armsha3_sha3_absorb,
    ossl_sha3_final_default,
    NULL
};
static PROV_SHA3_METHOD shake_ARMSHA3_md = {
    armsha3_sha3_absorb,
    ossl_sha3_final_default,
    ossl_shake_squeeze_default
};
#define SHAKE_SET_MD(uname, typ)                              \
    if (OPENSSL_armcap_P & ARMV8_HAVE_SHA3_AND_WORTH_USING) { \
        ctx->meth = shake_ARMSHA3_md;                         \
    } else {                                                  \
        ctx->meth = shake_generic_md;                         \
    }

#define SHA3_SET_MD(uname, typ)                               \
    if (OPENSSL_armcap_P & ARMV8_HAVE_SHA3_AND_WORTH_USING) { \
        ctx->meth = sha3_ARMSHA3_md;                          \
    } else {                                                  \
        ctx->meth = sha3_generic_md;                          \
    }
#define CSHAKE_KECCAK_SET_MD(bitlen)                          \
    if (OPENSSL_armcap_P & ARMV8_HAVE_SHA3_AND_WORTH_USING) { \
        ctx->meth = shake_ARMSHA3_md;                         \
    } else {                                                  \
        ctx->meth = shake_generic_md;                         \
    }
#else
#define SHA3_SET_MD(uname, typ) ctx->meth = sha3_generic_md;
#define CSHAKE_KECCAK_SET_MD(bitlen) ctx->meth = shake_generic_md;
#define SHAKE_SET_MD(uname, typ) ctx->meth = shake_generic_md;
#endif /* S390_SHA3 */

#define SHA3_newctx(typ, uname, name, bitlen, pad)        \
    static OSSL_FUNC_digest_newctx_fn name##_newctx;      \
    static void *name##_newctx(void *provctx)             \
    {                                                     \
        KECCAK1600_CTX *ctx;                              \
                                                          \
        DIGEST_PROV_CHECK(provctx, SHA3_256);             \
        if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL) \
            return NULL;                                  \
        ossl_sha3_init(ctx, pad, bitlen);                 \
        SHA3_SET_MD(uname, typ)                           \
        return ctx;                                       \
    }

#define SHAKE_newctx(typ, uname, name, bitlen, mdlen, pad) \
    static OSSL_FUNC_digest_newctx_fn name##_newctx;       \
    static void *name##_newctx(void *provctx)              \
    {                                                      \
        KECCAK1600_CTX *ctx;                               \
                                                           \
        DIGEST_PROV_CHECK(provctx, SHA3_256);              \
        if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL)  \
            return NULL;                                   \
        ossl_keccak_init(ctx, pad, bitlen, mdlen);         \
        if (mdlen == 0)                                    \
            ctx->md_size = SIZE_MAX;                       \
        SHAKE_SET_MD(uname, typ)                           \
        return ctx;                                        \
    }

#define CSHAKE_KECCAK_newctx(uname, bitlen, pad)          \
    static OSSL_FUNC_digest_newctx_fn uname##_newctx;     \
    static void *uname##_newctx(void *provctx)            \
    {                                                     \
        KECCAK1600_CTX *ctx;                              \
                                                          \
        DIGEST_PROV_CHECK(provctx, SHA3_256);             \
        if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL) \
            return NULL;                                  \
        ossl_keccak_init(ctx, pad, bitlen, 2 * bitlen);   \
        CSHAKE_KECCAK_SET_MD(bitlen)                      \
        return ctx;                                       \
    }

#define KMAC_newctx(uname, bitlen, pad)                   \
    static OSSL_FUNC_digest_newctx_fn uname##_newctx;     \
    static void *uname##_newctx(void *provctx)            \
    {                                                     \
        KECCAK1600_CTX *ctx;                              \
                                                          \
        DIGEST_PROV_CHECK(provctx, SHA3_256);             \
        if ((ctx = OPENSSL_zalloc(sizeof(*ctx))) == NULL) \
            return NULL;                                  \
        ossl_keccak_init(ctx, pad, bitlen, 2 * bitlen);   \
        KMAC_SET_MD(bitlen)                               \
        return ctx;                                       \
    }

#define PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags)  \
    PROV_FUNC_DIGEST_GET_PARAM(name, blksize, dgstsize, flags)                \
    const OSSL_DISPATCH ossl_##name##_functions[] = {                         \
        { OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))name##_newctx },           \
        { OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))keccak_update },           \
        { OSSL_FUNC_DIGEST_FINAL, (void (*)(void))keccak_final },             \
        { OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))keccak_freectx },         \
        { OSSL_FUNC_DIGEST_DUPCTX, (void (*)(void))keccak_dupctx },           \
        { OSSL_FUNC_DIGEST_COPYCTX, (void (*)(void))keccak_copyctx },         \
        { OSSL_FUNC_DIGEST_SERIALIZE, (void (*)(void))name##_serialize },     \
        { OSSL_FUNC_DIGEST_DESERIALIZE, (void (*)(void))name##_deserialize }, \
        PROV_DISPATCH_FUNC_DIGEST_GET_PARAMS(name)

#ifdef FIPS_MODULE
#define PROV_SHA3_FIPS_GET_CTX_PARAMS \
    PROV_DISPATCH_FUNC_DIGEST_GET_CTX_PARAMS,
#else
#define PROV_SHA3_FIPS_GET_CTX_PARAMS
#endif

#define PROV_FUNC_SHA3_DIGEST(name, bitlen, blksize, dgstsize, flags)     \
    PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags), \
        { OSSL_FUNC_DIGEST_INIT, (void (*)(void))keccak_init },           \
        PROV_SHA3_FIPS_GET_CTX_PARAMS                                     \
            PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END

#define PROV_FUNC_SHAKE_DIGEST(name, bitlen, blksize, dgstsize, flags)             \
    PROV_FUNC_SHA3_DIGEST_COMMON(name, bitlen, blksize, dgstsize, flags),          \
        { OSSL_FUNC_DIGEST_SQUEEZE, (void (*)(void))shake_squeeze },               \
        { OSSL_FUNC_DIGEST_INIT, (void (*)(void))keccak_init_params },             \
        { OSSL_FUNC_DIGEST_SET_CTX_PARAMS, (void (*)(void))shake_set_ctx_params }, \
        { OSSL_FUNC_DIGEST_SETTABLE_CTX_PARAMS,                                    \
            (void (*)(void))shake_settable_ctx_params },                           \
        { OSSL_FUNC_DIGEST_GET_CTX_PARAMS, (void (*)(void))shake_get_ctx_params }, \
        { OSSL_FUNC_DIGEST_GETTABLE_CTX_PARAMS,                                    \
            (void (*)(void))shake_gettable_ctx_params },                           \
        PROV_DISPATCH_FUNC_DIGEST_CONSTRUCT_END

static void keccak_freectx(void *vctx)
{
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)vctx;

    OPENSSL_clear_free(ctx, sizeof(*ctx));
}

static void keccak_copyctx(void *voutctx, void *vinctx)
{
    KECCAK1600_CTX *outctx = (KECCAK1600_CTX *)voutctx;
    KECCAK1600_CTX *inctx = (KECCAK1600_CTX *)vinctx;

    *outctx = *inctx;
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

static const unsigned char keccakmagic[] = "KECCAKv1";
#define KECCAKMAGIC_LEN (sizeof(keccakmagic) - 1)
#define KECCAK_SERIALIZATION_LEN                                                     \
    (                                                                                \
        KECCAKMAGIC_LEN /* magic string */                                           \
        + sizeof(uint64_t) /* impl-ID */                                             \
        + sizeof(uint64_t) /* c->md_size */                                          \
        + (sizeof(uint64_t) * 4) /* c->block_size, c->bufsz, c->pad, c->xof_state */ \
        + (sizeof(uint64_t) * 5 * 5) /* c->A */                                      \
        + (KECCAK1600_WIDTH / 8 - 32) /* c->buf */                                   \
    )

static int KECCAK_Serialize(KECCAK1600_CTX *c, int impl_id,
    unsigned char *output, size_t *outlen)
{
    unsigned char *p;
    int i, j;

    if (output == NULL) {
        if (outlen == NULL)
            return 0;

        *outlen = KECCAK_SERIALIZATION_LEN;
        return 1;
    }

    if (outlen != NULL && *outlen < KECCAK_SERIALIZATION_LEN)
        return 0;

    p = output;

    /* Magic code */
    memcpy(p, keccakmagic, KECCAKMAGIC_LEN);
    p += KECCAKMAGIC_LEN;

    /* Additional check data */
    p = OPENSSL_store_u64_le(p, impl_id);
    p = OPENSSL_store_u64_le(p, c->md_size);

    p = OPENSSL_store_u64_le(p, c->block_size);
    p = OPENSSL_store_u64_le(p, c->bufsz);
    p = OPENSSL_store_u64_le(p, c->pad);
    p = OPENSSL_store_u64_le(p, c->xof_state);

    /* A matrix */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++)
            p = OPENSSL_store_u64_le(p, c->A[i][j]);
    }

    if (outlen != NULL)
        *outlen = KECCAK_SERIALIZATION_LEN;

    /* buf */
    memcpy(p, c->buf, sizeof(c->buf));

    return 1;
}

/*
 * This function only performs basic input sanity checks and is not
 * built to handle malicious input data. Only trusted input should be
 * fed to this function
 */
static int KECCAK_Deserialize(KECCAK1600_CTX *c, int impl_id,
    const unsigned char *input, size_t len)
{
    const unsigned char *p;
    uint64_t val;
    int i, j;

    if (c == NULL || input == NULL || len != KECCAK_SERIALIZATION_LEN)
        return 0;

    /* Magic code */
    if (memcmp(input, keccakmagic, KECCAKMAGIC_LEN) != 0)
        return 0;

    p = input + KECCAKMAGIC_LEN;

    /* Check for matching Impl ID */
    p = OPENSSL_load_u64_le(&val, p);
    if (val != (uint64_t)impl_id)
        return 0;

    /* Check for matching md_size */
    p = OPENSSL_load_u64_le(&val, p);
    if (val != (uint64_t)c->md_size)
        return 0;

    /* check that block_size is congruent with the initialized value */
    p = OPENSSL_load_u64_le(&val, p);
    if (val != c->block_size)
        return 0;
    /* check that bufsz does not exceed block_size */
    p = OPENSSL_load_u64_le(&val, p);
    if (val > c->block_size)
        return 0;
    c->bufsz = (size_t)val;
    p = OPENSSL_load_u64_le(&val, p);
    if (val != c->pad)
        return 0;
    p = OPENSSL_load_u64_le(&val, p);
    c->xof_state = (int)val;

    /* A matrix */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            p = OPENSSL_load_u64_le(&val, p);
            c->A[i][j] = val;
        }
    }

    /* buf */
    memcpy(c->buf, p, sizeof(c->buf));

    return 1;
}

#define IMPLEMENT_SERIALIZE_FNS(name, id)                              \
    static int name##_serialize(void *vctx, unsigned char *out,        \
        size_t *outlen)                                                \
    {                                                                  \
        return KECCAK_Serialize(vctx, id, out, outlen);                \
    }                                                                  \
    static int name##_deserialize(void *vctx, const unsigned char *in, \
        size_t inlen)                                                  \
    {                                                                  \
        return KECCAK_Deserialize(vctx, id, in, inlen);                \
    }

static const OSSL_PARAM *shake_gettable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return shake_get_ctx_params_list;
}

static int shake_get_ctx_params(void *vctx, OSSL_PARAM params[])
{
    struct shake_get_ctx_params_st p;
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)vctx;

    if (ctx == NULL || !shake_get_ctx_params_decoder(params, &p))
        return 0;

    if (p.xoflen != NULL && !OSSL_PARAM_set_size_t(p.xoflen, ctx->md_size)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }
    /* Size is an alias of xoflen but separate them for compatibility */
    if (p.size != NULL && !OSSL_PARAM_set_size_t(p.size, ctx->md_size)) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }
    return 1;
}

static const OSSL_PARAM *shake_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return shake_set_ctx_params_list;
}

static int shake_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    struct shake_set_ctx_params_st p;
    KECCAK1600_CTX *ctx = (KECCAK1600_CTX *)vctx;

    if (ossl_unlikely(ctx == NULL || !shake_set_ctx_params_decoder(params, &p)))
        return 0;

    if (ossl_unlikely(p.xoflen != NULL
            && !OSSL_PARAM_get_size_t(p.xoflen, &ctx->md_size))) {
        ERR_raise(ERR_LIB_PROV, PROV_R_FAILED_TO_GET_PARAMETER);
        return 0;
    }
    return 1;
}

#define KECCAK_SER_ID 0x010000
#define SHAKE_SER_ID 0x020000
#define SHA3_SER_ID 0x040000
#define CSHAKE_KECCAK_SER_ID 0x080000

#define IMPLEMENT_SHA3_functions(bitlen)                                           \
    SHA3_newctx(sha3, SHA3_##bitlen, sha3_##bitlen, bitlen, (uint8_t)SHA3_PADDING) \
        IMPLEMENT_SERIALIZE_FNS(sha3_##bitlen, SHA3_SER_ID + bitlen)               \
            PROV_FUNC_SHA3_DIGEST(sha3_##bitlen, bitlen,                           \
                SHA3_BLOCKSIZE(bitlen), SHA3_MDSIZE(bitlen),                       \
                SHA3_FLAGS)

#define IMPLEMENT_KECCAK_functions(bitlen)                                                 \
    SHA3_newctx(keccak, KECCAK_##bitlen, keccak_##bitlen, bitlen, (uint8_t)KECCAK_PADDING) \
        IMPLEMENT_SERIALIZE_FNS(keccak_##bitlen, KECCAK_SER_ID + bitlen)                   \
            PROV_FUNC_SHA3_DIGEST(keccak_##bitlen, bitlen,                                 \
                SHA3_BLOCKSIZE(bitlen), SHA3_MDSIZE(bitlen),                               \
                SHA3_FLAGS)

#define IMPLEMENT_SHAKE_functions(bitlen)                              \
    SHAKE_newctx(shake, SHAKE_##bitlen, shake_##bitlen, bitlen,        \
        0 /* no default md length */, (uint8_t)SHAKE_PADDING)          \
        IMPLEMENT_SERIALIZE_FNS(shake_##bitlen, SHAKE_SER_ID + bitlen) \
            PROV_FUNC_SHAKE_DIGEST(shake_##bitlen, bitlen,             \
                SHA3_BLOCKSIZE(bitlen), 0,                             \
                SHAKE_FLAGS)

#define IMPLEMENT_CSHAKE_KECCAK_functions(bitlen)                                        \
    CSHAKE_KECCAK_newctx(cshake_keccak_##bitlen, bitlen, (uint8_t)CSHAKE_KECCAK_PADDING) \
        IMPLEMENT_SERIALIZE_FNS(cshake_keccak_##bitlen, CSHAKE_KECCAK_SER_ID + bitlen)   \
            PROV_FUNC_SHAKE_DIGEST(cshake_keccak_##bitlen, bitlen,                       \
                SHA3_BLOCKSIZE(bitlen),                                                  \
                CSHAKE_KECCAK_MDSIZE(bitlen),                                            \
                CSHAKE_KECCAK_FLAGS)

/* ossl_sha3_224_functions */
IMPLEMENT_SHA3_functions(224)
/* ossl_sha3_256_functions */
IMPLEMENT_SHA3_functions(256)
/* ossl_sha3_384_functions */
IMPLEMENT_SHA3_functions(384)
/* ossl_sha3_512_functions */
IMPLEMENT_SHA3_functions(512)
/* ossl_keccak_224_functions */
IMPLEMENT_KECCAK_functions(224)
/* ossl_keccak_256_functions */
IMPLEMENT_KECCAK_functions(256)
/* ossl_keccak_384_functions */
IMPLEMENT_KECCAK_functions(384)
/* ossl_keccak_512_functions */
IMPLEMENT_KECCAK_functions(512)
/* ossl_shake_128_functions */
IMPLEMENT_SHAKE_functions(128)
/* ossl_shake_256_functions */
IMPLEMENT_SHAKE_functions(256)
/* ossl_cshake_keccak_128_functions */
IMPLEMENT_CSHAKE_KECCAK_functions(128)
    /* ossl_cshake_keccak_256_functions */
    IMPLEMENT_CSHAKE_KECCAK_functions(256)
