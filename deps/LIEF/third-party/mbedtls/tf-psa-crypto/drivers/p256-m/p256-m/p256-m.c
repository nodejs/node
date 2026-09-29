/*
 * Implementation of curve P-256 (ECDH and ECDSA)
 *
 * Copyright The Mbed TLS Contributors
 * Author: Manuel Pégourié-Gonnard.
 * SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "p256-m.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (MBEDTLS_PSA_P256M_DRIVER_ENABLED)

/*
 * Zeroize memory - this should not be optimized away
 */
#define zeroize mbedtls_platform_zeroize

/*
 * Helpers to test constant-time behaviour with valgrind or MemSan.
 *
 * CT_POISON() is used for secret data. It marks the memory area as
 * uninitialised, so that any branch or pointer dereference that depends on it
 * (even indirectly) triggers a warning.
 * CT_UNPOISON() is used for public data; it marks the area as initialised.
 *
 * These are macros in order to avoid interfering with origin tracking.
 */
#if defined(CT_MEMSAN)

#include <sanitizer/msan_interface.h>
#define CT_POISON   __msan_allocated_memory
// void __msan_allocated_memory(const volatile void* data, size_t size);
#define CT_UNPOISON __msan_unpoison
// void __msan_unpoison(const volatile void *a, size_t size);

#elif defined(CT_VALGRIND)

#include <valgrind/memcheck.h>
#define CT_POISON   VALGRIND_MAKE_MEM_UNDEFINED
// VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr,_qzz_len)
#define CT_UNPOISON VALGRIND_MAKE_MEM_DEFINED
// VALGRIND_MAKE_MEM_DEFINED(_qzz_addr,_qzz_len)

#else
#define CT_POISON(p, sz)
#define CT_UNPOISON(p, sz)
#endif

/**********************************************************************
 *
 * Operations on fixed-width unsigned integers
 *
 * Represented using 32-bit limbs, least significant limb first.
 * That is: x = x[0] + 2^32 x[1] + ... + 2^224 x[7] for 256-bit.
 *
 **********************************************************************/

/*
 * 256-bit set to 32-bit value
 *
 * in: x in [0, 2^32)
 * out: z = x
 */
static void u256_set32(uint32_t z[8], uint32_t x)
{
    z[0] = x;
    for (unsigned i = 1; i < 8; i++) {
        z[i] = 0;
    }
}

/*
 * 256-bit addition
 *
 * in: x, y in [0, 2^256)
 * out: z = (x + y) mod 2^256
 *      c = (x + y) div 2^256
 * That is, z + c * 2^256 = x + y
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static uint32_t u256_add(uint32_t z[8],
                         const uint32_t x[8], const uint32_t y[8])
{
    uint32_t carry = 0;

    for (unsigned i = 0; i < 8; i++) {
        uint64_t sum = (uint64_t) carry + x[i] + y[i];
        z[i] = (uint32_t) sum;
        carry = (uint32_t) (sum >> 32);
    }

    return carry;
}

/*
 * 256-bit subtraction
 *
 * in: x, y in [0, 2^256)
 * out: z = (x - y) mod 2^256
 *      c = 0 if x >=y, 1 otherwise
 * That is, z = c * 2^256 + x - y
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static uint32_t u256_sub(uint32_t z[8],
                         const uint32_t x[8], const uint32_t y[8])
{
    uint32_t carry = 0;

    for (unsigned i = 0; i < 8; i++) {
        uint64_t diff = (uint64_t) x[i] - y[i] - carry;
        z[i] = (uint32_t) diff;
        carry = -(uint32_t) (diff >> 32);
    }

    return carry;
}

/*
 * 256-bit conditional assignment
 *
 * in: x in [0, 2^256)
 *     c in [0, 1]
 * out: z = x if c == 1, z unchanged otherwise
 *
 * Note: as a memory area, z must be either equal to x, or not overlap.
 */
static void u256_cmov(uint32_t z[8], const uint32_t x[8], uint32_t c)
{
    const uint32_t x_mask = -c;
    for (unsigned i = 0; i < 8; i++) {
        z[i] = (z[i] & ~x_mask) | (x[i] & x_mask);
    }
}

/*
 * 256-bit compare for equality
 *
 * in: x in [0, 2^256)
 *     y in [0, 2^256)
 * out: 0 if x == y, unspecified non-zero otherwise
 */
static uint32_t u256_diff(const uint32_t x[8], const uint32_t y[8])
{
    uint32_t diff = 0;
    for (unsigned i = 0; i < 8; i++) {
        diff |= x[i] ^ y[i];
    }
    return diff;
}

/*
 * 256-bit compare to zero
 *
 * in: x in [0, 2^256)
 * out: 0 if x == 0, unspecified non-zero otherwise
 */
static uint32_t u256_diff0(const uint32_t x[8])
{
    uint32_t diff = 0;
    for (unsigned i = 0; i < 8; i++) {
        diff |= x[i];
    }
    return diff;
}

/*
 * 32 x 32 -> 64-bit multiply-and-accumulate
 *
 * in: x, y, z, t in [0, 2^32)
 * out: x * y + z + t in [0, 2^64)
 *
 * Note: this computation cannot overflow.
 *
 * Note: this function has two pure-C implementations (depending on whether
 * MUL64_IS_CONSTANT_TIME), and possibly optimised asm implementations.
 * Start with the potential asm definitions, and use the C definition only if
 * we no have no asm for the current toolchain & CPU.
 */
static uint64_t u32_muladd64(uint32_t x, uint32_t y, uint32_t z, uint32_t t);

/* This macro is used to mark whether an asm implentation is found */
#undef MULADD64_ASM
/* This macro is used to mark whether the implementation has a small
 * code size (ie, it can be inlined even in an unrolled loop) */
#undef MULADD64_SMALL

/*
 * Currently assembly optimisations are only supported with GCC/Clang for
 * Arm's Cortex-A and Cortex-M lines of CPUs, which start with the v6-M and
 * v7-M architectures. __ARM_ARCH_PROFILE is not defined for v6 and earlier.
 * Thumb and 32-bit assembly is supported; aarch64 is not supported.
 */
#if defined(__GNUC__) &&\
    defined(__ARM_ARCH) && __ARM_ARCH >= 6 && defined(__ARM_ARCH_PROFILE) && \
    ( __ARM_ARCH_PROFILE == 77 || __ARM_ARCH_PROFILE == 65 ) /* 'M' or 'A' */ && \
    !defined(__aarch64__)

