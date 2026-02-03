/*
 *  FIPS-180-2 compliant SHA-384/512 implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The SHA-512 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#if defined(__aarch64__) && !defined(__ARM_FEATURE_SHA512) && \
    defined(__clang__) && __clang_major__ >= 7
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
#define __ARM_FEATURE_SHA512 1
#define MBEDTLS_ENABLE_ARM_SHA3_EXTENSIONS_COMPILER_FLAG
#endif

#include "common.h"

#if defined(MBEDTLS_SHA512_C) || defined(MBEDTLS_SHA384_C)

#include "mbedtls/sha512.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#if defined(_MSC_VER) || defined(__WATCOMC__)
  #define UL64(x) x##ui64
#else
  #define UL64(x) x##ULL
#endif

#include <string.h>

#include "mbedtls/platform.h"

#if defined(__aarch64__)
#  if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)
/* *INDENT-OFF* */
#   if !defined(MBEDTLS_HAVE_NEON_INTRINSICS)
#       error "Target does not support NEON instructions"
#   endif
/*
 * Best performance comes from most recent compilers, with intrinsics and -O3.
 * Must compile with -march=armv8.2-a+sha3, but we can't detect armv8.2-a, and
 * can't always detect __ARM_FEATURE_SHA512 (notably clang 7-12).
 *
 * GCC < 8 won't work at all (lacks the sha512 instructions)
 * GCC >= 8 uses intrinsics, sets __ARM_FEATURE_SHA512
 *
 * Clang < 7 won't work at all (lacks the sha512 instructions)
 * Clang 7-12 don't have intrinsics (but we work around that with inline
 *            assembler) or __ARM_FEATURE_SHA512
 * Clang == 13.0.0 same as clang 12 (only seen on macOS)
 * Clang >= 13.0.1 has __ARM_FEATURE_SHA512 and intrinsics
 */
#    if !defined(__ARM_FEATURE_SHA512) || defined(MBEDTLS_ENABLE_ARM_SHA3_EXTENSIONS_COMPILER_FLAG)
       /* Test Clang first, as it defines __GNUC__ */
#      if defined(__ARMCOMPILER_VERSION)
#        if __ARMCOMPILER_VERSION < 6090000
#          error "A more recent armclang is required for MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#        elif __ARMCOMPILER_VERSION == 6090000
#          error "Must use minimum -march=armv8.2-a+sha3 for MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#        else
#          pragma clang attribute push (__attribute__((target("sha3"))), apply_to=function)
#          define MBEDTLS_POP_TARGET_PRAGMA
#        endif
#      elif defined(__clang__)
#        if __clang_major__ < 7
#          error "A more recent Clang is required for MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#        else
#          pragma clang attribute push (__attribute__((target("sha3"))), apply_to=function)
#          define MBEDTLS_POP_TARGET_PRAGMA
#        endif
#      elif defined(__GNUC__)
#        if __GNUC__ < 8
#          error "A more recent GCC is required for MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#        else
#          pragma GCC push_options
#          pragma GCC target ("arch=armv8.2-a+sha3")
#          define MBEDTLS_POP_TARGET_PRAGMA
#        endif
#      else
#        error "Only GCC and Clang supported for MBEDTLS_SHA512_USE_A64_CRYPTO_*"
#      endif
#    endif
/* *INDENT-ON* */
#  endif
#  if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
#    if defined(__unix__)
#      if defined(__linux__)
/* Our preferred method of detection is getauxval() */
#        include <sys/auxv.h>
#        if !defined(HWCAP_SHA512)
/* The same header that declares getauxval() should provide the HWCAP_xxx
 * constants to analyze its return value. However, the libc may be too
 * old to have the constant that we need. So if it's missing, assume that
 * the value is the same one used by the Linux kernel ABI.
 */
#          define HWCAP_SHA512 (1 << 21)
#        endif
#      endif
/* Use SIGILL on Unix, and fall back to it on Linux */
#      include <signal.h>
#    endif
#  endif
#elif !defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
#  undef MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY
#  undef MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT
#endif

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
/*
 * Capability detection code comes early, so we can disable
 * MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT if no detection mechanism found
 */
#if defined(HWCAP_SHA512)
static int mbedtls_a64_crypto_sha512_determine_support(void)
{
    return (getauxval(AT_HWCAP) & HWCAP_SHA512) ? 1 : 0;
}
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>

static int mbedtls_a64_crypto_sha512_determine_support(void)
{
    int value = 0;
    size_t value_len = sizeof(value);

    int ret = sysctlbyname("hw.optional.armv8_2_sha512", &value, &value_len,
                           NULL, 0);
    return ret == 0 && value != 0;
}
#elif defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
/*
 * As of March 2022, there don't appear to be any PF_ARM_V8_* flags
 * available to pass to IsProcessorFeaturePresent() to check for
 * SHA-512 support. So we fall back to the C code only.
 */
#if defined(_MSC_VER)
#pragma message "No mechanism to detect A64_CRYPTO found, using C code only"
#else
#warning "No mechanism to detect A64_CRYPTO found, using C code only"
#endif
#elif defined(__unix__) && defined(SIG_SETMASK)
/* Detection with SIGILL, setjmp() and longjmp() */
#include <signal.h>
#include <setjmp.h>

static jmp_buf return_from_sigill;

/*
 * A64 SHA512 support detection via SIGILL
 */
static void sigill_handler(int signal)
{
    (void) signal;
    longjmp(return_from_sigill, 1);
}

