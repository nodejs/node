/*
 *  ARIA implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * This implementation is based on the following standards:
 * [1] http://210.104.33.10/ARIA/doc/ARIA-specification-e.pdf
 * [2] https://tools.ietf.org/html/rfc5794
 */

#include "common.h"

#if defined(MBEDTLS_ARIA_C)

#include "mbedtls/aria.h"

#include <string.h>

#include "mbedtls/platform.h"

#if !defined(MBEDTLS_ARIA_ALT)

#include "mbedtls/platform_util.h"

/*
 * modify byte order: ( A B C D ) -> ( B A D C ), i.e. swap pairs of bytes
 *
 * This is submatrix P1 in [1] Appendix B.1
 *
 * Common compilers fail to translate this to minimal number of instructions,
 * so let's provide asm versions for common platforms with C fallback.
 */
#if defined(MBEDTLS_HAVE_ASM)
#if defined(__arm__) /* rev16 available from v6 up */
/* armcc5 --gnu defines __GNUC__ but doesn't support GNU's extended asm */
#if defined(__GNUC__) && \
    (!defined(__ARMCC_VERSION) || __ARMCC_VERSION >= 6000000) && \
    __ARM_ARCH >= 6
static inline uint32_t aria_p1(uint32_t x)
{
    uint32_t r;
    __asm("rev16 %0, %1" : "=l" (r) : "l" (x));
    return r;
}
#define ARIA_P1 aria_p1
#elif defined(__ARMCC_VERSION) && __ARMCC_VERSION < 6000000 && \
    (__TARGET_ARCH_ARM >= 6 || __TARGET_ARCH_THUMB >= 3)
static inline uint32_t aria_p1(uint32_t x)
{
    uint32_t r;
    __asm("rev16 r, x");
    return r;
}
#define ARIA_P1 aria_p1
#endif
#endif /* arm */
#if defined(__GNUC__) && \
    defined(__i386__) || defined(__amd64__) || defined(__x86_64__)
/* I couldn't find an Intel equivalent of rev16, so two instructions */
#define ARIA_P1(x) ARIA_P2(ARIA_P3(x))
#endif /* x86 gnuc */
#endif /* MBEDTLS_HAVE_ASM && GNUC */
#if !defined(ARIA_P1)
#define ARIA_P1(x) ((((x) >> 8) & 0x00FF00FF) ^ (((x) & 0x00FF00FF) << 8))
#endif

/*
 * modify byte order: ( A B C D ) -> ( C D A B ), i.e. rotate by 16 bits
 *
 * This is submatrix P2 in [1] Appendix B.1
 *
 * Common compilers will translate this to a single instruction.
 */
#define ARIA_P2(x) (((x) >> 16) ^ ((x) << 16))

/*
 * modify byte order: ( A B C D ) -> ( D C B A ), i.e. change endianness
 *
 * This is submatrix P3 in [1] Appendix B.1
 */
#define ARIA_P3(x) MBEDTLS_BSWAP32(x)

/*
 * ARIA Affine Transform
 * (a, b, c, d) = state in/out
 *
 * If we denote the first byte of input by 0, ..., the last byte by f,
 * then inputs are: a = 0123, b = 4567, c = 89ab, d = cdef.
 *
 * Reading [1] 2.4 or [2] 2.4.3 in columns and performing simple
 * rearrangements on adjacent pairs, output is:
 *
 * a = 3210 + 4545 + 6767 + 88aa + 99bb + dccd + effe
 *   = 3210 + 4567 + 6745 + 89ab + 98ba + dcfe + efcd
 * b = 0101 + 2323 + 5476 + 8998 + baab + eecc + ffdd
 *   = 0123 + 2301 + 5476 + 89ab + ba98 + efcd + fedc
 * c = 0022 + 1133 + 4554 + 7667 + ab89 + dcdc + fefe
 *   = 0123 + 1032 + 4567 + 7654 + ab89 + dcfe + fedc
 * d = 1001 + 2332 + 6644 + 7755 + 9898 + baba + cdef
 *   = 1032 + 2301 + 6745 + 7654 + 98ba + ba98 + cdef
 *
 * Note: another presentation of the A transform can be found as the first
 * half of App. B.1 in [1] in terms of 4-byte operators P1, P2, P3 and P4.
 * The implementation below uses only P1 and P2 as they are sufficient.
 */
static inline void aria_a(uint32_t *a, uint32_t *b,
                          uint32_t *c, uint32_t *d)
{
    uint32_t ta, tb, tc;
    ta  =  *b;                      // 4567
    *b  =  *a;                      // 0123
    *a  =  ARIA_P2(ta);             // 6745
    tb  =  ARIA_P2(*d);             // efcd
    *d  =  ARIA_P1(*c);             // 98ba
    *c  =  ARIA_P1(tb);             // fedc
    ta  ^= *d;                      // 4567+98ba
    tc  =  ARIA_P2(*b);             // 2301
    ta  =  ARIA_P1(ta) ^ tc ^ *c;   // 2301+5476+89ab+fedc
    tb  ^= ARIA_P2(*d);             // ba98+efcd
    tc  ^= ARIA_P1(*a);             // 2301+7654
    *b  ^= ta ^ tb;                 // 0123+2301+5476+89ab+ba98+efcd+fedc OUT
    tb  =  ARIA_P2(tb) ^ ta;        // 2301+5476+89ab+98ba+cdef+fedc
    *a  ^= ARIA_P1(tb);             // 3210+4567+6745+89ab+98ba+dcfe+efcd OUT
    ta  =  ARIA_P2(ta);             // 0123+7654+ab89+dcfe
    *d  ^= ARIA_P1(ta) ^ tc;        // 1032+2301+6745+7654+98ba+ba98+cdef OUT
    tc  =  ARIA_P2(tc);             // 0123+5476
    *c  ^= ARIA_P1(tc) ^ ta;        // 0123+1032+4567+7654+ab89+dcfe+fedc OUT
}