/*
 * This set of CPUs is conveniently partitioned as follows:
 *
 * 1. Cores that have the DSP extension, which includes a 1-cycle UMAAL
 *    instruction: M4, M7, M33, all A-class cores.
 * 2. Cores that don't have the DSP extension, and also lack a constant-time
 *    64-bit multiplication instruction:
 *    - M0, M0+, M23: 32-bit multiplication only;
 *    - M3: 64-bit multiplication is not constant-time.
 */
#if defined(__ARM_FEATURE_DSP)

static uint64_t u32_muladd64(uint32_t x, uint32_t y, uint32_t z, uint32_t t)
{
    __asm__(
        /* UMAAL <RdLo>, <RdHi>, <Rn>, <Rm> */
        "umaal   %[z], %[t], %[x], %[y]"
        : [z] "+l" (z), [t] "+l" (t)
        : [x] "l" (x), [y] "l" (y)
    );
    return ((uint64_t) t << 32) | z;
}
#define MULADD64_ASM
#define MULADD64_SMALL

#else /* __ARM_FEATURE_DSP */

/*
 * This implementation only uses 16x16->32 bit multiplication.
 *
 * It decomposes the multiplicands as:
 *      x = xh:xl = 2^16 * xh + xl
 *      y = yh:yl = 2^16 * yh + yl
 * and computes their product as:
 *      x*y = xl*yl + 2**16 (xh*yl + yl*yh) + 2**32 xh*yh
 * then adds z and t to the result.
 */
static uint64_t u32_muladd64(uint32_t x, uint32_t y, uint32_t z, uint32_t t)
{
    /* First compute x*y, using 3 temporary registers */
    uint32_t tmp1, tmp2, tmp3;
    __asm__(
        ".syntax unified\n\t"
        /* start by splitting the inputs into halves */
        "lsrs    %[u], %[x], #16\n\t"
        "lsrs    %[v], %[y], #16\n\t"
        "uxth    %[x], %[x]\n\t"
        "uxth    %[y], %[y]\n\t"
        /* now we have %[x], %[y], %[u], %[v] = xl, yl, xh, yh */
        /* let's compute the 4 products we can form with those */
        "movs    %[w], %[v]\n\t"
        "muls    %[w], %[u]\n\t"
        "muls    %[v], %[x]\n\t"
        "muls    %[x], %[y]\n\t"
        "muls    %[y], %[u]\n\t"
        /* now we have %[x], %[y], %[v], %[w] = xl*yl, xh*yl, xl*yh, xh*yh */
        /* let's split and add the first middle product */
        "lsls    %[u], %[y], #16\n\t"
        "lsrs    %[y], %[y], #16\n\t"
        "adds    %[x], %[u]\n\t"
        "adcs    %[y], %[w]\n\t"
        /* let's finish with the second middle product */
        "lsls    %[u], %[v], #16\n\t"
        "lsrs    %[v], %[v], #16\n\t"
        "adds    %[x], %[u]\n\t"
        "adcs    %[y], %[v]\n\t"
        : [x] "+l" (x), [y] "+l" (y),
          [u] "=&l" (tmp1), [v] "=&l" (tmp2), [w] "=&l" (tmp3)
        : /* no read-only inputs */
        : "cc"
    );
    (void) tmp1;
    (void) tmp2;
    (void) tmp3;

    /* Add z and t, using one temporary register */
    __asm__(
        ".syntax unified\n\t"
        "movs    %[u], #0\n\t"
        "adds    %[x], %[z]\n\t"
        "adcs    %[y], %[u]\n\t"
        "adds    %[x], %[t]\n\t"
        "adcs    %[y], %[u]\n\t"
        : [x] "+l" (x), [y] "+l" (y), [u] "=&l" (tmp1)
        : [z] "l" (z), [t] "l" (t)
        : "cc"
    );
    (void) tmp1;

    return ((uint64_t) y << 32) | x;
}
#define MULADD64_ASM

#endif /* __ARM_FEATURE_DSP */

#endif /* GCC/Clang with Cortex-M/A CPU */

#if !defined(MULADD64_ASM)
#if defined(MUL64_IS_CONSTANT_TIME)
static uint64_t u32_muladd64(uint32_t x, uint32_t y, uint32_t z, uint32_t t)
{
    return (uint64_t) x * y + z + t;
}
#define MULADD64_SMALL
#else
static uint64_t u32_muladd64(uint32_t x, uint32_t y, uint32_t z, uint32_t t)
{
    /* x = xl + 2**16 xh, y = yl + 2**16 yh */
    const uint16_t xl = (uint16_t) x;
    const uint16_t yl = (uint16_t) y;
    const uint16_t xh = x >> 16;
    const uint16_t yh = y >> 16;

    /* x*y = xl*yl + 2**16 (xh*yl + yl*yh) + 2**32 xh*yh
     *     = lo    + 2**16 (m1    + m2   ) + 2**32 hi    */
    const uint32_t lo = (uint32_t) xl * yl;
    const uint32_t m1 = (uint32_t) xh * yl;
    const uint32_t m2 = (uint32_t) xl * yh;
    const uint32_t hi = (uint32_t) xh * yh;

    uint64_t acc = lo + ((uint64_t) (hi + (m1 >> 16) + (m2 >> 16)) << 32);
    acc += m1 << 16;
    acc += m2 << 16;
    acc += z;
    acc += t;

    return acc;
}
#endif /* MUL64_IS_CONSTANT_TIME */
#endif /* MULADD64_ASM */

/*
 * 288 + 32 x 256 -> 288-bit multiply and add
 *
 * in: x in [0, 2^32)
 *     y in [0, 2^256)
 *     z in [0, 2^288)
 * out: z_out = z_in + x * y mod 2^288
 *      c     = z_in + x * y div 2^288
 * That is, z_out + c * 2^288 = z_in + x * y
 *
 * Note: as a memory area, z must be either equal to y, or not overlap.
 *
 * This is a helper for Montgomery multiplication.
 */
static uint32_t u288_muladd(uint32_t z[9], uint32_t x, const uint32_t y[8])
{
    uint32_t carry = 0;

#define U288_MULADD_STEP(i) \
    do { \
        uint64_t prod = u32_muladd64(x, y[i], z[i], carry); \
        z[i] = (uint32_t) prod; \
        carry = (uint32_t) (prod >> 32); \
    } while( 0 )

#if defined(MULADD64_SMALL)
    U288_MULADD_STEP(0);
    U288_MULADD_STEP(1);
    U288_MULADD_STEP(2);
    U288_MULADD_STEP(3);
    U288_MULADD_STEP(4);
    U288_MULADD_STEP(5);
    U288_MULADD_STEP(6);
    U288_MULADD_STEP(7);
#else
    for (unsigned i = 0; i < 8; i++) {
        U288_MULADD_STEP(i);
    }
#endif

    uint64_t sum = (uint64_t) z[8] + carry;
    z[8] = (uint32_t) sum;
    carry = (uint32_t) (sum >> 32);

    return carry;
}

