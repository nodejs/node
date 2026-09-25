/*
 *  FIPS-180-2 compliant SHA-256 implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#if defined(__clang__) &&  (__clang_major__ >= 4)

/* Ideally, we would simply use MBEDTLS_ARCH_IS_ARMV8_A in the following #if,
 * but that is defined by build_info.h, and we need this block to happen first. */
#if defined(__ARM_ARCH) && (__ARM_ARCH_PROFILE == 'A')
#if __ARM_ARCH >= 8
#define MBEDTLS_SHA256_ARCH_IS_ARMV8_A
#endif
#endif

#if defined(MBEDTLS_SHA256_ARCH_IS_ARMV8_A) && !defined(__ARM_FEATURE_CRYPTO)
/* TODO: Re-consider above after https://reviews.llvm.org/D131064 merged.
 *
 * The intrinsic declaration are guarded by predefined ACLE macros in clang:
 * these are normally only enabled by the -march option on the command line.
 * By defining the macros ourselves we gain access to those declarations without
 * requiring -march on the command line.
 *
 * `arm_neon.h` is included by common.h, so we put these defines
 * at the top of this file, before any includes.
 */
#define __ARM_FEATURE_CRYPTO 1
/* See: https://arm-software.github.io/acle/main/acle.html#cryptographic-extensions
 *
 * `__ARM_FEATURE_CRYPTO` is deprecated, but we need to continue to specify it
 * for older compilers.
 */
#define __ARM_FEATURE_SHA2   1
#define MBEDTLS_ENABLE_ARM_CRYPTO_EXTENSIONS_COMPILER_FLAG
#endif

#endif /* defined(__clang__) &&  (__clang_major__ >= 4) */

/* Ensure that SIG_SETMASK is defined when -std=c99 is used. */
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "common.h"

#if defined(MBEDTLS_SHA256_C) || defined(MBEDTLS_SHA224_C)

#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#include "mbedtls/platform.h"

#if defined(MBEDTLS_ARCH_IS_ARMV8_A)

#  if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
#       if !defined(MBEDTLS_HAVE_NEON_INTRINSICS)
#           if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
#               warning "Target does not support NEON instructions"
#               undef MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT
#           else
#               error "Target does not support NEON instructions"
#           endif
#       endif
#   endif

#  if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
/* *INDENT-OFF* */

#   if !defined(__ARM_FEATURE_CRYPTO) || defined(MBEDTLS_ENABLE_ARM_CRYPTO_EXTENSIONS_COMPILER_FLAG)
#      if defined(__ARMCOMPILER_VERSION)
#        if __ARMCOMPILER_VERSION <= 6090000
#          error "Must use minimum -march=armv8-a+crypto for MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#        endif
#          pragma clang attribute push (__attribute__((target("sha2"))), apply_to=function)
#          define MBEDTLS_POP_TARGET_PRAGMA
#      elif defined(__clang__)
#        if __clang_major__ < 4
#          error "A more recent Clang is required for MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#        endif
#        pragma clang attribute push (__attribute__((target("crypto"))), apply_to=function)
#        define MBEDTLS_POP_TARGET_PRAGMA
#      elif defined(__GNUC__)
         /* FIXME: GCC 5 claims to support Armv8 Crypto Extensions, but some
          *        intrinsics are missing. Missing intrinsics could be worked around.
          */
#        if __GNUC__ < 6
#          error "A more recent GCC is required for MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#        else
#          pragma GCC push_options
#          pragma GCC target ("arch=armv8-a+crypto")
#          define MBEDTLS_POP_TARGET_PRAGMA
#        endif
#      else
#        error "Only GCC and Clang supported for MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_*"
#      endif
#    endif
/* *INDENT-ON* */

#  endif
#  if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
#    if defined(__unix__)
#      if defined(__linux__)
/* Our preferred method of detection is getauxval() */
#        include <sys/auxv.h>
/* These are not always defined via sys/auxv.h */
#        if !defined(HWCAP_SHA2)
#          define HWCAP_SHA2  (1 << 6)
#        endif
#        if !defined(HWCAP2_SHA2)
#          define HWCAP2_SHA2 (1 << 3)
#        endif
#      endif
/* Use SIGILL on Unix, and fall back to it on Linux */
#      include <signal.h>
#    endif
#  endif
#elif !defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
#  undef MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY
#  undef MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT
#endif

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
/*
 * Capability detection code comes early, so we can disable
 * MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT if no detection mechanism found
 */