/*
 * ARIA Substitution Layer SL1 / SL2
 * (a, b, c, d) = state in/out
 * (sa, sb, sc, sd) = 256 8-bit S-Boxes (see below)
 *
 * By passing sb1, sb2, is1, is2 as S-Boxes you get SL1
 * By passing is1, is2, sb1, sb2 as S-Boxes you get SL2
 */
static inline void aria_sl(uint32_t *a, uint32_t *b,
                           uint32_t *c, uint32_t *d,
                           const uint8_t sa[256], const uint8_t sb[256],
                           const uint8_t sc[256], const uint8_t sd[256])
{
    *a = ((uint32_t) sa[MBEDTLS_BYTE_0(*a)]) ^
         (((uint32_t) sb[MBEDTLS_BYTE_1(*a)]) <<  8) ^
         (((uint32_t) sc[MBEDTLS_BYTE_2(*a)]) << 16) ^
         (((uint32_t) sd[MBEDTLS_BYTE_3(*a)]) << 24);
    *b = ((uint32_t) sa[MBEDTLS_BYTE_0(*b)]) ^
         (((uint32_t) sb[MBEDTLS_BYTE_1(*b)]) <<  8) ^
         (((uint32_t) sc[MBEDTLS_BYTE_2(*b)]) << 16) ^
         (((uint32_t) sd[MBEDTLS_BYTE_3(*b)]) << 24);
    *c = ((uint32_t) sa[MBEDTLS_BYTE_0(*c)]) ^
         (((uint32_t) sb[MBEDTLS_BYTE_1(*c)]) <<  8) ^
         (((uint32_t) sc[MBEDTLS_BYTE_2(*c)]) << 16) ^
         (((uint32_t) sd[MBEDTLS_BYTE_3(*c)]) << 24);
    *d = ((uint32_t) sa[MBEDTLS_BYTE_0(*d)]) ^
         (((uint32_t) sb[MBEDTLS_BYTE_1(*d)]) <<  8) ^
         (((uint32_t) sc[MBEDTLS_BYTE_2(*d)]) << 16) ^
         (((uint32_t) sd[MBEDTLS_BYTE_3(*d)]) << 24);
}

/*
 * S-Boxes
 */
static const uint8_t aria_sb1[256] =
{
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B,
    0xFE, 0xD7, 0xAB, 0x76, 0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0, 0xB7, 0xFD, 0x93, 0x26,
    0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2,
    0xEB, 0x27, 0xB2, 0x75, 0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84, 0x53, 0xD1, 0x00, 0xED,
    0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F,
    0x50, 0x3C, 0x9F, 0xA8, 0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2, 0xCD, 0x0C, 0x13, 0xEC,
    0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14,
    0xDE, 0x5E, 0x0B, 0xDB, 0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79, 0xE7, 0xC8, 0x37, 0x6D,
    0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F,
    0x4B, 0xBD, 0x8B, 0x8A, 0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E, 0xE1, 0xF8, 0x98, 0x11,
    0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F,
    0xB0, 0x54, 0xBB, 0x16
};

static const uint8_t aria_sb2[256] =
{
    0xE2, 0x4E, 0x54, 0xFC, 0x94, 0xC2, 0x4A, 0xCC, 0x62, 0x0D, 0x6A, 0x46,
    0x3C, 0x4D, 0x8B, 0xD1, 0x5E, 0xFA, 0x64, 0xCB, 0xB4, 0x97, 0xBE, 0x2B,
    0xBC, 0x77, 0x2E, 0x03, 0xD3, 0x19, 0x59, 0xC1, 0x1D, 0x06, 0x41, 0x6B,
    0x55, 0xF0, 0x99, 0x69, 0xEA, 0x9C, 0x18, 0xAE, 0x63, 0xDF, 0xE7, 0xBB,
    0x00, 0x73, 0x66, 0xFB, 0x96, 0x4C, 0x85, 0xE4, 0x3A, 0x09, 0x45, 0xAA,
    0x0F, 0xEE, 0x10, 0xEB, 0x2D, 0x7F, 0xF4, 0x29, 0xAC, 0xCF, 0xAD, 0x91,
    0x8D, 0x78, 0xC8, 0x95, 0xF9, 0x2F, 0xCE, 0xCD, 0x08, 0x7A, 0x88, 0x38,
    0x5C, 0x83, 0x2A, 0x28, 0x47, 0xDB, 0xB8, 0xC7, 0x93, 0xA4, 0x12, 0x53,
    0xFF, 0x87, 0x0E, 0x31, 0x36, 0x21, 0x58, 0x48, 0x01, 0x8E, 0x37, 0x74,
    0x32, 0xCA, 0xE9, 0xB1, 0xB7, 0xAB, 0x0C, 0xD7, 0xC4, 0x56, 0x42, 0x26,
    0x07, 0x98, 0x60, 0xD9, 0xB6, 0xB9, 0x11, 0x40, 0xEC, 0x20, 0x8C, 0xBD,
    0xA0, 0xC9, 0x84, 0x04, 0x49, 0x23, 0xF1, 0x4F, 0x50, 0x1F, 0x13, 0xDC,
    0xD8, 0xC0, 0x9E, 0x57, 0xE3, 0xC3, 0x7B, 0x65, 0x3B, 0x02, 0x8F, 0x3E,
    0xE8, 0x25, 0x92, 0xE5, 0x15, 0xDD, 0xFD, 0x17, 0xA9, 0xBF, 0xD4, 0x9A,
    0x7E, 0xC5, 0x39, 0x67, 0xFE, 0x76, 0x9D, 0x43, 0xA7, 0xE1, 0xD0, 0xF5,
    0x68, 0xF2, 0x1B, 0x34, 0x70, 0x05, 0xA3, 0x8A, 0xD5, 0x79, 0x86, 0xA8,
    0x30, 0xC6, 0x51, 0x4B, 0x1E, 0xA6, 0x27, 0xF6, 0x35, 0xD2, 0x6E, 0x24,
    0x16, 0x82, 0x5F, 0xDA, 0xE6, 0x75, 0xA2, 0xEF, 0x2C, 0xB2, 0x1C, 0x9F,
    0x5D, 0x6F, 0x80, 0x0A, 0x72, 0x44, 0x9B, 0x6C, 0x90, 0x0B, 0x5B, 0x33,
    0x7D, 0x5A, 0x52, 0xF3, 0x61, 0xA1, 0xF7, 0xB0, 0xD6, 0x3F, 0x7C, 0x6D,
    0xED, 0x14, 0xE0, 0xA5, 0x3D, 0x22, 0xB3, 0xF8, 0x89, 0xDE, 0x71, 0x1A,
    0xAF, 0xBA, 0xB5, 0x81
};