/*
 * 288-bit in-place right shift by 32 bits
 *
 * in: z in [0, 2^288)
 *     c in [0, 2^32)
 * out: z_out = z_in div 2^32 + c * 2^256
 *            = (z_in + c * 2^288) div 2^32
 *
 * This is a helper for Montgomery multiplication.
 */
static void u288_rshift32(uint32_t z[9], uint32_t c)
{
    for (unsigned i = 0; i < 8; i++) {
        z[i] = z[i + 1];
    }
    z[8] = c;
}

/*
 * 256-bit import from big-endian bytes
 *
 * in: p = p0, ..., p31
 * out: z = p0 * 2^248 + p1 * 2^240 + ... + p30 * 2^8 + p31
 */
static void u256_from_bytes(uint32_t z[8], const uint8_t p[32])
{
    for (unsigned i = 0; i < 8; i++) {
        unsigned j = 4 * (7 - i);
        z[i] = ((uint32_t) p[j + 0] << 24) |
               ((uint32_t) p[j + 1] << 16) |
               ((uint32_t) p[j + 2] <<  8) |
               ((uint32_t) p[j + 3] <<  0);
    }
}

/*
 * 256-bit export to big-endian bytes
 *
 * in: z in [0, 2^256)
 * out: p = p0, ..., p31 such that
 *      z = p0 * 2^248 + p1 * 2^240 + ... + p30 * 2^8 + p31
 */
static void u256_to_bytes(uint8_t p[32], const uint32_t z[8])
{
    for (unsigned i = 0; i < 8; i++) {
        unsigned j = 4 * (7 - i);
        p[j + 0] = (uint8_t) (z[i] >> 24);
        p[j + 1] = (uint8_t) (z[i] >> 16);
        p[j + 2] = (uint8_t) (z[i] >>  8);
        p[j + 3] = (uint8_t) (z[i] >>  0);
    }
}

/**********************************************************************
 *
 * Operations modulo a 256-bit prime m
 *
 * These are done in the Montgomery domain, that is x is represented by
 *  x * 2^256 mod m
 * Numbers need to be converted to that domain before computations,
 * and back from it afterwards.
 *
 * Inversion is computed using Fermat's little theorem.
 *
 * Assumptions on m:
 * - Montgomery operations require that m is odd.
 * - Fermat's little theorem require it to be a prime.
 * - m256_inv() further requires that m % 2^32 >= 2.
 * - m256_inv() also assumes that the value of m is not a secret.
 *
 * In practice operations are done modulo the curve's p and n,
 * both of which satisfy those assumptions.
 *
 **********************************************************************/

/*
 * Data associated to a modulus for Montgomery operations.
 *
 * m in [0, 2^256) - the modulus itself, must be odd
 * R2 = 2^512 mod m
 * ni = -m^-1 mod 2^32
 */
typedef struct {
    uint32_t m[8];
    uint32_t R2[8];
    uint32_t ni;
}
m256_mod;

/*
 * Data for Montgomery operations modulo the curve's p
 */
static const m256_mod p256_p = {
    {   /* the curve's p */
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
        0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF,
    },
    {   /* 2^512 mod p */
        0x00000003, 0x00000000, 0xffffffff, 0xfffffffb,
        0xfffffffe, 0xffffffff, 0xfffffffd, 0x00000004,
    },
    0x00000001, /* -p^-1 mod 2^32 */
};

/*
 * Data for Montgomery operations modulo the curve's n
 */
static const m256_mod p256_n = {
    {   /* the curve's n */
        0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD,
        0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF,
    },
    {   /* 2^512 mod n */
        0xbe79eea2, 0x83244c95, 0x49bd6fa6, 0x4699799c,
        0x2b6bec59, 0x2845b239, 0xf3d95620, 0x66e12d94,
    },
    0xee00bc4f, /* -n^-1 mod 2^32 */
};

/*
 * Modular addition
 *
 * in: x, y in [0, m)
 *     mod must point to a valid m256_mod structure
 * out: z = (x + y) mod m, in [0, m)
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static void m256_add(uint32_t z[8],
                     const uint32_t x[8], const uint32_t y[8],
                     const m256_mod *mod)
{
    uint32_t r[8];
    uint32_t carry_add = u256_add(z, x, y);
    uint32_t carry_sub = u256_sub(r, z, mod->m);
    /* Need to subract m if:
     *      x+y >= 2^256 > m (that is, carry_add == 1)
     *   OR z >= m (that is, carry_sub == 0) */
    uint32_t use_sub = carry_add | (1 - carry_sub);
    u256_cmov(z, r, use_sub);
}

/*
 * Modular addition mod p
 *
 * in: x, y in [0, p)
 * out: z = (x + y) mod p, in [0, p)
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static void m256_add_p(uint32_t z[8],
                       const uint32_t x[8], const uint32_t y[8])
{
    m256_add(z, x, y, &p256_p);
}

/*
 * Modular subtraction
 *
 * in: x, y in [0, m)
 *     mod must point to a valid m256_mod structure
 * out: z = (x - y) mod m, in [0, m)
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static void m256_sub(uint32_t z[8],
                     const uint32_t x[8], const uint32_t y[8],
                     const m256_mod *mod)
{
    uint32_t r[8];
    uint32_t carry = u256_sub(z, x, y);
    (void) u256_add(r, z, mod->m);
    /* Need to add m if and only if x < y, that is carry == 1.
     * In that case z is in [2^256 - m + 1, 2^256 - 1], so the
     * addition will have a carry as well, which cancels out. */
    u256_cmov(z, r, carry);
}

/*
 * Modular subtraction mod p
 *
 * in: x, y in [0, p)
 * out: z = (x + y) mod p, in [0, p)
 *
 * Note: as a memory area, z must be either equal to x or y, or not overlap.
 */
static void m256_sub_p(uint32_t z[8],
                       const uint32_t x[8], const uint32_t y[8])
{
    m256_sub(z, x, y, &p256_p);
}

/*
 * Montgomery modular multiplication
 *
 * in: x, y in [0, m)
 *     mod must point to a valid m256_mod structure
 * out: z = (x * y) / 2^256 mod m, in [0, m)
 *
 * Note: as a memory area, z may overlap with x or y.
 */
