/**
 * \file common.h
 *
 * \brief Utility macros for internal use in the library
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_LIBRARY_COMMON_H
#define MBEDTLS_LIBRARY_COMMON_H

#include "mbedtls/build_info.h"
#include "alignment.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#define MBEDTLS_HAVE_NEON_INTRINSICS
#elif defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
#include <arm64_neon.h>
#define MBEDTLS_HAVE_NEON_INTRINSICS
#endif

/** Helper to define a function as static except when building invasive tests.
 *
 * If a function is only used inside its own source file and should be
 * declared `static` to allow the compiler to optimize for code size,
 * but that function has unit tests, define it with
 * ```
 * MBEDTLS_STATIC_TESTABLE int mbedtls_foo(...) { ... }
 * ```
 * and declare it in a header in the `library/` directory with
 * ```
 * #if defined(MBEDTLS_TEST_HOOKS)
 * int mbedtls_foo(...);
 * #endif
 * ```
 */
#if defined(MBEDTLS_TEST_HOOKS)
#define MBEDTLS_STATIC_TESTABLE
#else
#define MBEDTLS_STATIC_TESTABLE static
#endif

#if defined(MBEDTLS_TEST_HOOKS)
extern void (*mbedtls_test_hook_test_fail)(const char *test, int line, const char *file);
#define MBEDTLS_TEST_HOOK_TEST_ASSERT(TEST) \
    do { \
        if ((!(TEST)) && ((*mbedtls_test_hook_test_fail) != NULL)) \
        { \
            (*mbedtls_test_hook_test_fail)( #TEST, __LINE__, __FILE__); \
        } \
    } while (0)
#else
#define MBEDTLS_TEST_HOOK_TEST_ASSERT(TEST)
#endif /* defined(MBEDTLS_TEST_HOOKS) */

/** \def ARRAY_LENGTH
 * Return the number of elements of a static or stack array.
 *
 * \param array         A value of array (not pointer) type.
 *
 * \return The number of elements of the array.
 */
/* A correct implementation of ARRAY_LENGTH, but which silently gives
 * a nonsensical result if called with a pointer rather than an array. */
#define ARRAY_LENGTH_UNSAFE(array)            \
    (sizeof(array) / sizeof(*(array)))

#if defined(__GNUC__)
/* Test if arg and &(arg)[0] have the same type. This is true if arg is
 * an array but not if it's a pointer. */
#define IS_ARRAY_NOT_POINTER(arg)                                     \
    (!__builtin_types_compatible_p(__typeof__(arg),                \
                                   __typeof__(&(arg)[0])))
/* A compile-time constant with the value 0. If `const_expr` is not a
 * compile-time constant with a nonzero value, cause a compile-time error. */
#define STATIC_ASSERT_EXPR(const_expr)                                \
    (0 && sizeof(struct { unsigned int STATIC_ASSERT : 1 - 2 * !(const_expr); }))

/* Return the scalar value `value` (possibly promoted). This is a compile-time
 * constant if `value` is. `condition` must be a compile-time constant.
 * If `condition` is false, arrange to cause a compile-time error. */
#define STATIC_ASSERT_THEN_RETURN(condition, value)   \
    (STATIC_ASSERT_EXPR(condition) ? 0 : (value))

#define ARRAY_LENGTH(array)                                           \
    (STATIC_ASSERT_THEN_RETURN(IS_ARRAY_NOT_POINTER(array),         \
                               ARRAY_LENGTH_UNSAFE(array)))

#else
/* If we aren't sure the compiler supports our non-standard tricks,
 * fall back to the unsafe implementation. */
#define ARRAY_LENGTH(array) ARRAY_LENGTH_UNSAFE(array)
#endif
/** Allow library to access its structs' private members.
 *
 * Although structs defined in header files are publicly available,
 * their members are private and should not be accessed by the user.
 */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

/**
 * \brief       Securely zeroize a buffer then free it.
 *
 *              Similar to making consecutive calls to
 *              \c mbedtls_platform_zeroize() and \c mbedtls_free(), but has
 *              code size savings, and potential for optimisation in the future.
 *
 *              Guaranteed to be a no-op if \p buf is \c NULL and \p len is 0.
 *
 * \param buf   Buffer to be zeroized then freed.
 * \param len   Length of the buffer in bytes
 */
void mbedtls_zeroize_and_free(void *buf, size_t len);

/** Return an offset into a buffer.
 *
 * This is just the addition of an offset to a pointer, except that this
 * function also accepts an offset of 0 into a buffer whose pointer is null.
 * (`p + n` has undefined behavior when `p` is null, even when `n == 0`.
 * A null pointer is a valid buffer pointer when the size is 0, for example
 * as the result of `malloc(0)` on some platforms.)
 *
 * \param p     Pointer to a buffer of at least n bytes.
 *              This may be \p NULL if \p n is zero.
 * \param n     An offset in bytes.
 * \return      Pointer to offset \p n in the buffer \p p.
 *              Note that this is only a valid pointer if the size of the
 *              buffer is at least \p n + 1.
 */
static inline unsigned char *mbedtls_buffer_offset(
    unsigned char *p, size_t n)
{
    return p == NULL ? NULL : p + n;
}

/** Return an offset into a read-only buffer.
 *
 * Similar to mbedtls_buffer_offset(), but for const pointers.
 *
 * \param p     Pointer to a buffer of at least n bytes.
 *              This may be \p NULL if \p n is zero.
 * \param n     An offset in bytes.
 * \return      Pointer to offset \p n in the buffer \p p.
 *              Note that this is only a valid pointer if the size of the
 *              buffer is at least \p n + 1.
 */
static inline const unsigned char *mbedtls_buffer_offset_const(
    const unsigned char *p, size_t n)
{
    return p == NULL ? NULL : p + n;
}

/* Always inline mbedtls_xor() for similar reasons as mbedtls_xor_no_simd(). */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
/**
 * Perform a fast block XOR operation, such that
 * r[i] = a[i] ^ b[i] where 0 <= i < n
 *
 * \param   r Pointer to result (buffer of at least \p n bytes). \p r
 *            may be equal to either \p a or \p b, but behaviour when
 *            it overlaps in other ways is undefined.
 * \param   a Pointer to input (buffer of at least \p n bytes)
 * \param   b Pointer to input (buffer of at least \p n bytes)
 * \param   n Number of bytes to process.
 *
 * \note      Depending on the situation, it may be faster to use either mbedtls_xor() or
 *            mbedtls_xor_no_simd() (these are functionally equivalent).
 *            If the result is used immediately after the xor operation in non-SIMD code (e.g, in
 *            AES-CBC), there may be additional latency to transfer the data from SIMD to scalar
 *            registers, and in this case, mbedtls_xor_no_simd() may be faster. In other cases where
 *            the result is not used immediately (e.g., in AES-CTR), mbedtls_xor() may be faster.
 *            For targets without SIMD support, they will behave the same.
 */
static inline void mbedtls_xor(unsigned char *r,
                               const unsigned char *a,
                               const unsigned char *b,
                               size_t n)
{
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
#if defined(MBEDTLS_HAVE_NEON_INTRINSICS) && \
    (!(defined(MBEDTLS_COMPILER_IS_GCC) && MBEDTLS_GCC_VERSION < 70300))
    /* Old GCC versions generate a warning here, so disable the NEON path for these compilers */
    for (; (i + 16) <= n; i += 16) {
        uint8x16_t v1 = vld1q_u8(a + i);
        uint8x16_t v2 = vld1q_u8(b + i);
        uint8x16_t x = veorq_u8(v1, v2);
        vst1q_u8(r + i, x);
    }
#if defined(__IAR_SYSTEMS_ICC__)
    /* This if statement helps some compilers (e.g., IAR) optimise out the byte-by-byte tail case
     * where n is a constant multiple of 16.
     * For other compilers (e.g. recent gcc and clang) it makes no difference if n is a compile-time
     * constant, and is a very small perf regression if n is not a compile-time constant. */
    if (n % 16 == 0) {
        return;
    }
#endif
#elif defined(MBEDTLS_ARCH_IS_X64) || defined(MBEDTLS_ARCH_IS_ARM64)
    /* This codepath probably only makes sense on architectures with 64-bit registers */
    for (; (i + 8) <= n; i += 8) {
        uint64_t x = mbedtls_get_unaligned_uint64(a + i) ^ mbedtls_get_unaligned_uint64(b + i);
        mbedtls_put_unaligned_uint64(r + i, x);
    }
#if defined(__IAR_SYSTEMS_ICC__)
    if (n % 8 == 0) {
        return;
    }
#endif
#else
    for (; (i + 4) <= n; i += 4) {
        uint32_t x = mbedtls_get_unaligned_uint32(a + i) ^ mbedtls_get_unaligned_uint32(b + i);
        mbedtls_put_unaligned_uint32(r + i, x);
    }
#if defined(__IAR_SYSTEMS_ICC__)
    if (n % 4 == 0) {
        return;
    }
#endif
#endif
#endif
    for (; i < n; i++) {
        r[i] = a[i] ^ b[i];
    }
}

/* Always inline mbedtls_xor_no_simd() as we see significant perf regressions when it does not get
 * inlined (e.g., observed about 3x perf difference in gcm_mult_largetable with gcc 7 - 12) */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
/**
 * Perform a fast block XOR operation, such that
 * r[i] = a[i] ^ b[i] where 0 <= i < n
 *
 * In some situations, this can perform better than mbedtls_xor() (e.g., it's about 5%
 * better in AES-CBC).
 *
 * \param   r Pointer to result (buffer of at least \p n bytes). \p r
 *            may be equal to either \p a or \p b, but behaviour when
 *            it overlaps in other ways is undefined.
 * \param   a Pointer to input (buffer of at least \p n bytes)
 * \param   b Pointer to input (buffer of at least \p n bytes)
 * \param   n Number of bytes to process.
 *
 * \note      Depending on the situation, it may be faster to use either mbedtls_xor() or
 *            mbedtls_xor_no_simd() (these are functionally equivalent).
 *            If the result is used immediately after the xor operation in non-SIMD code (e.g, in
 *            AES-CBC), there may be additional latency to transfer the data from SIMD to scalar
 *            registers, and in this case, mbedtls_xor_no_simd() may be faster. In other cases where
 *            the result is not used immediately (e.g., in AES-CTR), mbedtls_xor() may be faster.
 *            For targets without SIMD support, they will behave the same.
 */
static inline void mbedtls_xor_no_simd(unsigned char *r,
                                       const unsigned char *a,
                                       const unsigned char *b,
                                       size_t n)
{
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
#if defined(MBEDTLS_ARCH_IS_X64) || defined(MBEDTLS_ARCH_IS_ARM64)
    /* This codepath probably only makes sense on architectures with 64-bit registers */
    for (; (i + 8) <= n; i += 8) {
        uint64_t x = mbedtls_get_unaligned_uint64(a + i) ^ mbedtls_get_unaligned_uint64(b + i);
        mbedtls_put_unaligned_uint64(r + i, x);
    }
#if defined(__IAR_SYSTEMS_ICC__)
    /* This if statement helps some compilers (e.g., IAR) optimise out the byte-by-byte tail case
     * where n is a constant multiple of 8.
     * For other compilers (e.g. recent gcc and clang) it makes no difference if n is a compile-time
     * constant, and is a very small perf regression if n is not a compile-time constant. */
    if (n % 8 == 0) {
        return;
    }
#endif
#else
    for (; (i + 4) <= n; i += 4) {
        uint32_t x = mbedtls_get_unaligned_uint32(a + i) ^ mbedtls_get_unaligned_uint32(b + i);
        mbedtls_put_unaligned_uint32(r + i, x);
    }
#if defined(__IAR_SYSTEMS_ICC__)
    if (n % 4 == 0) {
        return;
    }
#endif
#endif
#endif
    for (; i < n; i++) {
        r[i] = a[i] ^ b[i];
    }
}

/* Fix MSVC C99 compatible issue
 *      MSVC support __func__ from visual studio 2015( 1900 )
 *      Use MSVC predefine macro to avoid name check fail.
 */
#if (defined(_MSC_VER) && (_MSC_VER <= 1900))
#define /*no-check-names*/ __func__ __FUNCTION__
#endif

/* Define `asm` for compilers which don't define it. */
/* *INDENT-OFF* */
#ifndef asm
#if defined(__IAR_SYSTEMS_ICC__)
#define asm __asm
#else
#define asm __asm__
#endif
#endif
/* *INDENT-ON* */

/*
 * Define the constraint used for read-only pointer operands to aarch64 asm.
 *
 * This is normally the usual "r", but for aarch64_32 (aka ILP32,
 * as found in watchos), "p" is required to avoid warnings from clang.
 *
 * Note that clang does not recognise '+p' or '=p', and armclang
 * does not recognise 'p' at all. Therefore, to update a pointer from
 * aarch64 assembly, it is necessary to use something like:
 *
 * uintptr_t uptr = (uintptr_t) ptr;
 * asm( "ldr x4, [%x0], #8" ... : "+r" (uptr) : : )
 * ptr = (void*) uptr;
 *
 * Note that the "x" in "%x0" is neccessary; writing "%0" will cause warnings.
 */
#if defined(__aarch64__) && defined(MBEDTLS_HAVE_ASM)
#if UINTPTR_MAX == 0xfffffffful
/* ILP32: Specify the pointer operand slightly differently, as per #7787. */
#define MBEDTLS_ASM_AARCH64_PTR_CONSTRAINT "p"
#elif UINTPTR_MAX == 0xfffffffffffffffful
/* Normal case (64-bit pointers): use "r" as the constraint for pointer operands to asm */
#define MBEDTLS_ASM_AARCH64_PTR_CONSTRAINT "r"
#else
#error "Unrecognised pointer size for aarch64"
#endif
#endif

/* Always provide a static assert macro, so it can be used unconditionally.
 * It does nothing on systems where we don't know how to define a static assert.
 */
/* Can't use the C11-style `defined(static_assert)` on FreeBSD, since it
 * defines static_assert even with -std=c99, but then complains about it.
 */
#if defined(static_assert) && !defined(__FreeBSD__)
#define MBEDTLS_STATIC_ASSERT(expr, msg)    static_assert(expr, msg)
#else
/* Make sure `MBEDTLS_STATIC_ASSERT(expr, msg);` is valid both inside and
 * outside a function. We choose a struct declaration, which can be repeated
 * any number of times and does not need a matching definition. */
#define MBEDTLS_STATIC_ASSERT(expr, msg)                                \
    struct ISO_C_does_not_allow_extra_semicolon_outside_of_a_function
#endif

#if defined(__has_builtin)
#define MBEDTLS_HAS_BUILTIN(x) __has_builtin(x)
#else
#define MBEDTLS_HAS_BUILTIN(x) 0
#endif

/* Define compiler branch hints */
#if MBEDTLS_HAS_BUILTIN(__builtin_expect)
#define MBEDTLS_LIKELY(x)       __builtin_expect(!!(x), 1)
#define MBEDTLS_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#define MBEDTLS_LIKELY(x)       x
#define MBEDTLS_UNLIKELY(x)     x
#endif

/* MBEDTLS_ASSUME may be used to provide additional information to the compiler
 * which can result in smaller code-size. */
#if MBEDTLS_HAS_BUILTIN(__builtin_assume)
/* clang provides __builtin_assume */
#define MBEDTLS_ASSUME(x)       __builtin_assume(x)
#elif MBEDTLS_HAS_BUILTIN(__builtin_unreachable)
/* gcc and IAR can use __builtin_unreachable */
#define MBEDTLS_ASSUME(x)       do { if (!(x)) __builtin_unreachable(); } while (0)
#elif defined(_MSC_VER)
/* Supported by MSVC since VS 2005 */
#define MBEDTLS_ASSUME(x)       __assume(x)
#else
#define MBEDTLS_ASSUME(x)       do { } while (0)
#endif

/* For gcc -Os, override with -O2 for a given function.
 *
 * This will not affect behaviour for other optimisation settings, e.g. -O0.
 */
#if defined(MBEDTLS_COMPILER_IS_GCC) && defined(__OPTIMIZE_SIZE__)
#define MBEDTLS_OPTIMIZE_FOR_PERFORMANCE __attribute__((optimize("-O2")))
#else
#define MBEDTLS_OPTIMIZE_FOR_PERFORMANCE
#endif

/* Suppress compiler warnings for unused functions and variables. */
#if !defined(MBEDTLS_MAYBE_UNUSED) && defined(__has_attribute)
#    if __has_attribute(unused)
#        define MBEDTLS_MAYBE_UNUSED __attribute__((unused))
#    endif
#endif
#if !defined(MBEDTLS_MAYBE_UNUSED) && defined(__GNUC__)
#    define MBEDTLS_MAYBE_UNUSED __attribute__((unused))
#endif
#if !defined(MBEDTLS_MAYBE_UNUSED) && defined(__IAR_SYSTEMS_ICC__) && defined(__VER__)
/* IAR does support __attribute__((unused)), but only if the -e flag (extended language support)
 * is given; the pragma always works.
 * Unfortunately the pragma affects the rest of the file where it is used, but this is harmless.
 * Check for version 5.2 or later - this pragma may be supported by earlier versions, but I wasn't
 * able to find documentation).
 */
#    if (__VER__ >= 5020000)
#        define MBEDTLS_MAYBE_UNUSED _Pragma("diag_suppress=Pe177")
#    endif
#endif
#if !defined(MBEDTLS_MAYBE_UNUSED) && defined(_MSC_VER)
#    define MBEDTLS_MAYBE_UNUSED __pragma(warning(suppress:4189))
#endif
#if !defined(MBEDTLS_MAYBE_UNUSED)
#    define MBEDTLS_MAYBE_UNUSED
#endif

/* GCC >= 15 has a warning 'unterminated-string-initialization' which complains if you initialize
 * a string into an array without space for a terminating NULL character. In some places in the
 * codebase this behaviour is intended, so we add the macro MBEDTLS_ATTRIBUTE_UNTERMINATED_STRING
 * to suppress the warning in these places.
 */
#if defined(__has_attribute)
#if __has_attribute(nonstring)
#define MBEDTLS_HAS_ATTRIBUTE_NONSTRING
#endif /* __has_attribute(nonstring) */
#endif /* __has_attribute */
#if defined(MBEDTLS_HAS_ATTRIBUTE_NONSTRING)
#define MBEDTLS_ATTRIBUTE_UNTERMINATED_STRING __attribute__((nonstring))
#else
#define MBEDTLS_ATTRIBUTE_UNTERMINATED_STRING
#endif /* MBEDTLS_HAS_ATTRIBUTE_NONSTRING */

#endif /* MBEDTLS_LIBRARY_COMMON_H */