static const uint8_t aria_is1[256] =
{
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E,
    0x81, 0xF3, 0xD7, 0xFB, 0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB, 0x54, 0x7B, 0x94, 0x32,
    0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49,
    0x6D, 0x8B, 0xD1, 0x25, 0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92, 0x6C, 0x70, 0x48, 0x50,
    0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05,
    0xB8, 0xB3, 0x45, 0x06, 0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B, 0x3A, 0x91, 0x11, 0x41,
    0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8,
    0x1C, 0x75, 0xDF, 0x6E, 0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B, 0xFC, 0x56, 0x3E, 0x4B,
    0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59,
    0x27, 0x80, 0xEC, 0x5F, 0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF, 0xA0, 0xE0, 0x3B, 0x4D,
    0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0C, 0x7D
};

static const uint8_t aria_is2[256] =
{
    0x30, 0x68, 0x99, 0x1B, 0x87, 0xB9, 0x21, 0x78, 0x50, 0x39, 0xDB, 0xE1,
    0x72, 0x09, 0x62, 0x3C, 0x3E, 0x7E, 0x5E, 0x8E, 0xF1, 0xA0, 0xCC, 0xA3,
    0x2A, 0x1D, 0xFB, 0xB6, 0xD6, 0x20, 0xC4, 0x8D, 0x81, 0x65, 0xF5, 0x89,
    0xCB, 0x9D, 0x77, 0xC6, 0x57, 0x43, 0x56, 0x17, 0xD4, 0x40, 0x1A, 0x4D,
    0xC0, 0x63, 0x6C, 0xE3, 0xB7, 0xC8, 0x64, 0x6A, 0x53, 0xAA, 0x38, 0x98,
    0x0C, 0xF4, 0x9B, 0xED, 0x7F, 0x22, 0x76, 0xAF, 0xDD, 0x3A, 0x0B, 0x58,
    0x67, 0x88, 0x06, 0xC3, 0x35, 0x0D, 0x01, 0x8B, 0x8C, 0xC2, 0xE6, 0x5F,
    0x02, 0x24, 0x75, 0x93, 0x66, 0x1E, 0xE5, 0xE2, 0x54, 0xD8, 0x10, 0xCE,
    0x7A, 0xE8, 0x08, 0x2C, 0x12, 0x97, 0x32, 0xAB, 0xB4, 0x27, 0x0A, 0x23,
    0xDF, 0xEF, 0xCA, 0xD9, 0xB8, 0xFA, 0xDC, 0x31, 0x6B, 0xD1, 0xAD, 0x19,
    0x49, 0xBD, 0x51, 0x96, 0xEE, 0xE4, 0xA8, 0x41, 0xDA, 0xFF, 0xCD, 0x55,
    0x86, 0x36, 0xBE, 0x61, 0x52, 0xF8, 0xBB, 0x0E, 0x82, 0x48, 0x69, 0x9A,
    0xE0, 0x47, 0x9E, 0x5C, 0x04, 0x4B, 0x34, 0x15, 0x79, 0x26, 0xA7, 0xDE,
    0x29, 0xAE, 0x92, 0xD7, 0x84, 0xE9, 0xD2, 0xBA, 0x5D, 0xF3, 0xC5, 0xB0,
    0xBF, 0xA4, 0x3B, 0x71, 0x44, 0x46, 0x2B, 0xFC, 0xEB, 0x6F, 0xD5, 0xF6,
    0x14, 0xFE, 0x7C, 0x70, 0x5A, 0x7D, 0xFD, 0x2F, 0x18, 0x83, 0x16, 0xA5,
    0x91, 0x1F, 0x05, 0x95, 0x74, 0xA9, 0xC1, 0x5B, 0x4A, 0x85, 0x6D, 0x13,
    0x07, 0x4F, 0x4E, 0x45, 0xB2, 0x0F, 0xC9, 0x1C, 0xA6, 0xBC, 0xEC, 0x73,
    0x90, 0x7B, 0xCF, 0x59, 0x8F, 0xA1, 0xF9, 0x2D, 0xF2, 0xB1, 0x00, 0x94,
    0x37, 0x9F, 0xD0, 0x2E, 0x9C, 0x6E, 0x28, 0x3F, 0x80, 0xF0, 0x3D, 0xD3,
    0x25, 0x8A, 0xB5, 0xE7, 0x42, 0xB3, 0xC7, 0xEA, 0xF7, 0x4C, 0x11, 0x33,
    0x03, 0xA2, 0xAC, 0x60
};

/*
 * Helper for key schedule: r = FO( p, k ) ^ x
 */
static void aria_fo_xor(uint32_t r[4], const uint32_t p[4],
                        const uint32_t k[4], const uint32_t x[4])
{
    uint32_t a, b, c, d;

    a = p[0] ^ k[0];
    b = p[1] ^ k[1];
    c = p[2] ^ k[2];
    d = p[3] ^ k[3];

    aria_sl(&a, &b, &c, &d, aria_sb1, aria_sb2, aria_is1, aria_is2);
    aria_a(&a, &b, &c, &d);

    r[0] = a ^ x[0];
    r[1] = b ^ x[1];
    r[2] = c ^ x[2];
    r[3] = d ^ x[3];
}