static void m256_mul(uint32_t z[8],
                     const uint32_t x[8], const uint32_t y[8],
                     const m256_mod *mod)
{
    /*
     * Algorithm 14.36 in Handbook of Applied Cryptography with:
     * b = 2^32, n = 8, R = 2^256
     */
    uint32_t m_prime = mod->ni;
    uint32_t a[9];

    for (unsigned i = 0; i < 9; i++) {
        a[i] = 0;
    }

    for (unsigned i = 0; i < 8; i++) {
        /* the "mod 2^32" is implicit from the type */
        uint32_t u = (a[0] + x[i] * y[0]) * m_prime;

        /* a = (a + x[i] * y + u * m) div b */
        uint32_t c = u288_muladd(a, x[i], y);
        c += u288_muladd(a, u, mod->m);
        u288_rshift32(a, c);
    }

    /* a = a > m ? a - m : a */
    uint32_t carry_add = a[8];  // 0 or 1 since a < 2m, see HAC Note 14.37
    uint32_t carry_sub = u256_sub(z, a, mod->m);
    uint32_t use_sub = carry_add | (1 - carry_sub);     // see m256_add()
    u256_cmov(z, a, 1 - use_sub);
}

/*
 * Montgomery modular multiplication modulo p.
 *
 * in: x, y in [0, p)
 * out: z = (x * y) / 2^256 mod p, in [0, p)
 *
 * Note: as a memory area, z may overlap with x or y.
 */
static void m256_mul_p(uint32_t z[8],
                       const uint32_t x[8], const uint32_t y[8])
{
    m256_mul(z, x, y, &p256_p);
}

/*
 * In-place conversion to Montgomery form
 *
 * in: z in [0, m)
 *     mod must point to a valid m256_mod structure
 * out: z_out = z_in * 2^256 mod m, in [0, m)
 */
static void m256_prep(uint32_t z[8], const m256_mod *mod)
{
    m256_mul(z, z, mod->R2, mod);
}

/*
 * In-place conversion from Montgomery form
 *
 * in: z in [0, m)
 *     mod must point to a valid m256_mod structure
 * out: z_out = z_in / 2^256 mod m, in [0, m)
 * That is, z_in was z_actual * 2^256 mod m, and z_out is z_actual
 */
static void m256_done(uint32_t z[8], const m256_mod *mod)
{
    uint32_t one[8];
    u256_set32(one, 1);
    m256_mul(z, z, one, mod);
}

/*
 * Set to 32-bit value
 *
 * in: x in [0, 2^32)
 *     mod must point to a valid m256_mod structure
 * out: z = x * 2^256 mod m, in [0, m)
 * That is, z is set to the image of x in the Montgomery domain.
 */
static void m256_set32(uint32_t z[8], uint32_t x, const m256_mod *mod)
{
    u256_set32(z, x);
    m256_prep(z, mod);
}

/*
 * Modular inversion in Montgomery form
 *
 * in: x in [0, m)
 *     mod must point to a valid m256_mod structure
 *     such that mod->m % 2^32 >= 2, assumed to be public.
 * out: z = x^-1 * 2^512 mod m if x != 0,
 *      z = 0 if x == 0
 * That is, if x = x_actual    * 2^256 mod m, then
 *             z = x_actual^-1 * 2^256 mod m
 *
 * Note: as a memory area, z may overlap with x.
 */
static void m256_inv(uint32_t z[8], const uint32_t x[8],
                     const m256_mod *mod)
{
    /*
     * Use Fermat's little theorem to compute x^-1 as x^(m-2).
     *
     * Take advantage of the fact that both p's and n's least significant limb
     * is at least 2 to perform the subtraction on the flight (no carry).
     *
     * Use plain right-to-left binary exponentiation;
     * branches are OK as the exponent is not a secret.
     */
    uint32_t bitval[8];
    u256_cmov(bitval, x, 1);    /* copy x before writing to z */

    m256_set32(z, 1, mod);

    unsigned i = 0;
    uint32_t limb = mod->m[i] - 2;
    while (1) {
        for (unsigned j = 0; j < 32; j++) {
            if ((limb & 1) != 0) {
                m256_mul(z, z, bitval, mod);
            }
            m256_mul(bitval, bitval, bitval, mod);
            limb >>= 1;
        }

        if (i == 7)
            break;

        i++;
        limb = mod->m[i];
    }
}

/*
 * Import modular integer from bytes to Montgomery domain
 *
 * in: p = p0, ..., p32
 *     mod must point to a valid m256_mod structure
 * out: z = (p0 * 2^248 + ... + p31) * 2^256 mod m, in [0, m)
 *      return 0 if the number was already in [0, m), or -1.
 *      z may be incorrect and must be discared when -1 is returned.
 */
static int m256_from_bytes(uint32_t z[8],
                           const uint8_t p[32], const m256_mod *mod)
{
    u256_from_bytes(z, p);

    uint32_t t[8];
    uint32_t lt_m = u256_sub(t, z, mod->m);
    if (lt_m != 1)
        return -1;

    m256_prep(z, mod);
    return 0;
}

/*
 * Export modular integer from Montgomery domain to bytes
 *
 * in: z in [0, 2^256)
 *     mod must point to a valid m256_mod structure
 * out: p = p0, ..., p31 such that
 *      z = (p0 * 2^248 + ... + p31) * 2^256 mod m
 */
static void m256_to_bytes(uint8_t p[32],
                          const uint32_t z[8], const m256_mod *mod)
{
    uint32_t zi[8];
    u256_cmov(zi, z, 1);
    m256_done(zi, mod);

    u256_to_bytes(p, zi);
}

/**********************************************************************
 *
 * Operations on curve points
 *
 * Points are represented in two coordinates system:
 *  - affine (x, y) - extended to represent 0 (see below)
 *  - jacobian (x:y:z)
 * In either case, coordinates are integers modulo p256_p and
 * are always represented in the Montgomery domain.
 *
 * For background on jacobian coordinates, see for example [GECC] 3.2.2:
 * - conversions go (x, y) -> (x:y:1) and (x:y:z) -> (x/z^2, y/z^3)
 * - the curve equation becomes y^2 = x^3 - 3 x z^4 + b z^6
 * - 0 (aka the origin aka point at infinity) is (x:y:0) with y^2 = x^3.
 * - point negation goes -(x:y:z) = (x:-y:z)
 *
 * Normally 0 (the point at infinity) can't be represented in affine
 * coordinates. However we extend affine coordinates with the convention that
 * (0, 0) (which is normally not a point on the curve) is interpreted as 0.
 *
 * References:
 * - [GECC]: Guide to Elliptic Curve Cryptography; Hankerson, Menezes,
 *   Vanstone; Springer, 2004.
 * - [CMO98]: Efficient Elliptic Curve Exponentiation Using Mixed Coordinates;
 *   Cohen, Miyaji, Ono; Springer, ASIACRYPT 1998.
 *   https://link.springer.com/content/pdf/10.1007/3-540-49649-1_6.pdf
 * - [RCB15]: Complete addition formulas for prime order elliptic curves;
 *   Renes, Costello, Batina; IACR e-print 2015-1060.
 *   https://eprint.iacr.org/2015/1060.pdf
 *
 **********************************************************************/