static int mbedtls_a64_crypto_sha512_determine_support(void)
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
        asm ("sha512h q0, q0, v0.2d" : : : "v0");
        ret = 1;
    }

    sigaction(SIGILL, &old_action, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);

    return ret;
}
#else
#warning "No mechanism to detect A64_CRYPTO found, using C code only"
#undef MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT
#endif  /* HWCAP_SHA512, __APPLE__, __unix__ && SIG_SETMASK */

#endif  /* MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT */

#if !defined(MBEDTLS_SHA512_ALT)

#define SHA512_BLOCK_SIZE 128

#if defined(MBEDTLS_SHA512_SMALLER)
static void sha512_put_uint64_be(uint64_t n, unsigned char *b, uint8_t i)
{
    MBEDTLS_PUT_UINT64_BE(n, b, i);
}
#else
#define sha512_put_uint64_be    MBEDTLS_PUT_UINT64_BE
#endif /* MBEDTLS_SHA512_SMALLER */

void mbedtls_sha512_init(mbedtls_sha512_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_sha512_context));
}

void mbedtls_sha512_free(mbedtls_sha512_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha512_context));
}

void mbedtls_sha512_clone(mbedtls_sha512_context *dst,
                          const mbedtls_sha512_context *src)
{
    *dst = *src;
}

/*
 * SHA-512 context setup
 */
int mbedtls_sha512_starts(mbedtls_sha512_context *ctx, int is384)
{
#if defined(MBEDTLS_SHA384_C) && defined(MBEDTLS_SHA512_C)
    if (is384 != 0 && is384 != 1) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#elif defined(MBEDTLS_SHA512_C)
    if (is384 != 0) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#else /* defined MBEDTLS_SHA384_C only */
    if (is384 == 0) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#endif

    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is384 == 0) {
#if defined(MBEDTLS_SHA512_C)
        ctx->state[0] = UL64(0x6A09E667F3BCC908);
        ctx->state[1] = UL64(0xBB67AE8584CAA73B);
        ctx->state[2] = UL64(0x3C6EF372FE94F82B);
        ctx->state[3] = UL64(0xA54FF53A5F1D36F1);
        ctx->state[4] = UL64(0x510E527FADE682D1);
        ctx->state[5] = UL64(0x9B05688C2B3E6C1F);
        ctx->state[6] = UL64(0x1F83D9ABFB41BD6B);
        ctx->state[7] = UL64(0x5BE0CD19137E2179);
#endif /* MBEDTLS_SHA512_C */
    } else {
#if defined(MBEDTLS_SHA384_C)
        ctx->state[0] = UL64(0xCBBB9D5DC1059ED8);
        ctx->state[1] = UL64(0x629A292A367CD507);
        ctx->state[2] = UL64(0x9159015A3070DD17);
        ctx->state[3] = UL64(0x152FECD8F70E5939);
        ctx->state[4] = UL64(0x67332667FFC00B31);
        ctx->state[5] = UL64(0x8EB44A8768581511);
        ctx->state[6] = UL64(0xDB0C2E0D64F98FA7);
        ctx->state[7] = UL64(0x47B5481DBEFA4FA4);
#endif /* MBEDTLS_SHA384_C */
    }

#if defined(MBEDTLS_SHA384_C)
    ctx->is384 = is384;
#endif

    return 0;
}

#if !defined(MBEDTLS_SHA512_PROCESS_ALT)

/*
 * Round constants
 */