#if defined(MBEDTLS_ARCH_IS_ARM64) && defined(HWCAP_SHA2)
static int mbedtls_a64_crypto_sha256_determine_support(void)
{
    return (getauxval(AT_HWCAP) & HWCAP_SHA2) ? 1 : 0;
}
#elif defined(MBEDTLS_ARCH_IS_ARM32) && defined(HWCAP2_SHA2)
static int mbedtls_a64_crypto_sha256_determine_support(void)
{
    return (getauxval(AT_HWCAP2) & HWCAP2_SHA2) ? 1 : 0;
}
#elif defined(__APPLE__)
static int mbedtls_a64_crypto_sha256_determine_support(void)
{
    return 1;
}
#elif defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <processthreadsapi.h>

static int mbedtls_a64_crypto_sha256_determine_support(void)
{
    return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) ?
           1 : 0;
}
#elif defined(__unix__) && defined(SIG_SETMASK)
/* Detection with SIGILL, setjmp() and longjmp() */
#include <signal.h>
#include <setjmp.h>

static jmp_buf return_from_sigill;

/*
 * Armv8-A SHA256 support detection via SIGILL
 */
static void sigill_handler(int signal)
{
    (void) signal;
    longjmp(return_from_sigill, 1);
}

static int mbedtls_a64_crypto_sha256_determine_support(void)
{
    struct sigaction old_action, new_action;

    sigset_t old_mask;
    if (sigprocmask(0, NULL, &old_mask)) {
        return 0;
    }

    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    new_action.sa_handler = sigill_handler;

    sigaction(SIGILL, &new_action, &old_action);

    static int ret = 0;

    if (setjmp(return_from_sigill) == 0) {         /* First return only */
        /* If this traps, we will return a second time from setjmp() with 1 */
#if defined(MBEDTLS_ARCH_IS_ARM64)
        asm volatile ("sha256h q0, q0, v0.4s" : : : "v0");
#else
        asm volatile ("sha256h.32 q0, q0, q0" : : : "q0");
#endif
        ret = 1;
    }

    sigaction(SIGILL, &old_action, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return ret;
}
#else
#warning "No mechanism to detect ARMV8_CRYPTO found, using C code only"
#undef MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT
#endif  /* HWCAP_SHA2, __APPLE__, __unix__ && SIG_SETMASK */

#endif  /* MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT */

#if !defined(MBEDTLS_SHA256_ALT)

#define SHA256_BLOCK_SIZE 64

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
                          const mbedtls_sha256_context *src)
{
    *dst = *src;
}

/*
 * SHA-256 context setup
 */
int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224)
{
#if defined(MBEDTLS_SHA224_C) && defined(MBEDTLS_SHA256_C)
    if (is224 != 0 && is224 != 1) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#elif defined(MBEDTLS_SHA256_C)
    if (is224 != 0) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#else /* defined MBEDTLS_SHA224_C only */
    if (is224 == 0) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#endif

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is224 == 0) {
#if defined(MBEDTLS_SHA256_C)
        ctx->state[0] = 0x6A09E667;
        ctx->state[1] = 0xBB67AE85;
        ctx->state[2] = 0x3C6EF372;
        ctx->state[3] = 0xA54FF53A;
        ctx->state[4] = 0x510E527F;
        ctx->state[5] = 0x9B05688C;
        ctx->state[6] = 0x1F83D9AB;
        ctx->state[7] = 0x5BE0CD19;
#endif
    } else {
#if defined(MBEDTLS_SHA224_C)
        ctx->state[0] = 0xC1059ED8;
        ctx->state[1] = 0x367CD507;
        ctx->state[2] = 0x3070DD17;
        ctx->state[3] = 0xF70E5939;
        ctx->state[4] = 0xFFC00B31;
        ctx->state[5] = 0x68581511;
        ctx->state[6] = 0x64F98FA7;
        ctx->state[7] = 0xBEFA4FA4;
#endif
    }

#if defined(MBEDTLS_SHA224_C)
    ctx->is224 = is224;