/*
 * Helper for key schedule: r = FE( p, k ) ^ x
 */
static void aria_fe_xor(uint32_t r[4], const uint32_t p[4],
                        const uint32_t k[4], const uint32_t x[4])
{
    uint32_t a, b, c, d;

    a = p[0] ^ k[0];
    b = p[1] ^ k[1];
    c = p[2] ^ k[2];
    d = p[3] ^ k[3];

    aria_sl(&a, &b, &c, &d, aria_is1, aria_is2, aria_sb1, aria_sb2);
    aria_a(&a, &b, &c, &d);

    r[0] = a ^ x[0];
    r[1] = b ^ x[1];
    r[2] = c ^ x[2];
    r[3] = d ^ x[3];
}

/*
 * Big endian 128-bit rotation: r = a ^ (b <<< n), used only in key setup.
 *
 * We chose to store bytes into 32-bit words in little-endian format (see
 * MBEDTLS_GET_UINT32_LE / MBEDTLS_PUT_UINT32_LE ) so we need to reverse
 * bytes here.
 */
static void aria_rot128(uint32_t r[4], const uint32_t a[4],
                        const uint32_t b[4], uint8_t n)
{
    uint8_t i, j;
    uint32_t t, u;

    const uint8_t n1 = n % 32;              // bit offset
    const uint8_t n2 = n1 ? 32 - n1 : 0;    // reverse bit offset

    j = (n / 32) % 4;                       // initial word offset
    t = ARIA_P3(b[j]);                      // big endian
    for (i = 0; i < 4; i++) {
        j = (j + 1) % 4;                    // get next word, big endian
        u = ARIA_P3(b[j]);
        t <<= n1;                           // rotate
        t |= u >> n2;
        t = ARIA_P3(t);                     // back to little endian
        r[i] = a[i] ^ t;                    // store
        t = u;                              // move to next word
    }
}

/*
 * Set encryption key
 */
int mbedtls_aria_setkey_enc(mbedtls_aria_context *ctx,
                            const unsigned char *key, unsigned int keybits)
{
    /* round constant masks */
    const uint32_t rc[3][4] =
    {
        {   0xB7C17C51, 0x940A2227, 0xE8AB13FE, 0xE06E9AFA  },
        {   0xCC4AB16D, 0x20C8219E, 0xD5B128FF, 0xB0E25DEF  },
        {   0x1D3792DB, 0x70E92621, 0x75972403, 0x0EC9E804  }
    };

    int i;
    uint32_t w[4][4], *w2;

    if (keybits != 128 && keybits != 192 && keybits != 256) {
        return MBEDTLS_ERR_ARIA_BAD_INPUT_DATA;
    }

    /* Copy key to W0 (and potential remainder to W1) */
    w[0][0] = MBEDTLS_GET_UINT32_LE(key,  0);
    w[0][1] = MBEDTLS_GET_UINT32_LE(key,  4);
    w[0][2] = MBEDTLS_GET_UINT32_LE(key,  8);
    w[0][3] = MBEDTLS_GET_UINT32_LE(key, 12);

    memset(w[1], 0, 16);
    if (keybits >= 192) {
        w[1][0] = MBEDTLS_GET_UINT32_LE(key, 16);    // 192 bit key
        w[1][1] = MBEDTLS_GET_UINT32_LE(key, 20);
    }
    if (keybits == 256) {
        w[1][2] = MBEDTLS_GET_UINT32_LE(key, 24);    // 256 bit key
        w[1][3] = MBEDTLS_GET_UINT32_LE(key, 28);
    }

    i = (keybits - 128) >> 6;               // index: 0, 1, 2
    ctx->nr = 12 + 2 * i;                   // no. rounds: 12, 14, 16

    aria_fo_xor(w[1], w[0], rc[i], w[1]);   // W1 = FO(W0, CK1) ^ KR
    i = i < 2 ? i + 1 : 0;
    aria_fe_xor(w[2], w[1], rc[i], w[0]);   // W2 = FE(W1, CK2) ^ W0
    i = i < 2 ? i + 1 : 0;
    aria_fo_xor(w[3], w[2], rc[i], w[1]);   // W3 = FO(W2, CK3) ^ W1

    for (i = 0; i < 4; i++) {               // create round keys
        w2 = w[(i + 1) & 3];
        aria_rot128(ctx->rk[i], w[i], w2, 128 - 19);
        aria_rot128(ctx->rk[i +  4], w[i], w2, 128 - 31);
        aria_rot128(ctx->rk[i +  8], w[i], w2,       61);
        aria_rot128(ctx->rk[i + 12], w[i], w2,       31);
    }
    aria_rot128(ctx->rk[16], w[0], w[1], 19);

    /* w holds enough info to reconstruct the round keys */
    mbedtls_platform_zeroize(w, sizeof(w));

    return 0;
}

/*
 * Set decryption key
 */
#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
int mbedtls_aria_setkey_dec(mbedtls_aria_context *ctx,
                            const unsigned char *key, unsigned int keybits)
{
    int i, j, k, ret;

    ret = mbedtls_aria_setkey_enc(ctx, key, keybits);
    if (ret != 0) {
        return ret;
    }

    /* flip the order of round keys */
    for (i = 0, j = ctx->nr; i < j; i++, j--) {
        for (k = 0; k < 4; k++) {
            uint32_t t = ctx->rk[i][k];
            ctx->rk[i][k] = ctx->rk[j][k];
            ctx->rk[j][k] = t;
        }
    }

    /* apply affine transform to middle keys */
    for (i = 1; i < ctx->nr; i++) {
        aria_a(&ctx->rk[i][0], &ctx->rk[i][1],
               &ctx->rk[i][2], &ctx->rk[i][3]);
    }

    return 0;
}
#endif /* !MBEDTLS_BLOCK_CIPHER_NO_DECRYPT */