/*
 * The curve's b parameter in the Short Weierstrass equation
 *  y^2 = x^3 - 3*x + b
 * Compared to the standard, this is converted to the Montgomery domain.
 */
static const uint32_t p256_b[8] = { /* b * 2^256 mod p */
    0x29c4bddf, 0xd89cdf62, 0x78843090, 0xacf005cd,
    0xf7212ed6, 0xe5a220ab, 0x04874834, 0xdc30061d,
};

/*
 * The curve's conventional base point G.
 * Compared to the standard, coordinates converted to the Montgomery domain.
 */
static const uint32_t p256_gx[8] = { /* G_x * 2^256 mod p */
    0x18a9143c, 0x79e730d4, 0x5fedb601, 0x75ba95fc,
    0x77622510, 0x79fb732b, 0xa53755c6, 0x18905f76,
};
static const uint32_t p256_gy[8] = { /* G_y * 2^256 mod p */
    0xce95560a, 0xddf25357, 0xba19e45c, 0x8b4ab8e4,
    0xdd21f325, 0xd2e88688, 0x25885d85, 0x8571ff18,
};

/*
 * Point-on-curve check - do the coordinates satisfy the curve's equation?
 *
 * in: x, y in [0, p)   (Montgomery domain)
 * out: 0 if the point lies on the curve and is not 0,
 *      unspecified non-zero otherwise
 */
static uint32_t point_check(const uint32_t x[8], const uint32_t y[8])
{
    uint32_t lhs[8], rhs[8];

    /* lhs = y^2 */
    m256_mul_p(lhs, y, y);

    /* rhs = x^3 - 3x + b */
    m256_mul_p(rhs, x,   x);      /* x^2 */
    m256_mul_p(rhs, rhs, x);      /* x^3 */
    for (unsigned i = 0; i < 3; i++)
        m256_sub_p(rhs, rhs, x);  /* x^3 - 3x */
    m256_add_p(rhs, rhs, p256_b); /* x^3 - 3x + b */

    return u256_diff(lhs, rhs);
}

/*
 * In-place jacobian to affine coordinate conversion
 *
 * in: (x:y:z) must be on the curve (coordinates in Montegomery domain)
 * out: x_out = x_in / z_in^2   (Montgomery domain)
 *      y_out = y_in / z_in^3   (Montgomery domain)
 *      z_out unspecified, must be disregarded
 *
 * Note: if z is 0 (that is, the input point is 0), x_out = y_out = 0.
 */
static void point_to_affine(uint32_t x[8], uint32_t y[8], uint32_t z[8])
{
    uint32_t t[8];

    m256_inv(z, z, &p256_p);    /* z = z^-1 */

    m256_mul_p(t, z, z);        /* t = z^-2 */
    m256_mul_p(x, x, t);        /* x = x * z^-2 */

    m256_mul_p(t, t, z);        /* t = z^-3 */
    m256_mul_p(y, y, t);        /* y = y * z^-3 */
}

/*
 * In-place point doubling in jacobian coordinates (Montgomery domain)
 *
 * in: P_in = (x:y:z), must be on the curve
 * out: (x:y:z) = P_out = 2 * P_in
 */
static void point_double(uint32_t x[8], uint32_t y[8], uint32_t z[8])
{
    /*
     * This is formula 6 from [CMO98], cited as complete in [RCB15] (table 1).
     * Notations as in the paper, except u added and t ommited (it's x3).
     */
    uint32_t m[8], s[8], u[8];

    /* m = 3 * x^2 + a * z^4 = 3 * (x + z^2) * (x - z^2) */
    m256_mul_p(s, z, z);
    m256_add_p(m, x, s);
    m256_sub_p(u, x, s);
    m256_mul_p(s, m, u);
    m256_add_p(m, s, s);
    m256_add_p(m, m, s);

    /* s = 4 * x * y^2 */
    m256_mul_p(u, y, y);
    m256_add_p(u, u, u); /* u = 2 * y^2 (used below) */
    m256_mul_p(s, x, u);
    m256_add_p(s, s, s);

    /* u = 8 * y^4 (not named in the paper, first term of y3) */
    m256_mul_p(u, u, u);
    m256_add_p(u, u, u);

    /* x3 = t = m^2 - 2 * s */
    m256_mul_p(x, m, m);
    m256_sub_p(x, x, s);
    m256_sub_p(x, x, s);

    /* z3 = 2 * y * z */
    m256_mul_p(z, y, z);
    m256_add_p(z, z, z);

    /* y3 = -u + m * (s - t) */
    m256_sub_p(y, s, x);
    m256_mul_p(y, y, m);
    m256_sub_p(y, y, u);
}

/*
 * In-place point addition in jacobian-affine coordinates (Montgomery domain)
 *
 * in: P_in = (x1:y1:z1), must be on the curve and not 0
 *     Q = (x2, y2), must be on the curve and not P_in or -P_in or 0
 * out: P_out = (x1:y1:z1) = P_in + Q
 */
static void point_add(uint32_t x1[8], uint32_t y1[8], uint32_t z1[8],
                      const uint32_t x2[8], const uint32_t y2[8])
{
    /*
     * This is formula 5 from [CMO98], with z2 == 1 substituted. We use
     * intermediates with neutral names, and names from the paper in comments.
     */
    uint32_t t1[8], t2[8], t3[8];

    /* u1 = x1 and s1 = y1 (no computations) */

    /* t1 = u2 = x2 z1^2 */
    m256_mul_p(t1, z1, z1);
    m256_mul_p(t2, t1, z1);
    m256_mul_p(t1, t1, x2);

    /* t2 = s2 = y2 z1^3 */
    m256_mul_p(t2, t2, y2);

    /* t1 = h = u2 - u1 */
    m256_sub_p(t1, t1, x1); /* t1 = x2 * z1^2 - x1 */

    /* t2 = r = s2 - s1 */
    m256_sub_p(t2, t2, y1);

    /* z3 = z1 * h */
    m256_mul_p(z1, z1, t1);

    /* t1 = h^3 */
    m256_mul_p(t3, t1, t1);
    m256_mul_p(t1, t3, t1);

    /* t3 = x1 * h^2 */
    m256_mul_p(t3, t3, x1);

    /* x3 = r^2 - 2 * x1 * h^2 - h^3 */
    m256_mul_p(x1, t2, t2);
    m256_sub_p(x1, x1, t3);
    m256_sub_p(x1, x1, t3);
    m256_sub_p(x1, x1, t1);

    /* y3 = r * (x1 * h^2 - x3) - y1 h^3 */
    m256_sub_p(t3, t3, x1);
    m256_mul_p(t3, t3, t2);
    m256_mul_p(t1, t1, y1);
    m256_sub_p(y1, t3, t1);
}