#endif

    return 0;
}

#if !defined(MBEDTLS_SHA256_PROCESS_ALT)
static const uint32_t K[] =
{
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

#endif

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)
#  define mbedtls_internal_sha256_process_many_a64_crypto mbedtls_internal_sha256_process_many
#  define mbedtls_internal_sha256_process_a64_crypto      mbedtls_internal_sha256_process
#endif

static size_t mbedtls_internal_sha256_process_many_a64_crypto(
    mbedtls_sha256_context *ctx, const uint8_t *msg, size_t len)
{
    uint32x4_t abcd = vld1q_u32(&ctx->state[0]);
    uint32x4_t efgh = vld1q_u32(&ctx->state[4]);

    size_t processed = 0;

    for (;
         len >= SHA256_BLOCK_SIZE;
         processed += SHA256_BLOCK_SIZE,
         msg += SHA256_BLOCK_SIZE,
         len -= SHA256_BLOCK_SIZE) {
        uint32x4_t tmp, abcd_prev;

        uint32x4_t abcd_orig = abcd;
        uint32x4_t efgh_orig = efgh;

        uint32x4_t sched0 = vreinterpretq_u32_u8(vld1q_u8(msg + 16 * 0));
        uint32x4_t sched1 = vreinterpretq_u32_u8(vld1q_u8(msg + 16 * 1));
        uint32x4_t sched2 = vreinterpretq_u32_u8(vld1q_u8(msg + 16 * 2));
        uint32x4_t sched3 = vreinterpretq_u32_u8(vld1q_u8(msg + 16 * 3));

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__  /* Will be true if not defined */
                                               /* Untested on BE */
        sched0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(sched0)));
        sched1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(sched1)));
        sched2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(sched2)));
        sched3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(sched3)));
#endif

        /* Rounds 0 to 3 */
        tmp = vaddq_u32(sched0, vld1q_u32(&K[0]));
        abcd_prev = abcd;
        abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
        efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

        /* Rounds 4 to 7 */
        tmp = vaddq_u32(sched1, vld1q_u32(&K[4]));
        abcd_prev = abcd;
        abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
        efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

        /* Rounds 8 to 11 */
        tmp = vaddq_u32(sched2, vld1q_u32(&K[8]));
        abcd_prev = abcd;
        abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
        efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

        /* Rounds 12 to 15 */
        tmp = vaddq_u32(sched3, vld1q_u32(&K[12]));
        abcd_prev = abcd;
        abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
        efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

        for (int t = 16; t < 64; t += 16) {
            /* Rounds t to t + 3 */
            sched0 = vsha256su1q_u32(vsha256su0q_u32(sched0, sched1), sched2, sched3);
            tmp = vaddq_u32(sched0, vld1q_u32(&K[t]));
            abcd_prev = abcd;
            abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
            efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

            /* Rounds t + 4 to t + 7 */
            sched1 = vsha256su1q_u32(vsha256su0q_u32(sched1, sched2), sched3, sched0);
            tmp = vaddq_u32(sched1, vld1q_u32(&K[t + 4]));
            abcd_prev = abcd;
            abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
            efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

            /* Rounds t + 8 to t + 11 */
            sched2 = vsha256su1q_u32(vsha256su0q_u32(sched2, sched3), sched0, sched1);
            tmp = vaddq_u32(sched2, vld1q_u32(&K[t + 8]));
            abcd_prev = abcd;
            abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
            efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);

            /* Rounds t + 12 to t + 15 */
            sched3 = vsha256su1q_u32(vsha256su0q_u32(sched3, sched0), sched1, sched2);
            tmp = vaddq_u32(sched3, vld1q_u32(&K[t + 12]));
            abcd_prev = abcd;
            abcd = vsha256hq_u32(abcd_prev, efgh, tmp);
            efgh = vsha256h2q_u32(efgh, abcd_prev, tmp);
        }

        abcd = vaddq_u32(abcd, abcd_orig);
        efgh = vaddq_u32(efgh, efgh_orig);
    }

    vst1q_u32(&ctx->state[0], abcd);
    vst1q_u32(&ctx->state[4], efgh);

    return processed;
}

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
/*
 * This function is for internal use only if we are building both C and Armv8-A
 * versions, otherwise it is renamed to be the public mbedtls_internal_sha256_process()
 */