/*
 * Encrypt a block
 */
int mbedtls_aria_crypt_ecb(mbedtls_aria_context *ctx,
                           const unsigned char input[MBEDTLS_ARIA_BLOCKSIZE],
                           unsigned char output[MBEDTLS_ARIA_BLOCKSIZE])
{
    int i;

    uint32_t a, b, c, d;

    a = MBEDTLS_GET_UINT32_LE(input,  0);
    b = MBEDTLS_GET_UINT32_LE(input,  4);
    c = MBEDTLS_GET_UINT32_LE(input,  8);
    d = MBEDTLS_GET_UINT32_LE(input, 12);

    i = 0;
    while (1) {
        a ^= ctx->rk[i][0];
        b ^= ctx->rk[i][1];
        c ^= ctx->rk[i][2];
        d ^= ctx->rk[i][3];
        i++;

        aria_sl(&a, &b, &c, &d, aria_sb1, aria_sb2, aria_is1, aria_is2);
        aria_a(&a, &b, &c, &d);

        a ^= ctx->rk[i][0];
        b ^= ctx->rk[i][1];
        c ^= ctx->rk[i][2];
        d ^= ctx->rk[i][3];
        i++;

        aria_sl(&a, &b, &c, &d, aria_is1, aria_is2, aria_sb1, aria_sb2);
        if (i >= ctx->nr) {
            break;
        }
        aria_a(&a, &b, &c, &d);
    }

    /* final key mixing */
    a ^= ctx->rk[i][0];
    b ^= ctx->rk[i][1];
    c ^= ctx->rk[i][2];
    d ^= ctx->rk[i][3];

    MBEDTLS_PUT_UINT32_LE(a, output,  0);
    MBEDTLS_PUT_UINT32_LE(b, output,  4);
    MBEDTLS_PUT_UINT32_LE(c, output,  8);
    MBEDTLS_PUT_UINT32_LE(d, output, 12);

    return 0;
}

/* Initialize context */
void mbedtls_aria_init(mbedtls_aria_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_aria_context));
}

/* Clear context */
void mbedtls_aria_free(mbedtls_aria_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_aria_context));
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * ARIA-CBC buffer encryption/decryption
 */
int mbedtls_aria_crypt_cbc(mbedtls_aria_context *ctx,
                           int mode,
                           size_t length,
                           unsigned char iv[MBEDTLS_ARIA_BLOCKSIZE],
                           const unsigned char *input,
                           unsigned char *output)
{
    unsigned char temp[MBEDTLS_ARIA_BLOCKSIZE];

    if ((mode != MBEDTLS_ARIA_ENCRYPT) && (mode != MBEDTLS_ARIA_DECRYPT)) {
        return MBEDTLS_ERR_ARIA_BAD_INPUT_DATA;
    }

    if (length % MBEDTLS_ARIA_BLOCKSIZE) {
        return MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH;
    }

    if (mode == MBEDTLS_ARIA_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, MBEDTLS_ARIA_BLOCKSIZE);
            mbedtls_aria_crypt_ecb(ctx, input, output);

            mbedtls_xor(output, output, iv, MBEDTLS_ARIA_BLOCKSIZE);

            memcpy(iv, temp, MBEDTLS_ARIA_BLOCKSIZE);

            input  += MBEDTLS_ARIA_BLOCKSIZE;
            output += MBEDTLS_ARIA_BLOCKSIZE;
            length -= MBEDTLS_ARIA_BLOCKSIZE;
        }
    } else {
        while (length > 0) {
            mbedtls_xor(output, input, iv, MBEDTLS_ARIA_BLOCKSIZE);

            mbedtls_aria_crypt_ecb(ctx, output, output);
            memcpy(iv, output, MBEDTLS_ARIA_BLOCKSIZE);

            input  += MBEDTLS_ARIA_BLOCKSIZE;
            output += MBEDTLS_ARIA_BLOCKSIZE;
            length -= MBEDTLS_ARIA_BLOCKSIZE;
        }
    }

    return 0;
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
/*
 * ARIA-CFB128 buffer encryption/decryption
 */
int mbedtls_aria_crypt_cfb128(mbedtls_aria_context *ctx,
                              int mode,
                              size_t length,
                              size_t *iv_off,
                              unsigned char iv[MBEDTLS_ARIA_BLOCKSIZE],
                              const unsigned char *input,
                              unsigned char *output)
{
    unsigned char c;
    size_t n;

    if ((mode != MBEDTLS_ARIA_ENCRYPT) && (mode != MBEDTLS_ARIA_DECRYPT)) {
        return MBEDTLS_ERR_ARIA_BAD_INPUT_DATA;
    }

    n = *iv_off;

    /* An overly large value of n can lead to an unlimited
     * buffer overflow. */
    if (n >= MBEDTLS_ARIA_BLOCKSIZE) {
        return MBEDTLS_ERR_ARIA_BAD_INPUT_DATA;
    }

    if (mode == MBEDTLS_ARIA_DECRYPT) {
        while (length--) {
            if (n == 0) {
                mbedtls_aria_crypt_ecb(ctx, iv, iv);
            }

            c = *input++;
            *output++ = c ^ iv[n];
            iv[n] = c;

            n = (n + 1) & 0x0F;
        }
    } else {
        while (length--) {
            if (n == 0) {
                mbedtls_aria_crypt_ecb(ctx, iv, iv);
            }

            iv[n] = *output++ = (unsigned char) (iv[n] ^ *input++);

            n = (n + 1) & 0x0F;
        }
    }

    *iv_off = n;

    return 0;
}
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
/*
 * ARIA-CTR buffer encryption/decryption
 */
