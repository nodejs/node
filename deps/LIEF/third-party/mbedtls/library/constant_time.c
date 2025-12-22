/**
 *  Constant-time functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * The following functions are implemented without using comparison operators, as those
 * might be translated to branches by some compilers on some platforms.
 */

#include <stdint.h>
#include <limits.h>

#include "common.h"
#include "constant_time_internal.h"
#include "mbedtls/constant_time.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"

#include <string.h>

#if !defined(MBEDTLS_CT_ASM)
/*
 * Define an object with the value zero, such that the compiler cannot prove that it
 * has the value zero (because it is volatile, it "may be modified in ways unknown to
 * the implementation").
 */
volatile mbedtls_ct_uint_t mbedtls_ct_zero = 0;
#endif

/*
 * Define MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS where assembly is present to
 * perform fast unaligned access to volatile data.
 *
 * This is needed because mbedtls_get_unaligned_uintXX etc don't support volatile
 * memory accesses.
 *
 * Some of these definitions could be moved into alignment.h but for now they are
 * only used here.
 */
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS) && \
    ((defined(MBEDTLS_CT_ARM_ASM) && (UINTPTR_MAX == 0xfffffffful)) || \
    defined(MBEDTLS_CT_AARCH64_ASM))
/* We check pointer sizes to avoid issues with them not matching register size requirements */
#define MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS

static inline uint32_t mbedtls_get_unaligned_volatile_uint32(volatile const unsigned char *p)
{
    /* This is UB, even where it's safe:
     *    return *((volatile uint32_t*)p);
     * so instead the same thing is expressed in assembly below.
     */
    uint32_t r;
#if defined(MBEDTLS_CT_ARM_ASM)
    asm volatile ("ldr %0, [%1]" : "=r" (r) : "r" (p) :);
#elif defined(MBEDTLS_CT_AARCH64_ASM)
    asm volatile ("ldr %w0, [%1]" : "=r" (r) : MBEDTLS_ASM_AARCH64_PTR_CONSTRAINT(p) :);
#else
#error "No assembly defined for mbedtls_get_unaligned_volatile_uint32"
#endif
    return r;
}
#endif /* defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS) &&
          (defined(MBEDTLS_CT_ARM_ASM) || defined(MBEDTLS_CT_AARCH64_ASM)) */

int mbedtls_ct_memcmp(const void *a,
                      const void *b,
                      size_t n)
{
    size_t i = 0;
    /*
     * `A` and `B` are cast to volatile to ensure that the compiler
     * generates code that always fully reads both buffers.
     * Otherwise it could generate a test to exit early if `diff` has all
     * bits set early in the loop.
     */
    volatile const unsigned char *A = (volatile const unsigned char *) a;
    volatile const unsigned char *B = (volatile const unsigned char *) b;
    uint32_t diff = 0;

#if defined(MBEDTLS_EFFICIENT_UNALIGNED_VOLATILE_ACCESS)
    for (; (i + 4) <= n; i += 4) {
        uint32_t x = mbedtls_get_unaligned_volatile_uint32(A + i);
        uint32_t y = mbedtls_get_unaligned_volatile_uint32(B + i);
        diff |= x ^ y;
    }
#endif

    for (; i < n; i++) {
        /* Read volatile data in order before computing diff.
         * This avoids IAR compiler warning:
         * 'the order of volatile accesses is undefined ..' */
        unsigned char x = A[i], y = B[i];
        diff |= x ^ y;
    }


#if (INT_MAX < INT32_MAX)
    /* We don't support int smaller than 32-bits, but if someone tried to build
     * with this configuration, there is a risk that, for differing data, the
     * only bits set in diff are in the top 16-bits, and would be lost by a
     * simple cast from uint32 to int.
     * This would have significant security implications, so protect against it. */
#error "mbedtls_ct_memcmp() requires minimum 32-bit ints"
#else
    /* The bit-twiddling ensures that when we cast uint32_t to int, we are casting
     * a value that is in the range 0..INT_MAX - a value larger than this would
     * result in implementation defined behaviour.
     *
     * This ensures that the value returned by the function is non-zero iff
     * diff is non-zero.
     */
    return (int) ((diff & 0xffff) | (diff >> 16));
#endif
}

#if defined(MBEDTLS_NIST_KW_C)