static
#endif
int mbedtls_internal_sha256_process_a64_crypto(mbedtls_sha256_context *ctx,
                                               const unsigned char data[SHA256_BLOCK_SIZE])
{
    return (mbedtls_internal_sha256_process_many_a64_crypto(ctx, data,
                                                            SHA256_BLOCK_SIZE) ==
            SHA256_BLOCK_SIZE) ? 0 : -1;
}

#endif /* MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT || MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY */

#if defined(MBEDTLS_POP_TARGET_PRAGMA)
#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#undef MBEDTLS_POP_TARGET_PRAGMA
#endif

#if !defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
#define mbedtls_internal_sha256_process_many_c mbedtls_internal_sha256_process_many
#define mbedtls_internal_sha256_process_c      mbedtls_internal_sha256_process
#endif


#if !defined(MBEDTLS_SHA256_PROCESS_ALT) && \
    !defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)

#define  SHR(x, n) (((x) & 0xFFFFFFFF) >> (n))
#define ROTR(x, n) (SHR(x, n) | ((x) << (32 - (n))))

#define S0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^  SHR(x, 3))
#define S1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^  SHR(x, 10))

#define S2(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S3(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))

#define F0(x, y, z) (((x) & (y)) | ((z) & ((x) | (y))))
#define F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))

#define R(t)                                                        \
    (                                                               \
        local.W[t] = S1(local.W[(t) -  2]) + local.W[(t) -  7] +    \
                     S0(local.W[(t) - 15]) + local.W[(t) - 16]      \
    )

#define P(a, b, c, d, e, f, g, h, x, K)                                      \
    do                                                              \
    {                                                               \
        local.temp1 = (h) + S3(e) + F1((e), (f), (g)) + (K) + (x);    \
        local.temp2 = S2(a) + F0((a), (b), (c));                      \
        (d) += local.temp1; (h) = local.temp1 + local.temp2;        \
    } while (0)

#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)
/*
 * This function is for internal use only if we are building both C and Armv8
 * versions, otherwise it is renamed to be the public mbedtls_internal_sha256_process()
 */
static
#endif
int mbedtls_internal_sha256_process_c(mbedtls_sha256_context *ctx,
                                      const unsigned char data[SHA256_BLOCK_SIZE])
{
    struct {
        uint32_t temp1, temp2, W[64];
        uint32_t A[8];
    } local;

    unsigned int i;

    for (i = 0; i < 8; i++) {
        local.A[i] = ctx->state[i];
    }

#if defined(MBEDTLS_SHA256_SMALLER)
    for (i = 0; i < 64; i++) {
        if (i < 16) {
            local.W[i] = MBEDTLS_GET_UINT32_BE(data, 4 * i);
        } else {
            R(i);
        }

        P(local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
          local.A[5], local.A[6], local.A[7], local.W[i], K[i]);

        local.temp1 = local.A[7]; local.A[7] = local.A[6];
        local.A[6] = local.A[5]; local.A[5] = local.A[4];
        local.A[4] = local.A[3]; local.A[3] = local.A[2];
        local.A[2] = local.A[1]; local.A[1] = local.A[0];
        local.A[0] = local.temp1;
    }
#else /* MBEDTLS_SHA256_SMALLER */
    for (i = 0; i < 16; i++) {
        local.W[i] = MBEDTLS_GET_UINT32_BE(data, 4 * i);
    }

    for (i = 0; i < 16; i += 8) {
        P(local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
          local.A[5], local.A[6], local.A[7], local.W[i+0], K[i+0]);
        P(local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
          local.A[4], local.A[5], local.A[6], local.W[i+1], K[i+1]);
        P(local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
          local.A[3], local.A[4], local.A[5], local.W[i+2], K[i+2]);
        P(local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
          local.A[2], local.A[3], local.A[4], local.W[i+3], K[i+3]);
        P(local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
          local.A[1], local.A[2], local.A[3], local.W[i+4], K[i+4]);
        P(local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
          local.A[0], local.A[1], local.A[2], local.W[i+5], K[i+5]);
        P(local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
          local.A[7], local.A[0], local.A[1], local.W[i+6], K[i+6]);
        P(local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
          local.A[6], local.A[7], local.A[0], local.W[i+7], K[i+7]);
    }

    for (i = 16; i < 64; i += 8) {
        P(local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
          local.A[5], local.A[6], local.A[7], R(i+0), K[i+0]);
        P(local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
          local.A[4], local.A[5], local.A[6], R(i+1), K[i+1]);
        P(local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
          local.A[3], local.A[4], local.A[5], R(i+2), K[i+2]);
        P(local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
          local.A[2], local.A[3], local.A[4], R(i+3), K[i+3]);
        P(local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
          local.A[1], local.A[2], local.A[3], R(i+4), K[i+4]);
        P(local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
          local.A[0], local.A[1], local.A[2], R(i+5), K[i+5]);
        P(local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
          local.A[7], local.A[0], local.A[1], R(i+6), K[i+6]);
        P(local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
          local.A[6], local.A[7], local.A[0], R(i+7), K[i+7]);
    }
#endif /* MBEDTLS_SHA256_SMALLER */

    for (i = 0; i < 8; i++) {
        ctx->state[i] += local.A[i];
    }

    /* Zeroise buffers and variables to clear sensitive data from memory. */
    mbedtls_platform_zeroize(&local, sizeof(local));

    return 0;
}