int mbedtls_aria_crypt_ctr(mbedtls_aria_context *ctx,
                           size_t length,
                           size_t *nc_off,
                           unsigned char nonce_counter[MBEDTLS_ARIA_BLOCKSIZE],
                           unsigned char stream_block[MBEDTLS_ARIA_BLOCKSIZE],
                           const unsigned char *input,
                           unsigned char *output)
{
    int c, i;
    size_t n;

    n = *nc_off;
    /* An overly large value of n can lead to an unlimited
     * buffer overflow. */
    if (n >= MBEDTLS_ARIA_BLOCKSIZE) {
        return MBEDTLS_ERR_ARIA_BAD_INPUT_DATA;
    }

    while (length--) {
        if (n == 0) {
            mbedtls_aria_crypt_ecb(ctx, nonce_counter,
                                   stream_block);

            for (i = MBEDTLS_ARIA_BLOCKSIZE; i > 0; i--) {
                if (++nonce_counter[i - 1] != 0) {
                    break;
                }
            }
        }
        c = *input++;
        *output++ = (unsigned char) (c ^ stream_block[n]);

        n = (n + 1) & 0x0F;
    }

    *nc_off = n;

    return 0;
}
#endif /* MBEDTLS_CIPHER_MODE_CTR */
#endif /* !MBEDTLS_ARIA_ALT */

#if defined(MBEDTLS_SELF_TEST)

/*
 * Basic ARIA ECB test vectors from RFC 5794
 */
static const uint8_t aria_test1_ecb_key[32] =           // test key
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,     // 128 bit
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,     // 192 bit
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F      // 256 bit
};

static const uint8_t aria_test1_ecb_pt[MBEDTLS_ARIA_BLOCKSIZE] =            // plaintext
{
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,     // same for all
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF      // key sizes
};

static const uint8_t aria_test1_ecb_ct[3][MBEDTLS_ARIA_BLOCKSIZE] =         // ciphertext
{
    { 0xD7, 0x18, 0xFB, 0xD6, 0xAB, 0x64, 0x4C, 0x73,   // 128 bit
      0x9D, 0xA9, 0x5F, 0x3B, 0xE6, 0x45, 0x17, 0x78 },
    { 0x26, 0x44, 0x9C, 0x18, 0x05, 0xDB, 0xE7, 0xAA,   // 192 bit
      0x25, 0xA4, 0x68, 0xCE, 0x26, 0x3A, 0x9E, 0x79 },
    { 0xF9, 0x2B, 0xD7, 0xC7, 0x9F, 0xB7, 0x2E, 0x2F,   // 256 bit
      0x2B, 0x8F, 0x80, 0xC1, 0x97, 0x2D, 0x24, 0xFC }
};

/*
 * Mode tests from "Test Vectors for ARIA"  Version 1.0
 * http://210.104.33.10/ARIA/doc/ARIA-testvector-e.pdf
 */
#if (defined(MBEDTLS_CIPHER_MODE_CBC) || defined(MBEDTLS_CIPHER_MODE_CFB) || \
    defined(MBEDTLS_CIPHER_MODE_CTR))
static const uint8_t aria_test2_key[32] =
{
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,     // 128 bit
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,     // 192 bit
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff      // 256 bit
};

static const uint8_t aria_test2_pt[48] =
{
    0x11, 0x11, 0x11, 0x11, 0xaa, 0xaa, 0xaa, 0xaa,     // same for all
    0x11, 0x11, 0x11, 0x11, 0xbb, 0xbb, 0xbb, 0xbb,
    0x11, 0x11, 0x11, 0x11, 0xcc, 0xcc, 0xcc, 0xcc,
    0x11, 0x11, 0x11, 0x11, 0xdd, 0xdd, 0xdd, 0xdd,
    0x22, 0x22, 0x22, 0x22, 0xaa, 0xaa, 0xaa, 0xaa,
    0x22, 0x22, 0x22, 0x22, 0xbb, 0xbb, 0xbb, 0xbb,
};
#endif

#if (defined(MBEDTLS_CIPHER_MODE_CBC) || defined(MBEDTLS_CIPHER_MODE_CFB))
static const uint8_t aria_test2_iv[MBEDTLS_ARIA_BLOCKSIZE] =
{
    0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78,     // same for CBC, CFB
    0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf0      // CTR has zero IV
};
#endif