static const uint64_t K[80] =
{
    UL64(0x428A2F98D728AE22),  UL64(0x7137449123EF65CD),
    UL64(0xB5C0FBCFEC4D3B2F),  UL64(0xE9B5DBA58189DBBC),
    UL64(0x3956C25BF348B538),  UL64(0x59F111F1B605D019),
    UL64(0x923F82A4AF194F9B),  UL64(0xAB1C5ED5DA6D8118),
    UL64(0xD807AA98A3030242),  UL64(0x12835B0145706FBE),
    UL64(0x243185BE4EE4B28C),  UL64(0x550C7DC3D5FFB4E2),
    UL64(0x72BE5D74F27B896F),  UL64(0x80DEB1FE3B1696B1),
    UL64(0x9BDC06A725C71235),  UL64(0xC19BF174CF692694),
    UL64(0xE49B69C19EF14AD2),  UL64(0xEFBE4786384F25E3),
    UL64(0x0FC19DC68B8CD5B5),  UL64(0x240CA1CC77AC9C65),
    UL64(0x2DE92C6F592B0275),  UL64(0x4A7484AA6EA6E483),
    UL64(0x5CB0A9DCBD41FBD4),  UL64(0x76F988DA831153B5),
    UL64(0x983E5152EE66DFAB),  UL64(0xA831C66D2DB43210),
    UL64(0xB00327C898FB213F),  UL64(0xBF597FC7BEEF0EE4),
    UL64(0xC6E00BF33DA88FC2),  UL64(0xD5A79147930AA725),
    UL64(0x06CA6351E003826F),  UL64(0x142929670A0E6E70),
    UL64(0x27B70A8546D22FFC),  UL64(0x2E1B21385C26C926),
    UL64(0x4D2C6DFC5AC42AED),  UL64(0x53380D139D95B3DF),
    UL64(0x650A73548BAF63DE),  UL64(0x766A0ABB3C77B2A8),
    UL64(0x81C2C92E47EDAEE6),  UL64(0x92722C851482353B),
    UL64(0xA2BFE8A14CF10364),  UL64(0xA81A664BBC423001),
    UL64(0xC24B8B70D0F89791),  UL64(0xC76C51A30654BE30),
    UL64(0xD192E819D6EF5218),  UL64(0xD69906245565A910),
    UL64(0xF40E35855771202A),  UL64(0x106AA07032BBD1B8),
    UL64(0x19A4C116B8D2D0C8),  UL64(0x1E376C085141AB53),
    UL64(0x2748774CDF8EEB99),  UL64(0x34B0BCB5E19B48A8),
    UL64(0x391C0CB3C5C95A63),  UL64(0x4ED8AA4AE3418ACB),
    UL64(0x5B9CCA4F7763E373),  UL64(0x682E6FF3D6B2B8A3),
    UL64(0x748F82EE5DEFB2FC),  UL64(0x78A5636F43172F60),
    UL64(0x84C87814A1F0AB72),  UL64(0x8CC702081A6439EC),
    UL64(0x90BEFFFA23631E28),  UL64(0xA4506CEBDE82BDE9),
    UL64(0xBEF9A3F7B2C67915),  UL64(0xC67178F2E372532B),
    UL64(0xCA273ECEEA26619C),  UL64(0xD186B8C721C0C207),
    UL64(0xEADA7DD6CDE0EB1E),  UL64(0xF57D4F7FEE6ED178),
    UL64(0x06F067AA72176FBA),  UL64(0x0A637DC5A2C898A6),
    UL64(0x113F9804BEF90DAE),  UL64(0x1B710B35131C471B),
    UL64(0x28DB77F523047D84),  UL64(0x32CAAB7B40C72493),
    UL64(0x3C9EBE0A15C9BEBC),  UL64(0x431D67C49C100D4C),
    UL64(0x4CC5D4BECB3E42B6),  UL64(0x597F299CFC657E2A),
    UL64(0x5FCB6FAB3AD6FAEC),  UL64(0x6C44198C4A475817)
};
#endif

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT) || \
    defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)
#  define mbedtls_internal_sha512_process_many_a64_crypto mbedtls_internal_sha512_process_many
#  define mbedtls_internal_sha512_process_a64_crypto      mbedtls_internal_sha512_process
#endif

/* Accelerated SHA-512 implementation originally written by Simon Tatham for PuTTY,
 * under the MIT licence; dual-licensed as Apache 2 with his kind permission.
 */

#if defined(__clang__) && \
    (__clang_major__ < 13 || \
     (__clang_major__ == 13 && __clang_minor__ == 0 && __clang_patchlevel__ == 0))
static inline uint64x2_t vsha512su0q_u64(uint64x2_t x, uint64x2_t y)
{
    asm ("sha512su0 %0.2D,%1.2D" : "+w" (x) : "w" (y));
    return x;
}
static inline uint64x2_t vsha512su1q_u64(uint64x2_t x, uint64x2_t y, uint64x2_t z)
{
    asm ("sha512su1 %0.2D,%1.2D,%2.2D" : "+w" (x) : "w" (y), "w" (z));
    return x;
}
static inline uint64x2_t vsha512hq_u64(uint64x2_t x, uint64x2_t y, uint64x2_t z)
{
    asm ("sha512h %0,%1,%2.2D" : "+w" (x) : "w" (y), "w" (z));
    return x;
}
static inline uint64x2_t vsha512h2q_u64(uint64x2_t x, uint64x2_t y, uint64x2_t z)
{
    asm ("sha512h2 %0,%1,%2.2D" : "+w" (x) : "w" (y), "w" (z));
    return x;
}
#endif  /* __clang__ etc */

static size_t mbedtls_internal_sha512_process_many_a64_crypto(
    mbedtls_sha512_context *ctx, const uint8_t *msg, size_t len)
{
    uint64x2_t ab = vld1q_u64(&ctx->state[0]);
    uint64x2_t cd = vld1q_u64(&ctx->state[2]);
    uint64x2_t ef = vld1q_u64(&ctx->state[4]);
    uint64x2_t gh = vld1q_u64(&ctx->state[6]);

    size_t processed = 0;

    for (;
         len >= SHA512_BLOCK_SIZE;
         processed += SHA512_BLOCK_SIZE,
         msg += SHA512_BLOCK_SIZE,
         len -= SHA512_BLOCK_SIZE) {
        uint64x2_t initial_sum, sum, intermed;

        uint64x2_t ab_orig = ab;
        uint64x2_t cd_orig = cd;
        uint64x2_t ef_orig = ef;
        uint64x2_t gh_orig = gh;

        uint64x2_t s0 = (uint64x2_t) vld1q_u8(msg + 16 * 0);
        uint64x2_t s1 = (uint64x2_t) vld1q_u8(msg + 16 * 1);
        uint64x2_t s2 = (uint64x2_t) vld1q_u8(msg + 16 * 2);
        uint64x2_t s3 = (uint64x2_t) vld1q_u8(msg + 16 * 3);
        uint64x2_t s4 = (uint64x2_t) vld1q_u8(msg + 16 * 4);
        uint64x2_t s5 = (uint64x2_t) vld1q_u8(msg + 16 * 5);
        uint64x2_t s6 = (uint64x2_t) vld1q_u8(msg + 16 * 6);
        uint64x2_t s7 = (uint64x2_t) vld1q_u8(msg + 16 * 7);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__  /* assume LE if these not defined; untested on BE */
        s0 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s0)));
        s1 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s1)));
        s2 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s2)));
        s3 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s3)));
        s4 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s4)));
        s5 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s5)));
        s6 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s6)));
        s7 = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(s7)));