#endif /* !MBEDTLS_SHA256_PROCESS_ALT && !MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY */


#if !defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY)

static size_t mbedtls_internal_sha256_process_many_c(
    mbedtls_sha256_context *ctx, const uint8_t *data, size_t len)
{
    size_t processed = 0;

    while (len >= SHA256_BLOCK_SIZE) {
        if (mbedtls_internal_sha256_process_c(ctx, data) != 0) {
            return 0;
        }

        data += SHA256_BLOCK_SIZE;
        len  -= SHA256_BLOCK_SIZE;

        processed += SHA256_BLOCK_SIZE;
    }

    return processed;
}

#endif /* !MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_ONLY */


#if defined(MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT)

static int mbedtls_a64_crypto_sha256_has_support(void)
{
    static int done = 0;
    static int supported = 0;

    if (!done) {
        supported = mbedtls_a64_crypto_sha256_determine_support();
        done = 1;
    }

    return supported;
}

static size_t mbedtls_internal_sha256_process_many(mbedtls_sha256_context *ctx,
                                                   const uint8_t *msg, size_t len)
{
    if (mbedtls_a64_crypto_sha256_has_support()) {
        return mbedtls_internal_sha256_process_many_a64_crypto(ctx, msg, len);
    } else {
        return mbedtls_internal_sha256_process_many_c(ctx, msg, len);
    }
}

int mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx,
                                    const unsigned char data[SHA256_BLOCK_SIZE])
{
    if (mbedtls_a64_crypto_sha256_has_support()) {
        return mbedtls_internal_sha256_process_a64_crypto(ctx, data);
    } else {
        return mbedtls_internal_sha256_process_c(ctx, data);
    }
}

#endif /* MBEDTLS_SHA256_USE_ARMV8_A_CRYPTO_IF_PRESENT */


/*
 * SHA-256 process buffer
 */