#if defined(MBEDTLS_CIPHER_MODE_CBC)
static const uint8_t aria_test2_cbc_ct[3][48] =         // CBC ciphertext
{
    { 0x49, 0xd6, 0x18, 0x60, 0xb1, 0x49, 0x09, 0x10,   // 128-bit key
      0x9c, 0xef, 0x0d, 0x22, 0xa9, 0x26, 0x81, 0x34,
      0xfa, 0xdf, 0x9f, 0xb2, 0x31, 0x51, 0xe9, 0x64,
      0x5f, 0xba, 0x75, 0x01, 0x8b, 0xdb, 0x15, 0x38,
      0xb5, 0x33, 0x34, 0x63, 0x4b, 0xbf, 0x7d, 0x4c,
      0xd4, 0xb5, 0x37, 0x70, 0x33, 0x06, 0x0c, 0x15 },
    { 0xaf, 0xe6, 0xcf, 0x23, 0x97, 0x4b, 0x53, 0x3c,   // 192-bit key
      0x67, 0x2a, 0x82, 0x62, 0x64, 0xea, 0x78, 0x5f,
      0x4e, 0x4f, 0x7f, 0x78, 0x0d, 0xc7, 0xf3, 0xf1,
      0xe0, 0x96, 0x2b, 0x80, 0x90, 0x23, 0x86, 0xd5,
      0x14, 0xe9, 0xc3, 0xe7, 0x72, 0x59, 0xde, 0x92,
      0xdd, 0x11, 0x02, 0xff, 0xab, 0x08, 0x6c, 0x1e },
    { 0x52, 0x3a, 0x8a, 0x80, 0x6a, 0xe6, 0x21, 0xf1,   // 256-bit key
      0x55, 0xfd, 0xd2, 0x8d, 0xbc, 0x34, 0xe1, 0xab,
      0x7b, 0x9b, 0x42, 0x43, 0x2a, 0xd8, 0xb2, 0xef,
      0xb9, 0x6e, 0x23, 0xb1, 0x3f, 0x0a, 0x6e, 0x52,
      0xf3, 0x61, 0x85, 0xd5, 0x0a, 0xd0, 0x02, 0xc5,
      0xf6, 0x01, 0xbe, 0xe5, 0x49, 0x3f, 0x11, 0x8b }
};
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
static const uint8_t aria_test2_cfb_ct[3][48] =         // CFB ciphertext
{
    { 0x37, 0x20, 0xe5, 0x3b, 0xa7, 0xd6, 0x15, 0x38,   // 128-bit key
      0x34, 0x06, 0xb0, 0x9f, 0x0a, 0x05, 0xa2, 0x00,
      0xc0, 0x7c, 0x21, 0xe6, 0x37, 0x0f, 0x41, 0x3a,
      0x5d, 0x13, 0x25, 0x00, 0xa6, 0x82, 0x85, 0x01,
      0x7c, 0x61, 0xb4, 0x34, 0xc7, 0xb7, 0xca, 0x96,
      0x85, 0xa5, 0x10, 0x71, 0x86, 0x1e, 0x4d, 0x4b },
    { 0x41, 0x71, 0xf7, 0x19, 0x2b, 0xf4, 0x49, 0x54,   // 192-bit key
      0x94, 0xd2, 0x73, 0x61, 0x29, 0x64, 0x0f, 0x5c,
      0x4d, 0x87, 0xa9, 0xa2, 0x13, 0x66, 0x4c, 0x94,
      0x48, 0x47, 0x7c, 0x6e, 0xcc, 0x20, 0x13, 0x59,
      0x8d, 0x97, 0x66, 0x95, 0x2d, 0xd8, 0xc3, 0x86,
      0x8f, 0x17, 0xe3, 0x6e, 0xf6, 0x6f, 0xd8, 0x4b },
    { 0x26, 0x83, 0x47, 0x05, 0xb0, 0xf2, 0xc0, 0xe2,   // 256-bit key
      0x58, 0x8d, 0x4a, 0x7f, 0x09, 0x00, 0x96, 0x35,
      0xf2, 0x8b, 0xb9, 0x3d, 0x8c, 0x31, 0xf8, 0x70,
      0xec, 0x1e, 0x0b, 0xdb, 0x08, 0x2b, 0x66, 0xfa,
      0x40, 0x2d, 0xd9, 0xc2, 0x02, 0xbe, 0x30, 0x0c,
      0x45, 0x17, 0xd1, 0x96, 0xb1, 0x4d, 0x4c, 0xe1 }
};
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
static const uint8_t aria_test2_ctr_ct[3][48] =         // CTR ciphertext
{
    { 0xac, 0x5d, 0x7d, 0xe8, 0x05, 0xa0, 0xbf, 0x1c,   // 128-bit key
      0x57, 0xc8, 0x54, 0x50, 0x1a, 0xf6, 0x0f, 0xa1,
      0x14, 0x97, 0xe2, 0xa3, 0x45, 0x19, 0xde, 0xa1,
      0x56, 0x9e, 0x91, 0xe5, 0xb5, 0xcc, 0xae, 0x2f,
      0xf3, 0xbf, 0xa1, 0xbf, 0x97, 0x5f, 0x45, 0x71,
      0xf4, 0x8b, 0xe1, 0x91, 0x61, 0x35, 0x46, 0xc3 },
    { 0x08, 0x62, 0x5c, 0xa8, 0xfe, 0x56, 0x9c, 0x19,   // 192-bit key
      0xba, 0x7a, 0xf3, 0x76, 0x0a, 0x6e, 0xd1, 0xce,
      0xf4, 0xd1, 0x99, 0x26, 0x3e, 0x99, 0x9d, 0xde,
      0x14, 0x08, 0x2d, 0xbb, 0xa7, 0x56, 0x0b, 0x79,
      0xa4, 0xc6, 0xb4, 0x56, 0xb8, 0x70, 0x7d, 0xce,
      0x75, 0x1f, 0x98, 0x54, 0xf1, 0x88, 0x93, 0xdf },
    { 0x30, 0x02, 0x6c, 0x32, 0x96, 0x66, 0x14, 0x17,   // 256-bit key
      0x21, 0x17, 0x8b, 0x99, 0xc0, 0xa1, 0xf1, 0xb2,
      0xf0, 0x69, 0x40, 0x25, 0x3f, 0x7b, 0x30, 0x89,
      0xe2, 0xa3, 0x0e, 0xa8, 0x6a, 0xa3, 0xc8, 0x8f,
      0x59, 0x40, 0xf0, 0x5a, 0xd7, 0xee, 0x41, 0xd7,
      0x13, 0x47, 0xbb, 0x72, 0x61, 0xe3, 0x48, 0xf1 }
};
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#define ARIA_SELF_TEST_ASSERT(cond)                   \
    do {                                            \
        if (cond) {                                \
            if (verbose)                           \
            mbedtls_printf("failed\n");       \
            goto exit;                              \
        } else {                                    \
            if (verbose)                           \
            mbedtls_printf("passed\n");       \
        }                                           \
    } while (0)

/*
 * Checkup routine
 */