/*
 * Point addition or doubling (affine to jacobian, Montgomery domain)
 *
 * in: P = (x1, y1) - must be on the curve and not 0
 *     Q = (x2, y2) - must be on the curve and not 0
 * out: (x3, y3) = R = P + Q
 *
 * Note: unlike point_add(), this function works if P = +- Q;
 * however it leaks information on its input through timing,
 * branches taken and memory access patterns (if observable).
 */
static void point_add_or_double_leaky(
                        uint32_t x3[8], uint32_t y3[8],
                        const uint32_t x1[8], const uint32_t y1[8],
                        const uint32_t x2[8], const uint32_t y2[8])
{

    uint32_t z3[8];
    u256_cmov(x3, x1, 1);
    u256_cmov(y3, y1, 1);
    m256_set32(z3, 1, &p256_p);

    if (u256_diff(x1, x2) != 0) {
        // P != +- Q -> generic addition
        point_add(x3, y3, z3, x2, y2);
        point_to_affine(x3, y3, z3);
    }
    else if (u256_diff(y1, y2) == 0) {
        // P == Q -> double
        point_double(x3, y3, z3);
        point_to_affine(x3, y3, z3);
    } else {
        // P == -Q -> zero
        m256_set32(x3, 0, &p256_p);
        m256_set32(y3, 0, &p256_p);
    }
}

/*
 * Import curve point from bytes
 *
 * in: p = (x, y) concatenated, fixed-width 256-bit big-endian integers
 * out: x, y in Mongomery domain
 *      return 0 if x and y are both in [0, p)
 *                  and (x, y) is on the curve and not 0
 *             unspecified non-zero otherwise.
 *      x and y are unspecified and must be discarded if returning non-zero.
 */
static int point_from_bytes(uint32_t x[8], uint32_t y[8], const uint8_t p[64])
{
    int ret;

    ret = m256_from_bytes(x, p, &p256_p);
    if (ret != 0)
        return ret;

    ret = m256_from_bytes(y, p + 32, &p256_p);
    if (ret != 0)
        return ret;

    return (int) point_check(x, y);
}

/*
 * Export curve point to bytes
 *
 * in: x, y affine coordinates of a point (Montgomery domain)
 *     must be on the curve and not 0
 * out: p = (x, y) concatenated, fixed-width 256-bit big-endian integers
 */
static void point_to_bytes(uint8_t p[64],
                           const uint32_t x[8], const uint32_t y[8])
{
    m256_to_bytes(p,        x, &p256_p);
    m256_to_bytes(p + 32,   y, &p256_p);
}

/**********************************************************************
 *
 * Scalar multiplication and other scalar-related operations
 *
 **********************************************************************/

/*
 * Scalar multiplication
 *
 * in: P = (px, py), affine (Montgomery), must be on the curve and not 0
 *     s in [1, n-1]
 * out: R = s * P = (rx, ry), affine coordinates (Montgomery).
 *
 * Note: as memory areas, none of the parameters may overlap.
 */
static void scalar_mult(uint32_t rx[8], uint32_t ry[8],
                        const uint32_t px[8], const uint32_t py[8],
                        const uint32_t s[8])
{
    /*
     * We use a signed binary ladder, see for example slides 10-14 of
     * http://ecc2015.math.u-bordeaux1.fr/documents/hamburg.pdf but with
     * implicit recoding, and a different loop initialisation to avoid feeding
     * 0 to our addition formulas, as they don't support it.
     */
    uint32_t s_odd[8], py_neg[8], py_use[8], rz[8];

    /*
     * Make s odd by replacing it with n - s if necessary.
     *
     * If s was odd, we'll have s_odd = s, and define P' = P.
     * Otherwise, we'll have s_odd = n - s and define P' = -P.
     *
     * Either way, we can compute s * P as s_odd * P'.
     */
    u256_sub(s_odd, p256_n.m, s); /* no carry, result still in [1, n-1] */
    uint32_t negate = ~s[0] & 1;
    u256_cmov(s_odd, s, 1 - negate);

    /* Compute py_neg = - py mod p (that's the y coordinate of -P) */
    u256_set32(py_use, 0);
    m256_sub_p(py_neg, py_use, py);

    /* Initialize R = P' = (x:(-1)^negate * y:1) */
    u256_cmov(rx, px, 1);
    u256_cmov(ry, py, 1);
    m256_set32(rz, 1, &p256_p);
    u256_cmov(ry, py_neg, negate);

    /*
     * For any odd number s_odd = b255 ... b1 1, we have
     *      s_odd = 2^255 + 2^254 sbit(b255) + ... + 2 sbit(b2) + sbit(b1)
     * writing
     *      sbit(b) = 2 * b - 1 = b ? 1 : -1
     *
     * Use that to compute s_odd * P' by repeating R = 2 * R +- P':
     *      s_odd * P' = 2 * ( ... (2 * P' + sbit(b255) P') ... ) + sbit(b1) P'
     *
     * The loop invariant is that when beginning an iteration we have
     *      R = s_i P'
     * with
     *      s_i = 2^(255-i) + 2^(254-i) sbit(b_255) + ...
     * where the sum has 256 - i terms.
     *
     * When updating R we need to make sure the input to point_add() is
     * neither 0 not +-P'. Since that input is 2 s_i P', it is sufficient to
     * see that 1 < 2 s_i < n-1. The lower bound is obvious since s_i is a
     * positive integer, and for the upper bound we distinguish three cases.
     *
     * If i > 1, then s_i < 2^254, so 2 s_i < 2^255 < n-1.
     * Otherwise, i == 1 and we have 2 s_i = s_odd - sbit(b1).
     *      If s_odd <= n-4, then 2 s_1 <= n-3.
     *      Otherwise, s_odd = n-2, and for this curve's value of n,
     *      we have b1 == 1, so sbit(b1) = 1 and 2 s_1 <= n-3.
     */
    for (unsigned i = 255; i > 0; i--) {
        uint32_t bit = (s_odd[i / 32] >> i % 32) & 1;

        /* set (px, py_use) = sbit(bit) P' = sbit(bit) * (-1)^negate P */
        u256_cmov(py_use, py, bit ^ negate);
        u256_cmov(py_use, py_neg, (1 - bit) ^ negate);

        /* Update R = 2 * R +- P' */
        point_double(rx, ry, rz);
        point_add(rx, ry, rz, px, py_use);
    }

    point_to_affine(rx, ry, rz);
}