#endif

        /* Rounds 0 and 1 */
        initial_sum = vaddq_u64(s0, vld1q_u64(&K[0]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), gh);
        intermed = vsha512hq_u64(sum, vextq_u64(ef, gh, 1), vextq_u64(cd, ef, 1));
        gh = vsha512h2q_u64(intermed, cd, ab);
        cd = vaddq_u64(cd, intermed);

        /* Rounds 2 and 3 */
        initial_sum = vaddq_u64(s1, vld1q_u64(&K[2]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ef);
        intermed = vsha512hq_u64(sum, vextq_u64(cd, ef, 1), vextq_u64(ab, cd, 1));
        ef = vsha512h2q_u64(intermed, ab, gh);
        ab = vaddq_u64(ab, intermed);

        /* Rounds 4 and 5 */
        initial_sum = vaddq_u64(s2, vld1q_u64(&K[4]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), cd);
        intermed = vsha512hq_u64(sum, vextq_u64(ab, cd, 1), vextq_u64(gh, ab, 1));
        cd = vsha512h2q_u64(intermed, gh, ef);
        gh = vaddq_u64(gh, intermed);

        /* Rounds 6 and 7 */
        initial_sum = vaddq_u64(s3, vld1q_u64(&K[6]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ab);
        intermed = vsha512hq_u64(sum, vextq_u64(gh, ab, 1), vextq_u64(ef, gh, 1));
        ab = vsha512h2q_u64(intermed, ef, cd);
        ef = vaddq_u64(ef, intermed);

        /* Rounds 8 and 9 */
        initial_sum = vaddq_u64(s4, vld1q_u64(&K[8]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), gh);
        intermed = vsha512hq_u64(sum, vextq_u64(ef, gh, 1), vextq_u64(cd, ef, 1));
        gh = vsha512h2q_u64(intermed, cd, ab);
        cd = vaddq_u64(cd, intermed);

        /* Rounds 10 and 11 */
        initial_sum = vaddq_u64(s5, vld1q_u64(&K[10]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ef);
        intermed = vsha512hq_u64(sum, vextq_u64(cd, ef, 1), vextq_u64(ab, cd, 1));
        ef = vsha512h2q_u64(intermed, ab, gh);
        ab = vaddq_u64(ab, intermed);

        /* Rounds 12 and 13 */
        initial_sum = vaddq_u64(s6, vld1q_u64(&K[12]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), cd);
        intermed = vsha512hq_u64(sum, vextq_u64(ab, cd, 1), vextq_u64(gh, ab, 1));
        cd = vsha512h2q_u64(intermed, gh, ef);
        gh = vaddq_u64(gh, intermed);

        /* Rounds 14 and 15 */
        initial_sum = vaddq_u64(s7, vld1q_u64(&K[14]));
        sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ab);
        intermed = vsha512hq_u64(sum, vextq_u64(gh, ab, 1), vextq_u64(ef, gh, 1));
        ab = vsha512h2q_u64(intermed, ef, cd);
        ef = vaddq_u64(ef, intermed);

        for (unsigned int t = 16; t < 80; t += 16) {
            /* Rounds t and t + 1 */
            s0 = vsha512su1q_u64(vsha512su0q_u64(s0, s1), s7, vextq_u64(s4, s5, 1));
            initial_sum = vaddq_u64(s0, vld1q_u64(&K[t]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), gh);
            intermed = vsha512hq_u64(sum, vextq_u64(ef, gh, 1), vextq_u64(cd, ef, 1));
            gh = vsha512h2q_u64(intermed, cd, ab);
            cd = vaddq_u64(cd, intermed);

            /* Rounds t + 2 and t + 3 */
            s1 = vsha512su1q_u64(vsha512su0q_u64(s1, s2), s0, vextq_u64(s5, s6, 1));
            initial_sum = vaddq_u64(s1, vld1q_u64(&K[t + 2]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ef);
            intermed = vsha512hq_u64(sum, vextq_u64(cd, ef, 1), vextq_u64(ab, cd, 1));
            ef = vsha512h2q_u64(intermed, ab, gh);
            ab = vaddq_u64(ab, intermed);

            /* Rounds t + 4 and t + 5 */
            s2 = vsha512su1q_u64(vsha512su0q_u64(s2, s3), s1, vextq_u64(s6, s7, 1));
            initial_sum = vaddq_u64(s2, vld1q_u64(&K[t + 4]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), cd);
            intermed = vsha512hq_u64(sum, vextq_u64(ab, cd, 1), vextq_u64(gh, ab, 1));
            cd = vsha512h2q_u64(intermed, gh, ef);
            gh = vaddq_u64(gh, intermed);

            /* Rounds t + 6 and t + 7 */
            s3 = vsha512su1q_u64(vsha512su0q_u64(s3, s4), s2, vextq_u64(s7, s0, 1));
            initial_sum = vaddq_u64(s3, vld1q_u64(&K[t + 6]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ab);
            intermed = vsha512hq_u64(sum, vextq_u64(gh, ab, 1), vextq_u64(ef, gh, 1));
            ab = vsha512h2q_u64(intermed, ef, cd);
            ef = vaddq_u64(ef, intermed);

            /* Rounds t + 8 and t + 9 */
            s4 = vsha512su1q_u64(vsha512su0q_u64(s4, s5), s3, vextq_u64(s0, s1, 1));
            initial_sum = vaddq_u64(s4, vld1q_u64(&K[t + 8]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), gh);
            intermed = vsha512hq_u64(sum, vextq_u64(ef, gh, 1), vextq_u64(cd, ef, 1));
            gh = vsha512h2q_u64(intermed, cd, ab);
            cd = vaddq_u64(cd, intermed);

            /* Rounds t + 10 and t + 11 */
            s5 = vsha512su1q_u64(vsha512su0q_u64(s5, s6), s4, vextq_u64(s1, s2, 1));
            initial_sum = vaddq_u64(s5, vld1q_u64(&K[t + 10]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ef);
            intermed = vsha512hq_u64(sum, vextq_u64(cd, ef, 1), vextq_u64(ab, cd, 1));
            ef = vsha512h2q_u64(intermed, ab, gh);
            ab = vaddq_u64(ab, intermed);

            /* Rounds t + 12 and t + 13 */
            s6 = vsha512su1q_u64(vsha512su0q_u64(s6, s7), s5, vextq_u64(s2, s3, 1));
            initial_sum = vaddq_u64(s6, vld1q_u64(&K[t + 12]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), cd);
            intermed = vsha512hq_u64(sum, vextq_u64(ab, cd, 1), vextq_u64(gh, ab, 1));
            cd = vsha512h2q_u64(intermed, gh, ef);
            gh = vaddq_u64(gh, intermed);

            /* Rounds t + 14 and t + 15 */
            s7 = vsha512su1q_u64(vsha512su0q_u64(s7, s0), s6, vextq_u64(s3, s4, 1));
            initial_sum = vaddq_u64(s7, vld1q_u64(&K[t + 14]));
            sum = vaddq_u64(vextq_u64(initial_sum, initial_sum, 1), ab);
            intermed = vsha512hq_u64(sum, vextq_u64(gh, ab, 1), vextq_u64(ef, gh, 1));
            ab = vsha512h2q_u64(intermed, ef, cd);
            ef = vaddq_u64(ef, intermed);
        }

        ab = vaddq_u64(ab, ab_orig);
        cd = vaddq_u64(cd, cd_orig);
        ef = vaddq_u64(ef, ef_orig);
        gh = vaddq_u64(gh, gh_orig);
    }

    vst1q_u64(&ctx->state[0], ab);
    vst1q_u64(&ctx->state[2], cd);
    vst1q_u64(&ctx->state[4], ef);
    vst1q_u64(&ctx->state[6], gh);

    return processed;
}

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
/*
 * This function is for internal use only if we are building both C and A64
 * versions, otherwise it is renamed to be the public mbedtls_internal_sha512_process()
 */
static
#endif
int mbedtls_internal_sha512_process_a64_crypto(mbedtls_sha512_context *ctx,
                                               const unsigned char data[SHA512_BLOCK_SIZE])
{
    return (mbedtls_internal_sha512_process_many_a64_crypto(ctx, data,
                                                            SHA512_BLOCK_SIZE) ==
            SHA512_BLOCK_SIZE) ? 0 : -1;
}

#endif /* MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT || MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY */

#if defined(MBEDTLS_POP_TARGET_PRAGMA)
#if defined(__clang__)
#pragma clang attribute pop
#elif defined(__GNUC__)
#pragma GCC pop_options
#endif
#undef MBEDTLS_POP_TARGET_PRAGMA
#endif


#if !defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
#define mbedtls_internal_sha512_process_many_c mbedtls_internal_sha512_process_many
#define mbedtls_internal_sha512_process_c      mbedtls_internal_sha512_process
#endif


#if !defined(MBEDTLS_SHA512_PROCESS_ALT) && !defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)

#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)
/*
 * This function is for internal use only if we are building both C and A64
 * versions, otherwise it is renamed to be the public mbedtls_internal_sha512_process()
 */
static
#endif
int mbedtls_internal_sha512_process_c(mbedtls_sha512_context *ctx,
                                      const unsigned char data[SHA512_BLOCK_SIZE])
{
    int i;
    struct {
        uint64_t temp1, temp2, W[80];
        uint64_t A[8];
    } local;

#define  SHR(x, n) ((x) >> (n))
#define ROTR(x, n) (SHR((x), (n)) | ((x) << (64 - (n))))

#define S0(x) (ROTR(x, 1) ^ ROTR(x, 8) ^  SHR(x, 7))
#define S1(x) (ROTR(x, 19) ^ ROTR(x, 61) ^  SHR(x, 6))

#define S2(x) (ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39))
#define S3(x) (ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41))

#define F0(x, y, z) (((x) & (y)) | ((z) & ((x) | (y))))
#define F1(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))

#define P(a, b, c, d, e, f, g, h, x, K)                                      \
    do                                                              \
    {                                                               \
        local.temp1 = (h) + S3(e) + F1((e), (f), (g)) + (K) + (x);    \
        local.temp2 = S2(a) + F0((a), (b), (c));                      \
        (d) += local.temp1; (h) = local.temp1 + local.temp2;        \
    } while (0)

    for (i = 0; i < 8; i++) {
        local.A[i] = ctx->state[i];
    }

#if defined(MBEDTLS_SHA512_SMALLER)
    for (i = 0; i < 80; i++) {
        if (i < 16) {
            local.W[i] = MBEDTLS_GET_UINT64_BE(data, i << 3);
        } else {
            local.W[i] = S1(local.W[i -  2]) + local.W[i -  7] +
                         S0(local.W[i - 15]) + local.W[i - 16];
        }

        P(local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
          local.A[5], local.A[6], local.A[7], local.W[i], K[i]);

        local.temp1 = local.A[7]; local.A[7] = local.A[6];
        local.A[6] = local.A[5]; local.A[5] = local.A[4];
        local.A[4] = local.A[3]; local.A[3] = local.A[2];
        local.A[2] = local.A[1]; local.A[1] = local.A[0];
        local.A[0] = local.temp1;
    }
#else /* MBEDTLS_SHA512_SMALLER */
    for (i = 0; i < 16; i++) {
        local.W[i] = MBEDTLS_GET_UINT64_BE(data, i << 3);
    }

    for (; i < 80; i++) {
        local.W[i] = S1(local.W[i -  2]) + local.W[i -  7] +
                     S0(local.W[i - 15]) + local.W[i - 16];
    }

    i = 0;
    do {
        P(local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
          local.A[5], local.A[6], local.A[7], local.W[i], K[i]); i++;
        P(local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
          local.A[4], local.A[5], local.A[6], local.W[i], K[i]); i++;
        P(local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
          local.A[3], local.A[4], local.A[5], local.W[i], K[i]); i++;
        P(local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
          local.A[2], local.A[3], local.A[4], local.W[i], K[i]); i++;
        P(local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
          local.A[1], local.A[2], local.A[3], local.W[i], K[i]); i++;
        P(local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
          local.A[0], local.A[1], local.A[2], local.W[i], K[i]); i++;
        P(local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
          local.A[7], local.A[0], local.A[1], local.W[i], K[i]); i++;
        P(local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
          local.A[6], local.A[7], local.A[0], local.W[i], K[i]); i++;
    } while (i < 80);
#endif /* MBEDTLS_SHA512_SMALLER */

    for (i = 0; i < 8; i++) {
        ctx->state[i] += local.A[i];
    }

    /* Zeroise buffers and variables to clear sensitive data from memory. */
    mbedtls_platform_zeroize(&local, sizeof(local));

    return 0;
}

#endif /* !MBEDTLS_SHA512_PROCESS_ALT && !MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY */


#if !defined(MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY)

static size_t mbedtls_internal_sha512_process_many_c(
    mbedtls_sha512_context *ctx, const uint8_t *data, size_t len)
{
    size_t processed = 0;

    while (len >= SHA512_BLOCK_SIZE) {
        if (mbedtls_internal_sha512_process_c(ctx, data) != 0) {
            return 0;
        }

        data += SHA512_BLOCK_SIZE;
        len  -= SHA512_BLOCK_SIZE;

        processed += SHA512_BLOCK_SIZE;
    }

    return processed;
}

#endif /* !MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY */


#if defined(MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT)

static int mbedtls_a64_crypto_sha512_has_support(void)
{
    static int done = 0;
    static int supported = 0;

    if (!done) {
        supported = mbedtls_a64_crypto_sha512_determine_support();
        done = 1;
    }

    return supported;
}

static size_t mbedtls_internal_sha512_process_many(mbedtls_sha512_context *ctx,
                                                   const uint8_t *msg, size_t len)
{
    if (mbedtls_a64_crypto_sha512_has_support()) {
        return mbedtls_internal_sha512_process_many_a64_crypto(ctx, msg, len);
    } else {
        return mbedtls_internal_sha512_process_many_c(ctx, msg, len);
    }
}

int mbedtls_internal_sha512_process(mbedtls_sha512_context *ctx,
                                    const unsigned char data[SHA512_BLOCK_SIZE])
{
    if (mbedtls_a64_crypto_sha512_has_support()) {
        return mbedtls_internal_sha512_process_a64_crypto(ctx, data);
    } else {
        return mbedtls_internal_sha512_process_c(ctx, data);
    }
}

#endif /* MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT */

/*
 * SHA-512 process buffer
 */
int mbedtls_sha512_update(mbedtls_sha512_context *ctx,
                          const unsigned char *input,
                          size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t fill;
    unsigned int left;

    if (ilen == 0) {
        return 0;
    }

    left = (unsigned int) (ctx->total[0] & 0x7F);
    fill = SHA512_BLOCK_SIZE - left;

    ctx->total[0] += (uint64_t) ilen;

    if (ctx->total[0] < (uint64_t) ilen) {
        ctx->total[1]++;
    }

    if (left && ilen >= fill) {
        memcpy((void *) (ctx->buffer + left), input, fill);

        if ((ret = mbedtls_internal_sha512_process(ctx, ctx->buffer)) != 0) {
            return ret;
        }

        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while (ilen >= SHA512_BLOCK_SIZE) {
        size_t processed =
            mbedtls_internal_sha512_process_many(ctx, input, ilen);
        if (processed < SHA512_BLOCK_SIZE) {
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
 * SHA-512 final digest
 */
int mbedtls_sha512_finish(mbedtls_sha512_context *ctx,
                          unsigned char *output)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned used;
    uint64_t high, low;
    int truncated = 0;

    /*
     * Add padding: 0x80 then 0x00 until 16 bytes remain for the length
     */
    used = ctx->total[0] & 0x7F;

    ctx->buffer[used++] = 0x80;

    if (used <= 112) {
        /* Enough room for padding + length in current block */
        memset(ctx->buffer + used, 0, 112 - used);
    } else {
        /* We'll need an extra block */
        memset(ctx->buffer + used, 0, SHA512_BLOCK_SIZE - used);

        if ((ret = mbedtls_internal_sha512_process(ctx, ctx->buffer)) != 0) {
            goto exit;
        }

        memset(ctx->buffer, 0, 112);
    }

    /*
     * Add message length
     */
    high = (ctx->total[0] >> 61)
           | (ctx->total[1] <<  3);
    low  = (ctx->total[0] <<  3);

    sha512_put_uint64_be(high, ctx->buffer, 112);
    sha512_put_uint64_be(low,  ctx->buffer, 120);

    if ((ret = mbedtls_internal_sha512_process(ctx, ctx->buffer)) != 0) {
        goto exit;
    }

    /*
     * Output final state
     */
    sha512_put_uint64_be(ctx->state[0], output,  0);
    sha512_put_uint64_be(ctx->state[1], output,  8);
    sha512_put_uint64_be(ctx->state[2], output, 16);
    sha512_put_uint64_be(ctx->state[3], output, 24);
    sha512_put_uint64_be(ctx->state[4], output, 32);
    sha512_put_uint64_be(ctx->state[5], output, 40);

#if defined(MBEDTLS_SHA384_C)
    truncated = ctx->is384;
#endif
    if (!truncated) {
        sha512_put_uint64_be(ctx->state[6], output, 48);
        sha512_put_uint64_be(ctx->state[7], output, 56);
    }

    ret = 0;

exit:
    mbedtls_sha512_free(ctx);
    return ret;
}

#endif /* !MBEDTLS_SHA512_ALT */

/*
 * output = SHA-512( input buffer )
 */
int mbedtls_sha512(const unsigned char *input,
                   size_t ilen,
                   unsigned char *output,
                   int is384)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_sha512_context ctx;

#if defined(MBEDTLS_SHA384_C) && defined(MBEDTLS_SHA512_C)
    if (is384 != 0 && is384 != 1) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#elif defined(MBEDTLS_SHA512_C)
    if (is384 != 0) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#else /* defined MBEDTLS_SHA384_C only */
    if (is384 == 0) {
        return MBEDTLS_ERR_SHA512_BAD_INPUT_DATA;
    }
#endif

    mbedtls_sha512_init(&ctx);

    if ((ret = mbedtls_sha512_starts(&ctx, is384)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_sha512_update(&ctx, input, ilen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_sha512_finish(&ctx, output)) != 0) {
        goto exit;
    }

exit:
    mbedtls_sha512_free(&ctx);

    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

/*
 * FIPS-180-2 test vectors
 */
static const unsigned char sha_test_buf[3][113] =
{
    { "abc" },
    {
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
    },
    { "" }
};

static const size_t sha_test_buflen[3] =
{
    3, 112, 1000
};

typedef const unsigned char (sha_test_sum_t)[64];

/*
 * SHA-384 test vectors
 */
#if defined(MBEDTLS_SHA384_C)
static sha_test_sum_t sha384_test_sum[] =
{
    { 0xCB, 0x00, 0x75, 0x3F, 0x45, 0xA3, 0x5E, 0x8B,
      0xB5, 0xA0, 0x3D, 0x69, 0x9A, 0xC6, 0x50, 0x07,
      0x27, 0x2C, 0x32, 0xAB, 0x0E, 0xDE, 0xD1, 0x63,
      0x1A, 0x8B, 0x60, 0x5A, 0x43, 0xFF, 0x5B, 0xED,
      0x80, 0x86, 0x07, 0x2B, 0xA1, 0xE7, 0xCC, 0x23,
      0x58, 0xBA, 0xEC, 0xA1, 0x34, 0xC8, 0x25, 0xA7 },
    { 0x09, 0x33, 0x0C, 0x33, 0xF7, 0x11, 0x47, 0xE8,
      0x3D, 0x19, 0x2F, 0xC7, 0x82, 0xCD, 0x1B, 0x47,
      0x53, 0x11, 0x1B, 0x17, 0x3B, 0x3B, 0x05, 0xD2,
      0x2F, 0xA0, 0x80, 0x86, 0xE3, 0xB0, 0xF7, 0x12,
      0xFC, 0xC7, 0xC7, 0x1A, 0x55, 0x7E, 0x2D, 0xB9,
      0x66, 0xC3, 0xE9, 0xFA, 0x91, 0x74, 0x60, 0x39 },
    { 0x9D, 0x0E, 0x18, 0x09, 0x71, 0x64, 0x74, 0xCB,
      0x08, 0x6E, 0x83, 0x4E, 0x31, 0x0A, 0x4A, 0x1C,
      0xED, 0x14, 0x9E, 0x9C, 0x00, 0xF2, 0x48, 0x52,
      0x79, 0x72, 0xCE, 0xC5, 0x70, 0x4C, 0x2A, 0x5B,
      0x07, 0xB8, 0xB3, 0xDC, 0x38, 0xEC, 0xC4, 0xEB,
      0xAE, 0x97, 0xDD, 0xD8, 0x7F, 0x3D, 0x89, 0x85 }
};
#endif /* MBEDTLS_SHA384_C */

/*
 * SHA-512 test vectors
 */
#if defined(MBEDTLS_SHA512_C)
static sha_test_sum_t sha512_test_sum[] =
{
    { 0xDD, 0xAF, 0x35, 0xA1, 0x93, 0x61, 0x7A, 0xBA,
      0xCC, 0x41, 0x73, 0x49, 0xAE, 0x20, 0x41, 0x31,
      0x12, 0xE6, 0xFA, 0x4E, 0x89, 0xA9, 0x7E, 0xA2,
      0x0A, 0x9E, 0xEE, 0xE6, 0x4B, 0x55, 0xD3, 0x9A,
      0x21, 0x92, 0x99, 0x2A, 0x27, 0x4F, 0xC1, 0xA8,
      0x36, 0xBA, 0x3C, 0x23, 0xA3, 0xFE, 0xEB, 0xBD,
      0x45, 0x4D, 0x44, 0x23, 0x64, 0x3C, 0xE8, 0x0E,
      0x2A, 0x9A, 0xC9, 0x4F, 0xA5, 0x4C, 0xA4, 0x9F },
    { 0x8E, 0x95, 0x9B, 0x75, 0xDA, 0xE3, 0x13, 0xDA,
      0x8C, 0xF4, 0xF7, 0x28, 0x14, 0xFC, 0x14, 0x3F,
      0x8F, 0x77, 0x79, 0xC6, 0xEB, 0x9F, 0x7F, 0xA1,
      0x72, 0x99, 0xAE, 0xAD, 0xB6, 0x88, 0x90, 0x18,
      0x50, 0x1D, 0x28, 0x9E, 0x49, 0x00, 0xF7, 0xE4,
      0x33, 0x1B, 0x99, 0xDE, 0xC4, 0xB5, 0x43, 0x3A,
      0xC7, 0xD3, 0x29, 0xEE, 0xB6, 0xDD, 0x26, 0x54,
      0x5E, 0x96, 0xE5, 0x5B, 0x87, 0x4B, 0xE9, 0x09 },
    { 0xE7, 0x18, 0x48, 0x3D, 0x0C, 0xE7, 0x69, 0x64,
      0x4E, 0x2E, 0x42, 0xC7, 0xBC, 0x15, 0xB4, 0x63,
      0x8E, 0x1F, 0x98, 0xB1, 0x3B, 0x20, 0x44, 0x28,
      0x56, 0x32, 0xA8, 0x03, 0xAF, 0xA9, 0x73, 0xEB,
      0xDE, 0x0F, 0xF2, 0x44, 0x87, 0x7E, 0xA6, 0x0A,
      0x4C, 0xB0, 0x43, 0x2C, 0xE5, 0x77, 0xC3, 0x1B,
      0xEB, 0x00, 0x9C, 0x5C, 0x2C, 0x49, 0xAA, 0x2E,
      0x4E, 0xAD, 0xB2, 0x17, 0xAD, 0x8C, 0xC0, 0x9B }
};
#endif /* MBEDTLS_SHA512_C */

static int mbedtls_sha512_common_self_test(int verbose, int is384)
{
    int i, buflen, ret = 0;
    unsigned char *buf;
    unsigned char sha512sum[64];
    mbedtls_sha512_context ctx;

#if defined(MBEDTLS_SHA384_C) && defined(MBEDTLS_SHA512_C)
    sha_test_sum_t *sha_test_sum = (is384) ? sha384_test_sum : sha512_test_sum;
#elif defined(MBEDTLS_SHA512_C)
    sha_test_sum_t *sha_test_sum = sha512_test_sum;
#else
    sha_test_sum_t *sha_test_sum = sha384_test_sum;
#endif

    buf = mbedtls_calloc(1024, sizeof(unsigned char));
    if (NULL == buf) {
        if (verbose != 0) {
            mbedtls_printf("Buffer allocation failed\n");
        }

        return 1;
    }

    mbedtls_sha512_init(&ctx);

    for (i = 0; i < 3; i++) {
        if (verbose != 0) {
            mbedtls_printf("  SHA-%d test #%d: ", 512 - is384 * 128, i + 1);
        }

        if ((ret = mbedtls_sha512_starts(&ctx, is384)) != 0) {
            goto fail;
        }

        if (i == 2) {
            memset(buf, 'a', buflen = 1000);

            for (int j = 0; j < 1000; j++) {
                ret = mbedtls_sha512_update(&ctx, buf, buflen);
                if (ret != 0) {
                    goto fail;
                }
            }
        } else {
            ret = mbedtls_sha512_update(&ctx, sha_test_buf[i],
                                        sha_test_buflen[i]);
            if (ret != 0) {
                goto fail;
            }
        }

        if ((ret = mbedtls_sha512_finish(&ctx, sha512sum)) != 0) {
            goto fail;
        }

        if (memcmp(sha512sum, sha_test_sum[i], 64 - is384 * 16) != 0) {
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
    mbedtls_sha512_free(&ctx);
    mbedtls_free(buf);

    return ret;
}

#if defined(MBEDTLS_SHA512_C)
int mbedtls_sha512_self_test(int verbose)
{
    return mbedtls_sha512_common_self_test(verbose, 0);
}
#endif /* MBEDTLS_SHA512_C */

#if defined(MBEDTLS_SHA384_C)
int mbedtls_sha384_self_test(int verbose)
{
    return mbedtls_sha512_common_self_test(verbose, 1);
}
#endif /* MBEDTLS_SHA384_C */

#undef ARRAY_LENGTH

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SHA512_C || MBEDTLS_SHA384_C */