int mbedtls_aria_self_test(int verbose)
{
    int i;
    uint8_t blk[MBEDTLS_ARIA_BLOCKSIZE];
    mbedtls_aria_context ctx;
    int ret = 1;

#if (defined(MBEDTLS_CIPHER_MODE_CFB) || defined(MBEDTLS_CIPHER_MODE_CTR))
    size_t j;
#endif

#if (defined(MBEDTLS_CIPHER_MODE_CBC) || \
    defined(MBEDTLS_CIPHER_MODE_CFB) || \
    defined(MBEDTLS_CIPHER_MODE_CTR))
    uint8_t buf[48], iv[MBEDTLS_ARIA_BLOCKSIZE];
#endif

    mbedtls_aria_init(&ctx);

    /*
     * Test set 1
     */
    for (i = 0; i < 3; i++) {
        /* test ECB encryption */
        if (verbose) {
            mbedtls_printf("  ARIA-ECB-%d (enc): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test1_ecb_key, 128 + 64 * i);
        mbedtls_aria_crypt_ecb(&ctx, aria_test1_ecb_pt, blk);
        ARIA_SELF_TEST_ASSERT(
            memcmp(blk, aria_test1_ecb_ct[i], MBEDTLS_ARIA_BLOCKSIZE)
            != 0);

        /* test ECB decryption */
        if (verbose) {
            mbedtls_printf("  ARIA-ECB-%d (dec): ", 128 + 64 * i);
#if defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
            mbedtls_printf("skipped\n");
#endif
        }

#if !defined(MBEDTLS_BLOCK_CIPHER_NO_DECRYPT)
        mbedtls_aria_setkey_dec(&ctx, aria_test1_ecb_key, 128 + 64 * i);
        mbedtls_aria_crypt_ecb(&ctx, aria_test1_ecb_ct[i], blk);
        ARIA_SELF_TEST_ASSERT(
            memcmp(blk, aria_test1_ecb_pt, MBEDTLS_ARIA_BLOCKSIZE)
            != 0);
#endif
    }
    if (verbose) {
        mbedtls_printf("\n");
    }

    /*
     * Test set 2
     */
#if defined(MBEDTLS_CIPHER_MODE_CBC)
    for (i = 0; i < 3; i++) {
        /* Test CBC encryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CBC-%d (enc): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test2_key, 128 + 64 * i);
        memcpy(iv, aria_test2_iv, MBEDTLS_ARIA_BLOCKSIZE);
        memset(buf, 0x55, sizeof(buf));
        mbedtls_aria_crypt_cbc(&ctx, MBEDTLS_ARIA_ENCRYPT, 48, iv,
                               aria_test2_pt, buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_cbc_ct[i], 48)
                              != 0);

        /* Test CBC decryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CBC-%d (dec): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_dec(&ctx, aria_test2_key, 128 + 64 * i);
        memcpy(iv, aria_test2_iv, MBEDTLS_ARIA_BLOCKSIZE);
        memset(buf, 0xAA, sizeof(buf));
        mbedtls_aria_crypt_cbc(&ctx, MBEDTLS_ARIA_DECRYPT, 48, iv,
                               aria_test2_cbc_ct[i], buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_pt, 48) != 0);
    }
    if (verbose) {
        mbedtls_printf("\n");
    }

#endif /* MBEDTLS_CIPHER_MODE_CBC */

#if defined(MBEDTLS_CIPHER_MODE_CFB)
    for (i = 0; i < 3; i++) {
        /* Test CFB encryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CFB-%d (enc): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test2_key, 128 + 64 * i);
        memcpy(iv, aria_test2_iv, MBEDTLS_ARIA_BLOCKSIZE);
        memset(buf, 0x55, sizeof(buf));
        j = 0;
        mbedtls_aria_crypt_cfb128(&ctx, MBEDTLS_ARIA_ENCRYPT, 48, &j, iv,
                                  aria_test2_pt, buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_cfb_ct[i], 48) != 0);

        /* Test CFB decryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CFB-%d (dec): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test2_key, 128 + 64 * i);
        memcpy(iv, aria_test2_iv, MBEDTLS_ARIA_BLOCKSIZE);
        memset(buf, 0xAA, sizeof(buf));
        j = 0;
        mbedtls_aria_crypt_cfb128(&ctx, MBEDTLS_ARIA_DECRYPT, 48, &j,
                                  iv, aria_test2_cfb_ct[i], buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_pt, 48) != 0);
    }
    if (verbose) {
        mbedtls_printf("\n");
    }
#endif /* MBEDTLS_CIPHER_MODE_CFB */

#if defined(MBEDTLS_CIPHER_MODE_CTR)
    for (i = 0; i < 3; i++) {
        /* Test CTR encryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CTR-%d (enc): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test2_key, 128 + 64 * i);
        memset(iv, 0, MBEDTLS_ARIA_BLOCKSIZE);                      // IV = 0
        memset(buf, 0x55, sizeof(buf));
        j = 0;
        mbedtls_aria_crypt_ctr(&ctx, 48, &j, iv, blk,
                               aria_test2_pt, buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_ctr_ct[i], 48) != 0);

        /* Test CTR decryption */
        if (verbose) {
            mbedtls_printf("  ARIA-CTR-%d (dec): ", 128 + 64 * i);
        }
        mbedtls_aria_setkey_enc(&ctx, aria_test2_key, 128 + 64 * i);
        memset(iv, 0, MBEDTLS_ARIA_BLOCKSIZE);                      // IV = 0
        memset(buf, 0xAA, sizeof(buf));
        j = 0;
        mbedtls_aria_crypt_ctr(&ctx, 48, &j, iv, blk,
                               aria_test2_ctr_ct[i], buf);
        ARIA_SELF_TEST_ASSERT(memcmp(buf, aria_test2_pt, 48) != 0);
    }
    if (verbose) {
        mbedtls_printf("\n");
    }
#endif /* MBEDTLS_CIPHER_MODE_CTR */

    ret = 0;

exit:
    mbedtls_aria_free(&ctx);
    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_ARIA_C */