int mbedtls_ct_memcmp_partial(const void *a,
                              const void *b,
                              size_t n,
                              size_t skip_head,
                              size_t skip_tail)
{
    unsigned int diff = 0;

    volatile const unsigned char *A = (volatile const unsigned char *) a;
    volatile const unsigned char *B = (volatile const unsigned char *) b;

    size_t valid_end = n - skip_tail;

    for (size_t i = 0; i < n; i++) {
        unsigned char x = A[i], y = B[i];
        unsigned int d = x ^ y;
        mbedtls_ct_condition_t valid = mbedtls_ct_bool_and(mbedtls_ct_uint_ge(i, skip_head),
                                                           mbedtls_ct_uint_lt(i, valid_end));
        diff |= mbedtls_ct_uint_if_else_0(valid, d);
    }

    /* Since we go byte-by-byte, the only bits set will be in the bottom 8 bits, so the
     * cast from uint to int is safe. */
    return (int) diff;
}

#endif

#if defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT)

void mbedtls_ct_memmove_left(void *start, size_t total, size_t offset)
{
    volatile unsigned char *buf = start;
    for (size_t i = 0; i < total; i++) {
        mbedtls_ct_condition_t no_op = mbedtls_ct_uint_gt(total - offset, i);
        /* The first `total - offset` passes are a no-op. The last
         * `offset` passes shift the data one byte to the left and
         * zero out the last byte. */
        for (size_t n = 0; n < total - 1; n++) {
            unsigned char current = buf[n];
            unsigned char next    = buf[n+1];
            buf[n] = mbedtls_ct_uint_if(no_op, current, next);
        }
        buf[total-1] = mbedtls_ct_uint_if_else_0(no_op, buf[total-1]);
    }
}

#endif /* MBEDTLS_PKCS1_V15 && MBEDTLS_RSA_C && ! MBEDTLS_RSA_ALT */

void mbedtls_ct_memcpy_if(mbedtls_ct_condition_t condition,
                          unsigned char *dest,
                          const unsigned char *src1,
                          const unsigned char *src2,
                          size_t len)
{
#if defined(MBEDTLS_CT_SIZE_64)
    const uint64_t mask     = (uint64_t) condition;
    const uint64_t not_mask = (uint64_t) ~mbedtls_ct_compiler_opaque(condition);
#else
    const uint32_t mask     = (uint32_t) condition;
    const uint32_t not_mask = (uint32_t) ~mbedtls_ct_compiler_opaque(condition);
#endif

    /* If src2 is NULL, setup src2 so that we read from the destination address.
     *
     * This means that if src2 == NULL && condition is false, the result will be a
     * no-op because we read from dest and write the same data back into dest.
     */
    if (src2 == NULL) {
        src2 = dest;
    }

    /* dest[i] = c1 == c2 ? src[i] : dest[i] */
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
#if defined(MBEDTLS_CT_SIZE_64)
    for (; (i + 8) <= len; i += 8) {
        uint64_t a = mbedtls_get_unaligned_uint64(src1 + i) & mask;
        uint64_t b = mbedtls_get_unaligned_uint64(src2 + i) & not_mask;
        mbedtls_put_unaligned_uint64(dest + i, a | b);
    }
#else
    for (; (i + 4) <= len; i += 4) {
        uint32_t a = mbedtls_get_unaligned_uint32(src1 + i) & mask;
        uint32_t b = mbedtls_get_unaligned_uint32(src2 + i) & not_mask;
        mbedtls_put_unaligned_uint32(dest + i, a | b);
    }
#endif /* defined(MBEDTLS_CT_SIZE_64) */
#endif /* MBEDTLS_EFFICIENT_UNALIGNED_ACCESS */
    for (; i < len; i++) {
        dest[i] = (src1[i] & mask) | (src2[i] & not_mask);
    }
}

void mbedtls_ct_memcpy_offset(unsigned char *dest,
                              const unsigned char *src,
                              size_t offset,
                              size_t offset_min,
                              size_t offset_max,
                              size_t len)
{
    size_t offsetval;

    for (offsetval = offset_min; offsetval <= offset_max; offsetval++) {
        mbedtls_ct_memcpy_if(mbedtls_ct_uint_eq(offsetval, offset), dest, src + offsetval, NULL,
                             len);
    }
}

#if defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT)

void mbedtls_ct_zeroize_if(mbedtls_ct_condition_t condition, void *buf, size_t len)
{
    uint32_t mask = (uint32_t) ~condition;
    uint8_t *p = (uint8_t *) buf;
    size_t i = 0;
#if defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)
    for (; (i + 4) <= len; i += 4) {
        mbedtls_put_unaligned_uint32((void *) (p + i),
                                     mbedtls_get_unaligned_uint32((void *) (p + i)) & mask);
    }
#endif
    for (; i < len; i++) {
        p[i] = p[i] & mask;
    }
}

#endif /* defined(MBEDTLS_PKCS1_V15) && defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_RSA_ALT) */