int mbedtls_sha256_update(mbedtls_sha256_context *ctx,
                          const unsigned char *input,
                          size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    uint32_t left;

    if (ilen == 0) {
        return 0;
    }

    left = ctx->total[0] & 0x3F;
    fill = SHA256_BLOCK_SIZE - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (uint32_t) ilen) {
        ctx->total[1]++;
    }

    if (left && ilen >= fill) {
        memcpy((void *) (ctx->buffer + left), input, fill);

        if ((ret = mbedtls_internal_sha256_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while (ilen >= SHA256_BLOCK_SIZE) {
        size_t processed =
            mbedtls_internal_sha256_process_many(ctx, input, ilen);
        if (processed < SHA256_BLOCK_SIZE) {
            return MBEDTLS_ERR_ERROR_GENERIC_ERROR;
        }

        input += processed;
        ilen  -= processed;
    }

    if (ilen > 0) {
        memcpy((void *) (ctx->buffer + left), input, ilen);
    }

    return 0;
}

/*
 * SHA-256 final digest
 */
int mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
                          unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint32_t used;
    uint32_t high, low;
    int truncated = 0;

    /*
     * Add padding: 0x80 then 0x00 until 8 bytes remain for the length
     */
    used = ctx->total[0] & 0x3F;

    ctx->buffer[used++] = 0x80;

    if (used <= 56) {
        /* Enough room for padding + length in current block */
        memset(ctx->buffer + used, 0, 56 - used);
    } else {
        /* We'll need an extra block */
        memset(ctx->buffer + used, 0, SHA256_BLOCK_SIZE - used);

        if ((ret = mbedtls_internal_sha256_process(ctx, ctx->buffer)) != 0) {
            goto exit;
        }

        memset(ctx->buffer, 0, 56);
    }

    /*
     * Add message length
     */
    high = (ctx->total[0] >> 29)
           | (ctx->total[1] <<  3);
    low  = (ctx->total[0] <<  3);

    MBEDTLS_PUT_UINT32_BE(high, ctx->buffer, 56);
    MBEDTLS_PUT_UINT32_BE(low,  ctx->buffer, 60);

    if ((ret = mbedtls_internal_sha256_process(ctx, ctx->buffer)) != 0) {
        goto exit;
    }

    /*
     * Output final state
     */
    MBEDTLS_PUT_UINT32_BE(ctx->state[0], output,  0);
    MBEDTLS_PUT_UINT32_BE(ctx->state[1], output,  4);
    MBEDTLS_PUT_UINT32_BE(ctx->state[2], output,  8);
    MBEDTLS_PUT_UINT32_BE(ctx->state[3], output, 12);
    MBEDTLS_PUT_UINT32_BE(ctx->state[4], output, 16);
    MBEDTLS_PUT_UINT32_BE(ctx->state[5], output, 20);
    MBEDTLS_PUT_UINT32_BE(ctx->state[6], output, 24);

#if defined(MBEDTLS_SHA224_C)
    truncated = ctx->is224;
#endif
    if (!truncated) {
        MBEDTLS_PUT_UINT32_BE(ctx->state[7], output, 28);
    }

    ret = 0;

exit:
    mbedtls_sha256_free(ctx);
    return ret;
}

#endif /* !MBEDTLS_SHA256_ALT */

/*
 * output = SHA-256( input buffer )
 */
int mbedtls_sha256(const unsigned char *input,
                   size_t ilen,
                   unsigned char *output,
                   int is224)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_sha256_context ctx;

#if defined(MBEDTLS_SHA224_C) && defined(MBEDTLS_SHA256_C)
    if (is224 != 0 && is224 != 1) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#elif defined(MBEDTLS_SHA256_C)
    if (is224 != 0) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#else /* defined MBEDTLS_SHA224_C only */
    if (is224 == 0) {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
#endif

    mbedtls_sha256_init(&ctx);

    if ((ret = mbedtls_sha256_starts(&ctx, is224)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_sha256_update(&ctx, input, ilen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_sha256_finish(&ctx, output)) != 0) {
        goto exit;
    }

exit:
    mbedtls_sha256_free(&ctx);

    return ret;
}

#if defined(MBEDTLS_SELF_TEST)
/*
 * FIPS-180-2 test vectors
 */
static const unsigned char sha_test_buf[3][57] =
{
    { "abc" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "" }
};

static const size_t sha_test_buflen[3] =
{
    3, 56, 1000
};

typedef const unsigned char (sha_test_sum_t)[32];

/*
 * SHA-224 test vectors
 */
#if defined(MBEDTLS_SHA224_C)
static sha_test_sum_t sha224_test_sum[] =
{
    { 0x23, 0x09, 0x7D, 0x22, 0x34, 0x05, 0xD8, 0x22,
      0x86, 0x42, 0xA4, 0x77, 0xBD, 0xA2, 0x55, 0xB3,
      0x2A, 0xAD, 0xBC, 0xE4, 0xBD, 0xA0, 0xB3, 0xF7,
      0xE3, 0x6C, 0x9D, 0xA7 },
    { 0x75, 0x38, 0x8B, 0x16, 0x51, 0x27, 0x76, 0xCC,
      0x5D, 0xBA, 0x5D, 0xA1, 0xFD, 0x89, 0x01, 0x50,
      0xB0, 0xC6, 0x45, 0x5C, 0xB4, 0xF5, 0x8B, 0x19,
      0x52, 0x52, 0x25, 0x25 },
    { 0x20, 0x79, 0x46, 0x55, 0x98, 0x0C, 0x91, 0xD8,
      0xBB, 0xB4, 0xC1, 0xEA, 0x97, 0x61, 0x8A, 0x4B,
      0xF0, 0x3F, 0x42, 0x58, 0x19, 0x48, 0xB2, 0xEE,
      0x4E, 0xE7, 0xAD, 0x67 }
};
#endif

/*
 * SHA-256 test vectors
 */
#if defined(MBEDTLS_SHA256_C)
static sha_test_sum_t sha256_test_sum[] =
{
    { 0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
      0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
      0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD },
    { 0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
      0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
      0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
      0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1 },
    { 0xCD, 0xC7, 0x6E, 0x5C, 0x99, 0x14, 0xFB, 0x92,
      0x81, 0xA1, 0xC7, 0xE2, 0x84, 0xD7, 0x3E, 0x67,
      0xF1, 0x80, 0x9A, 0x48, 0xA4, 0x97, 0x20, 0x0E,
      0x04, 0x6D, 0x39, 0xCC, 0xC7, 0x11, 0x2C, 0xD0 }
};
#endif

/*
 * Checkup routine
 */
static int mbedtls_sha256_common_self_test(int verbose, int is224)
{
    int i, buflen, ret = 0;
    unsigned char *buf;
    unsigned char sha256sum[32];
    mbedtls_sha256_context ctx;

#if defined(MBEDTLS_SHA224_C) && defined(MBEDTLS_SHA256_C)
    sha_test_sum_t *sha_test_sum = (is224) ? sha224_test_sum : sha256_test_sum;
#elif defined(MBEDTLS_SHA256_C)
    sha_test_sum_t *sha_test_sum = sha256_test_sum;
#else
    sha_test_sum_t *sha_test_sum = sha224_test_sum;
#endif

    buf = mbedtls_calloc(1024, sizeof(unsigned char));
    if (NULL == buf) {
        if (verbose != 0) {
            mbedtls_printf("Buffer allocation failed\n");
        }

        return 1;
    }

    mbedtls_sha256_init(&ctx);

    for (i = 0; i < 3; i++) {
        if (verbose != 0) {
            mbedtls_printf("  SHA-%d test #%d: ", 256 - is224 * 32, i + 1);
        }

        if ((ret = mbedtls_sha256_starts(&ctx, is224)) != 0) {
            goto fail;
        }

        if (i == 2) {
            memset(buf, 'a', buflen = 1000);

            for (int j = 0; j < 1000; j++) {
                ret = mbedtls_sha256_update(&ctx, buf, buflen);
                if (ret != 0) {
                    goto fail;
                }
            }

        } else {
            ret = mbedtls_sha256_update(&ctx, sha_test_buf[i],
                                        sha_test_buflen[i]);
            if (ret != 0) {
                goto fail;
            }
        }

        if ((ret = mbedtls_sha256_finish(&ctx, sha256sum)) != 0) {
            goto fail;
        }


        if (memcmp(sha256sum, sha_test_sum[i], 32 - is224 * 4) != 0) {
            ret = 1;
            goto fail;
        }

        if (verbose != 0) {
            mbedtls_printf("passed\n");
        }
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    goto exit;

fail:
    if (verbose != 0) {
        mbedtls_printf("failed\n");
    }

exit:
    mbedtls_sha256_free(&ctx);
    mbedtls_free(buf);

    return ret;
}

#if defined(MBEDTLS_SHA256_C)
int mbedtls_sha256_self_test(int verbose)
{
    return mbedtls_sha256_common_self_test(verbose, 0);
}
#endif /* MBEDTLS_SHA256_C */

#if defined(MBEDTLS_SHA224_C)
int mbedtls_sha224_self_test(int verbose)
{
    return mbedtls_sha256_common_self_test(verbose, 1);
}
#endif /* MBEDTLS_SHA224_C */

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SHA256_C || MBEDTLS_SHA224_C */