/*
 * Scalar import from big-endian bytes
 *
 * in: p = p0, ..., p31
 * out: s = p0 * 2^248 + p1 * 2^240 + ... + p30 * 2^8 + p31
 *      return 0 if s in [1, n-1],
 *            -1 otherwise.
 */
static int scalar_from_bytes(uint32_t s[8], const uint8_t p[32])
{
    u256_from_bytes(s, p);

    uint32_t r[8];
    uint32_t lt_n = u256_sub(r, s, p256_n.m);

    u256_set32(r, 1);
    uint32_t lt_1 = u256_sub(r, s, r);

    if (lt_n && !lt_1)
        return 0;

    return -1;
}

/* Using RNG functions from Mbed TLS as p256-m does not come with a
 * cryptographically secure RNG function.
 */
int p256_generate_random(uint8_t *output, unsigned output_size)
{
    int ret;
    ret = psa_generate_random(output, output_size);

    if (ret != 0){
        return P256_RANDOM_FAILED;
    }
    return P256_SUCCESS;
}

/*
 * Scalar generation, with public key
 *
 * out: sbytes the big-endian bytes representation of the scalar
 *      s its u256 representation
 *      x, y the affine coordinates of s * G (Montgomery domain)
 *      return 0 if OK, -1 on failure
 *      sbytes, s, x, y must be discarded when returning non-zero.
 */
static int scalar_gen_with_pub(uint8_t sbytes[32], uint32_t s[8],
                               uint32_t x[8], uint32_t y[8])
{
    /* generate a random valid scalar */
    int ret;
    unsigned nb_tried = 0;
    do {
        if (nb_tried++ >= 4)
            return -1;

        ret = p256_generate_random(sbytes, 32);
        CT_POISON(sbytes, 32);
        if (ret != 0)
            return -1;

        ret = scalar_from_bytes(s, sbytes);
        CT_UNPOISON(&ret, sizeof ret);
    }
    while (ret != 0);

    /* compute and ouput the associated public key */
    scalar_mult(x, y, p256_gx, p256_gy, s);

    /* the associated public key is not a secret */
    CT_UNPOISON(x, 32);
    CT_UNPOISON(y, 32);

    return 0;
}

/*
 * ECDH/ECDSA generate pair
 */
int p256_gen_keypair(uint8_t priv[32], uint8_t pub[64])
{
    uint32_t s[8], x[8], y[8];
    int ret = scalar_gen_with_pub(priv, s, x, y);
    zeroize(s, sizeof s);
    if (ret != 0)
        return P256_RANDOM_FAILED;

    point_to_bytes(pub, x, y);
    return 0;
}

/**********************************************************************
 *
 * ECDH
 *
 **********************************************************************/

/*
 * ECDH compute shared secret
 */
int p256_ecdh_shared_secret(uint8_t secret[32],
                            const uint8_t priv[32], const uint8_t peer[64])
{
    CT_POISON(priv, 32);

    uint32_t s[8], px[8], py[8], x[8], y[8];
    int ret;

    ret = scalar_from_bytes(s, priv);
    CT_UNPOISON(&ret, sizeof ret);
    if (ret != 0) {
        ret = P256_INVALID_PRIVKEY;
        goto cleanup;
    }

    ret = point_from_bytes(px, py, peer);
    if (ret != 0) {
        ret = P256_INVALID_PUBKEY;
        goto cleanup;
    }

    scalar_mult(x, y, px, py, s);

    m256_to_bytes(secret, x, &p256_p);
    CT_UNPOISON(secret, 32);

cleanup:
    zeroize(s, sizeof s);
    return ret;
}

/**********************************************************************
 *
 * ECDSA
 *
 * Reference:
 * [SEC1] SEC 1: Elliptic Curve Cryptography, Certicom research, 2009.
 *        http://www.secg.org/sec1-v2.pdf
 **********************************************************************/

/*
 * Reduction mod n of a small number
 *
 * in: x in [0, 2^256)
 * out: x_out = x_in mod n in [0, n)
 */
static void ecdsa_m256_mod_n(uint32_t x[8])
{
    uint32_t t[8];
    uint32_t c = u256_sub(t, x, p256_n.m);
    u256_cmov(x, t, 1 - c);
}

/*
 * Import integer mod n (Montgomery domain) from hash
 *
 * in: h = h0, ..., h_hlen
 *     hlen the length of h in bytes
 * out: z = (h0 * 2^l-8 + ... + h_l) * 2^256 mod n
 *      with l = min(32, hlen)
 *
 * Note: in [SEC1] this is step 5 of 4.1.3 (sign) or step 3 or 4.1.4 (verify),
 * with obvious simplications since n's bit-length is a multiple of 8.
 */
static void ecdsa_m256_from_hash(uint32_t z[8],
                                 const uint8_t *h, size_t hlen)
{
    /* convert from h (big-endian) */
    /* hlen is public data so it's OK to branch on it */
    if (hlen < 32) {
        uint8_t p[32];
        for (unsigned i = 0; i < 32; i++)
            p[i] = 0;
        for (unsigned i = 0; i < hlen; i++)
            p[32 - hlen + i] = h[i];
        u256_from_bytes(z, p);
    } else {
        u256_from_bytes(z, h);
    }

    /* ensure the result is in [0, n) */
    ecdsa_m256_mod_n(z);

    /* map to Montgomery domain */
    m256_prep(z, &p256_n);
}

/*
 * ECDSA sign
 */
int p256_ecdsa_sign(uint8_t sig[64], const uint8_t priv[32],
                    const uint8_t *hash, size_t hlen)
{
    CT_POISON(priv, 32);

    /*
     * Steps and notations from [SEC1] 4.1.3
     *
     * Instead of retrying on r == 0 or s == 0, just abort,
     * as those events have negligible probability.
     */
    int ret;

    /* Temporary buffers - the first two are mostly stable, so have names */
    uint32_t xr[8], k[8], t3[8], t4[8];

    /* 1. Set ephemeral keypair */
    uint8_t *kb = (uint8_t *) t4;
    /* kb will be erased by re-using t4 for dU - if we exit before that, we
     * haven't read the private key yet so we kb isn't sensitive yet */
    ret = scalar_gen_with_pub(kb, k, xr, t3);   /* xr = x_coord(k * G) */
    if (ret != 0)
        return P256_RANDOM_FAILED;
    m256_prep(k, &p256_n);

    /* 2. Convert xr to an integer */
    m256_done(xr, &p256_p);

    /* 3. Reduce xr mod n (extra: output it while at it) */
    ecdsa_m256_mod_n(xr);    /* xr = int(xr) mod n */

    /* xr is public data so it's OK to use a branch */
    if (u256_diff0(xr) == 0)
        return P256_RANDOM_FAILED;

    u256_to_bytes(sig, xr);

    m256_prep(xr, &p256_n);

    /* 4. Skipped - we take the hash as an input, not the message */

    /* 5. Derive an integer from the hash */
    ecdsa_m256_from_hash(t3, hash, hlen);   /* t3 = e */

    /* 6. Compute s = k^-1 * (e + r * dU) */

    /* Note: dU will be erased by re-using t4 for the value of s (public) */
    ret = scalar_from_bytes(t4, priv);   /* t4 = dU (integer domain) */
    CT_UNPOISON(&ret, sizeof ret); /* Result of input validation */
    if (ret != 0)
        return P256_INVALID_PRIVKEY;
    m256_prep(t4, &p256_n);         /* t4 = dU (Montgomery domain) */

    m256_inv(k, k, &p256_n);        /* k^-1 */
    m256_mul(t4, xr, t4, &p256_n);  /* t4 = r * dU */
    m256_add(t4, t3, t4, &p256_n);  /* t4 = e + r * dU */
    m256_mul(t4, k, t4, &p256_n);   /* t4 = s = k^-1 * (e + r * dU) */
    zeroize(k, sizeof k);

    /* 7. Output s (r already outputed at step 3) */
    CT_UNPOISON(t4, 32);
    if (u256_diff0(t4) == 0) {
        /* undo early output of r */
        u256_to_bytes(sig, t4);
        return P256_RANDOM_FAILED;
    }
    m256_to_bytes(sig + 32, t4, &p256_n);

    return P256_SUCCESS;
}

/*
 * ECDSA verify
 */
int p256_ecdsa_verify(const uint8_t sig[64], const uint8_t pub[64],
                      const uint8_t *hash, size_t hlen)
{
    /*
     * Steps and notations from [SEC1] 4.1.3
     *
     * Note: we're using public data only, so branches are OK
     */
    int ret;

    /* 1. Validate range of r and s : [1, n-1] */
    uint32_t r[8], s[8];
    ret = scalar_from_bytes(r, sig);
    if (ret != 0)
        return P256_INVALID_SIGNATURE;
    ret = scalar_from_bytes(s, sig + 32);
    if (ret != 0)
        return P256_INVALID_SIGNATURE;

    /* 2. Skipped - we take the hash as an input, not the message */

    /* 3. Derive an integer from the hash */
    uint32_t e[8];
    ecdsa_m256_from_hash(e, hash, hlen);

    /* 4. Compute u1 = e * s^-1 and u2 = r * s^-1 */
    uint32_t u1[8], u2[8];
    m256_prep(s, &p256_n);           /* s in Montgomery domain */
    m256_inv(s, s, &p256_n);         /* s = s^-1 mod n */
    m256_mul(u1, e, s, &p256_n);     /* u1 = e * s^-1 mod n */
    m256_done(u1, &p256_n);          /* u1 out of Montgomery domain */

    u256_cmov(u2, r, 1);
    m256_prep(u2, &p256_n);          /* r in Montgomery domain */
    m256_mul(u2, u2, s, &p256_n);    /* u2 = r * s^-1 mod n */
    m256_done(u2, &p256_n);          /* u2 out of Montgomery domain */

    /* 5. Compute R (and re-use (u1, u2) to store its coordinates */
    uint32_t px[8], py[8];
    ret = point_from_bytes(px, py, pub);
    if (ret != 0)
        return P256_INVALID_PUBKEY;

    scalar_mult(e, s, px, py, u2);      /* (e, s) = R2 = u2 * Qu */

    if (u256_diff0(u1) == 0) {
        /* u1 out of range for scalar_mult() - just skip it */
        u256_cmov(u1, e, 1);
        /* we don't care about the y coordinate */
    } else {
        scalar_mult(px, py, p256_gx, p256_gy, u1); /* (px, py) = R1 = u1 * G */

        /* (u1, u2) = R = R1 + R2 */
        point_add_or_double_leaky(u1, u2, px, py, e, s);
        /* No need to check if R == 0 here: if that's the case, it will be
         * caught when comparating rx (which will be 0) to r (which isn't). */
    }

    /* 6. Convert xR to an integer */
    m256_done(u1, &p256_p);

    /* 7. Reduce xR mod n */
    ecdsa_m256_mod_n(u1);

    /* 8. Compare xR mod n to r */
    uint32_t diff = u256_diff(u1, r);
    if (diff == 0)
        return P256_SUCCESS;

    return P256_INVALID_SIGNATURE;
}

/**********************************************************************
 *
 * Key management utilities
 *
 **********************************************************************/

int p256_validate_pubkey(const uint8_t pub[64])
{
    uint32_t x[8], y[8];
    int ret = point_from_bytes(x, y, pub);

    return ret == 0 ? P256_SUCCESS : P256_INVALID_PUBKEY;
}

int p256_validate_privkey(const uint8_t priv[32])
{
    uint32_t s[8];
    int ret = scalar_from_bytes(s, priv);
    zeroize(s, sizeof(s));

    return ret == 0 ? P256_SUCCESS : P256_INVALID_PRIVKEY;
}

int p256_public_from_private(uint8_t pub[64], const uint8_t priv[32])
{
    int ret;
    uint32_t s[8];

    ret = scalar_from_bytes(s, priv);
    if (ret != 0)
        return P256_INVALID_PRIVKEY;

    /* compute and ouput the associated public key */
    uint32_t x[8], y[8];
    scalar_mult(x, y, p256_gx, p256_gy, s);

    /* the associated public key is not a secret, the scalar was */
    CT_UNPOISON(x, 32);
    CT_UNPOISON(y, 32);
    zeroize(s, sizeof(s));

    point_to_bytes(pub, x, y);
    return P256_SUCCESS;
}

#endif
