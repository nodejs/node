/**
 * \file alignment.h
 *
 * \brief Utility code for dealing with unaligned memory accesses
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_LIBRARY_ALIGNMENT_H
#define MBEDTLS_LIBRARY_ALIGNMENT_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*
 * Define MBEDTLS_EFFICIENT_UNALIGNED_ACCESS for architectures where unaligned memory
 * accesses are known to be efficient.
 *
 * All functions defined here will behave correctly regardless, but might be less
 * efficient when this is not defined.
 */
#if defined(__ARM_FEATURE_UNALIGNED) \
    || defined(MBEDTLS_ARCH_IS_X86) || defined(MBEDTLS_ARCH_IS_X64) \
    || defined(MBEDTLS_PLATFORM_IS_WINDOWS_ON_ARM64)
/*
 * __ARM_FEATURE_UNALIGNED is defined where appropriate by armcc, gcc 7, clang 9
 * (and later versions) for Arm v7 and later; all x86 platforms should have
 * efficient unaligned access.
 *
 * https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170#alignment
 * specifies that on Windows-on-Arm64, unaligned access is safe (except for uncached
 * device memory).
 */
#define MBEDTLS_EFFICIENT_UNALIGNED_ACCESS
#endif

#if defined(__IAR_SYSTEMS_ICC__) && \
    (defined(MBEDTLS_ARCH_IS_ARM64) || defined(MBEDTLS_ARCH_IS_ARM32) \
    || defined(__ICCRX__) || defined(__ICCRL78__) || defined(__ICCRISCV__))
#pragma language=save
#pragma language=extended
#define MBEDTLS_POP_IAR_LANGUAGE_PRAGMA
/* IAR recommend this technique for accessing unaligned data in
 * https://www.iar.com/knowledge/support/technical-notes/compiler/accessing-unaligned-data
 * This results in a single load / store instruction (if unaligned access is supported).
 * According to that document, this is only supported on certain architectures.
 */
    #define UINT_UNALIGNED
typedef uint16_t __packed mbedtls_uint16_unaligned_t;
typedef uint32_t __packed mbedtls_uint32_unaligned_t;
typedef uint64_t __packed mbedtls_uint64_unaligned_t;
#elif defined(MBEDTLS_COMPILER_IS_GCC) && (MBEDTLS_GCC_VERSION >= 40504) && \
    ((MBEDTLS_GCC_VERSION < 60300) || (!defined(MBEDTLS_EFFICIENT_UNALIGNED_ACCESS)))
/*
 * gcc may generate a branch to memcpy for calls like `memcpy(dest, src, 4)` rather than
 * generating some LDR or LDRB instructions (similar for stores).
 *
 * This is architecture dependent: x86-64 seems fine even with old gcc; 32-bit Arm
 * is affected. To keep it simple, we enable for all architectures.
 *
 * For versions of gcc < 5.4.0 this issue always happens.
 * For gcc < 6.3.0, this issue happens at -O0
 * For all versions, this issue happens iff unaligned access is not supported.
 *
 * For gcc 4.x, this implementation will generate byte-by-byte loads even if unaligned access is
 * supported, which is correct but not optimal.
 *
 * For performance (and code size, in some cases), we want to avoid the branch and just generate
 * some inline load/store instructions since the access is small and constant-size.
 *
 * The manual states:
 * "The packed attribute specifies that a variable or structure field should have the smallest
 *  possible alignment—one byte for a variable"
 * https://gcc.gnu.org/onlinedocs/gcc-4.5.4/gcc/Variable-Attributes.html
 *
 * Previous implementations used __attribute__((__aligned__(1)), but had issues with a gcc bug:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94662
 *
 * Tested with several versions of GCC from 4.5.0 up to 13.2.0
 * We don't enable for older than 4.5.0 as this has not been tested.
 */
 #define UINT_UNALIGNED_STRUCT
typedef struct {
    uint16_t x;
} __attribute__((packed)) mbedtls_uint16_unaligned_t;
typedef struct {
    uint32_t x;
} __attribute__((packed)) mbedtls_uint32_unaligned_t;
typedef struct {
    uint64_t x;
} __attribute__((packed)) mbedtls_uint64_unaligned_t;
 #endif

/*
 * We try to force mbedtls_(get|put)_unaligned_uintXX to be always inline, because this results
 * in code that is both smaller and faster. IAR and gcc both benefit from this when optimising
 * for size.
 */

/**
 * Read the unsigned 16 bits integer from the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 2 bytes of data
 * \return  Data at the given address
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline uint16_t mbedtls_get_unaligned_uint16(const void *p)
{
    uint16_t r;
#if defined(UINT_UNALIGNED)
    mbedtls_uint16_unaligned_t *p16 = (mbedtls_uint16_unaligned_t *) p;
    r = *p16;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint16_unaligned_t *p16 = (mbedtls_uint16_unaligned_t *) p;
    r = p16->x;
#else
    memcpy(&r, p, sizeof(r));
#endif
    return r;
}

/**
 * Write the unsigned 16 bits integer to the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 2 bytes of data
 * \param   x data to write
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline void mbedtls_put_unaligned_uint16(void *p, uint16_t x)
{
#if defined(UINT_UNALIGNED)
    mbedtls_uint16_unaligned_t *p16 = (mbedtls_uint16_unaligned_t *) p;
    *p16 = x;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint16_unaligned_t *p16 = (mbedtls_uint16_unaligned_t *) p;
    p16->x = x;
#else
    memcpy(p, &x, sizeof(x));
#endif
}

/**
 * Read the unsigned 32 bits integer from the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 4 bytes of data
 * \return  Data at the given address
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline uint32_t mbedtls_get_unaligned_uint32(const void *p)
{
    uint32_t r;
#if defined(UINT_UNALIGNED)
    mbedtls_uint32_unaligned_t *p32 = (mbedtls_uint32_unaligned_t *) p;
    r = *p32;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint32_unaligned_t *p32 = (mbedtls_uint32_unaligned_t *) p;
    r = p32->x;
#else
    memcpy(&r, p, sizeof(r));
#endif
    return r;
}

/**
 * Write the unsigned 32 bits integer to the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 4 bytes of data
 * \param   x data to write
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline void mbedtls_put_unaligned_uint32(void *p, uint32_t x)
{
#if defined(UINT_UNALIGNED)
    mbedtls_uint32_unaligned_t *p32 = (mbedtls_uint32_unaligned_t *) p;
    *p32 = x;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint32_unaligned_t *p32 = (mbedtls_uint32_unaligned_t *) p;
    p32->x = x;
#else
    memcpy(p, &x, sizeof(x));
#endif
}

/**
 * Read the unsigned 64 bits integer from the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 8 bytes of data
 * \return  Data at the given address
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline uint64_t mbedtls_get_unaligned_uint64(const void *p)
{
    uint64_t r;
#if defined(UINT_UNALIGNED)
    mbedtls_uint64_unaligned_t *p64 = (mbedtls_uint64_unaligned_t *) p;
    r = *p64;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint64_unaligned_t *p64 = (mbedtls_uint64_unaligned_t *) p;
    r = p64->x;
#else
    memcpy(&r, p, sizeof(r));
#endif
    return r;
}

/**
 * Write the unsigned 64 bits integer to the given address, which need not
 * be aligned.
 *
 * \param   p pointer to 8 bytes of data
 * \param   x data to write
 */
#if defined(__IAR_SYSTEMS_ICC__)
#pragma inline = forced
#elif defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline void mbedtls_put_unaligned_uint64(void *p, uint64_t x)
{
#if defined(UINT_UNALIGNED)
    mbedtls_uint64_unaligned_t *p64 = (mbedtls_uint64_unaligned_t *) p;
    *p64 = x;
#elif defined(UINT_UNALIGNED_STRUCT)
    mbedtls_uint64_unaligned_t *p64 = (mbedtls_uint64_unaligned_t *) p;
    p64->x = x;
#else
    memcpy(p, &x, sizeof(x));
#endif
}

#if defined(MBEDTLS_POP_IAR_LANGUAGE_PRAGMA)
#pragma language=restore
#endif

/** Byte Reading Macros
 *
 * Given a multi-byte integer \p x, MBEDTLS_BYTE_n retrieves the n-th
 * byte from x, where byte 0 is the least significant byte.
 */
#define MBEDTLS_BYTE_0(x) ((uint8_t) ((x)         & 0xff))
#define MBEDTLS_BYTE_1(x) ((uint8_t) (((x) >>  8) & 0xff))
#define MBEDTLS_BYTE_2(x) ((uint8_t) (((x) >> 16) & 0xff))
#define MBEDTLS_BYTE_3(x) ((uint8_t) (((x) >> 24) & 0xff))
#define MBEDTLS_BYTE_4(x) ((uint8_t) (((x) >> 32) & 0xff))
#define MBEDTLS_BYTE_5(x) ((uint8_t) (((x) >> 40) & 0xff))
#define MBEDTLS_BYTE_6(x) ((uint8_t) (((x) >> 48) & 0xff))
#define MBEDTLS_BYTE_7(x) ((uint8_t) (((x) >> 56) & 0xff))

/*
 * Detect GCC built-in byteswap routines
 */
#if defined(__GNUC__) && defined(__GNUC_PREREQ)
#if __GNUC_PREREQ(4, 8)
#define MBEDTLS_BSWAP16 __builtin_bswap16
#endif /* __GNUC_PREREQ(4,8) */
#if __GNUC_PREREQ(4, 3)
#define MBEDTLS_BSWAP32 __builtin_bswap32
#define MBEDTLS_BSWAP64 __builtin_bswap64
#endif /* __GNUC_PREREQ(4,3) */
#endif /* defined(__GNUC__) && defined(__GNUC_PREREQ) */

/*
 * Detect Clang built-in byteswap routines
 */
#if defined(__clang__) && defined(__has_builtin)
#if __has_builtin(__builtin_bswap16) && !defined(MBEDTLS_BSWAP16)
#define MBEDTLS_BSWAP16 __builtin_bswap16
#endif /* __has_builtin(__builtin_bswap16) */
#if __has_builtin(__builtin_bswap32) && !defined(MBEDTLS_BSWAP32)
#define MBEDTLS_BSWAP32 __builtin_bswap32
#endif /* __has_builtin(__builtin_bswap32) */
#if __has_builtin(__builtin_bswap64) && !defined(MBEDTLS_BSWAP64)
#define MBEDTLS_BSWAP64 __builtin_bswap64
#endif /* __has_builtin(__builtin_bswap64) */
#endif /* defined(__clang__) && defined(__has_builtin) */

/*
 * Detect MSVC built-in byteswap routines
 */
#if defined(_MSC_VER)
#if !defined(MBEDTLS_BSWAP16)
#define MBEDTLS_BSWAP16 _byteswap_ushort
#endif
#if !defined(MBEDTLS_BSWAP32)
#define MBEDTLS_BSWAP32 _byteswap_ulong
#endif
#if !defined(MBEDTLS_BSWAP64)
#define MBEDTLS_BSWAP64 _byteswap_uint64
#endif
#endif /* defined(_MSC_VER) */

/* Detect armcc built-in byteswap routine */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 410000) && !defined(MBEDTLS_BSWAP32)
#if defined(__ARM_ACLE)  /* ARM Compiler 6 - earlier versions don't need a header */
#include <arm_acle.h>
#endif
#define MBEDTLS_BSWAP32 __rev
#endif

/* Detect IAR built-in byteswap routine */
#if defined(__IAR_SYSTEMS_ICC__)
#if defined(__ARM_ACLE)
#include <arm_acle.h>
#define MBEDTLS_BSWAP16(x) ((uint16_t) __rev16((uint32_t) (x)))
#define MBEDTLS_BSWAP32 __rev
#define MBEDTLS_BSWAP64 __revll
#endif
#endif

/*
 * Where compiler built-ins are not present, fall back to C code that the
 * compiler may be able to detect and transform into the relevant bswap or
 * similar instruction.
 */
#if !defined(MBEDTLS_BSWAP16)
static inline uint16_t mbedtls_bswap16(uint16_t x)
{
    return
        (x & 0x00ff) << 8 |
        (x & 0xff00) >> 8;
}
#define MBEDTLS_BSWAP16 mbedtls_bswap16
#endif /* !defined(MBEDTLS_BSWAP16) */

#if !defined(MBEDTLS_BSWAP32)
static inline uint32_t mbedtls_bswap32(uint32_t x)
{
    return
        (x & 0x000000ff) << 24 |
        (x & 0x0000ff00) <<  8 |
        (x & 0x00ff0000) >>  8 |
        (x & 0xff000000) >> 24;
}
#define MBEDTLS_BSWAP32 mbedtls_bswap32
#endif /* !defined(MBEDTLS_BSWAP32) */

#if !defined(MBEDTLS_BSWAP64)
static inline uint64_t mbedtls_bswap64(uint64_t x)
{
    return
        (x & 0x00000000000000ffULL) << 56 |
        (x & 0x000000000000ff00ULL) << 40 |
        (x & 0x0000000000ff0000ULL) << 24 |
        (x & 0x00000000ff000000ULL) <<  8 |
        (x & 0x000000ff00000000ULL) >>  8 |
        (x & 0x0000ff0000000000ULL) >> 24 |
        (x & 0x00ff000000000000ULL) >> 40 |
        (x & 0xff00000000000000ULL) >> 56;
}
#define MBEDTLS_BSWAP64 mbedtls_bswap64
#endif /* !defined(MBEDTLS_BSWAP64) */

#if !defined(__BYTE_ORDER__)

#if defined(__LITTLE_ENDIAN__)
/* IAR defines __xxx_ENDIAN__, but not __BYTE_ORDER__ */
#define MBEDTLS_IS_BIG_ENDIAN 0
#elif defined(__BIG_ENDIAN__)
#define MBEDTLS_IS_BIG_ENDIAN 1
#else
static const uint16_t mbedtls_byte_order_detector = { 0x100 };
#define MBEDTLS_IS_BIG_ENDIAN (*((unsigned char *) (&mbedtls_byte_order_detector)) == 0x01)
#endif

#else

#if (__BYTE_ORDER__) == (__ORDER_BIG_ENDIAN__)
#define MBEDTLS_IS_BIG_ENDIAN 1
#else
#define MBEDTLS_IS_BIG_ENDIAN 0
#endif

#endif /* !defined(__BYTE_ORDER__) */

/**
 * Get the unsigned 32 bits integer corresponding to four bytes in
 * big-endian order (MSB first).
 *
 * \param   data    Base address of the memory to get the four bytes from.
 * \param   offset  Offset from \p data of the first and most significant
 *                  byte of the four bytes to build the 32 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT32_BE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? mbedtls_get_unaligned_uint32((data) + (offset))                  \
        : MBEDTLS_BSWAP32(mbedtls_get_unaligned_uint32((data) + (offset))) \
    )

/**
 * Put in memory a 32 bits unsigned integer in big-endian order.
 *
 * \param   n       32 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 32
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the most significant
 *                  byte of the 32 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT32_BE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint32((data) + (offset), (uint32_t) (n));     \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint32((data) + (offset), MBEDTLS_BSWAP32((uint32_t) (n))); \
        }                                                                        \
    }

/**
 * Get the unsigned 32 bits integer corresponding to four bytes in
 * little-endian order (LSB first).
 *
 * \param   data    Base address of the memory to get the four bytes from.
 * \param   offset  Offset from \p data of the first and least significant
 *                  byte of the four bytes to build the 32 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT32_LE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? MBEDTLS_BSWAP32(mbedtls_get_unaligned_uint32((data) + (offset))) \
        : mbedtls_get_unaligned_uint32((data) + (offset))                  \
    )


/**
 * Put in memory a 32 bits unsigned integer in little-endian order.
 *
 * \param   n       32 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 32
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the least significant
 *                  byte of the 32 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT32_LE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint32((data) + (offset), MBEDTLS_BSWAP32((uint32_t) (n))); \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint32((data) + (offset), ((uint32_t) (n)));   \
        }                                                                        \
    }

/**
 * Get the unsigned 16 bits integer corresponding to two bytes in
 * little-endian order (LSB first).
 *
 * \param   data    Base address of the memory to get the two bytes from.
 * \param   offset  Offset from \p data of the first and least significant
 *                  byte of the two bytes to build the 16 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT16_LE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? MBEDTLS_BSWAP16(mbedtls_get_unaligned_uint16((data) + (offset))) \
        : mbedtls_get_unaligned_uint16((data) + (offset))                  \
    )

/**
 * Put in memory a 16 bits unsigned integer in little-endian order.
 *
 * \param   n       16 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 16
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the least significant
 *                  byte of the 16 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT16_LE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint16((data) + (offset), MBEDTLS_BSWAP16((uint16_t) (n))); \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint16((data) + (offset), (uint16_t) (n));     \
        }                                                                        \
    }

/**
 * Get the unsigned 16 bits integer corresponding to two bytes in
 * big-endian order (MSB first).
 *
 * \param   data    Base address of the memory to get the two bytes from.
 * \param   offset  Offset from \p data of the first and most significant
 *                  byte of the two bytes to build the 16 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT16_BE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? mbedtls_get_unaligned_uint16((data) + (offset))                  \
        : MBEDTLS_BSWAP16(mbedtls_get_unaligned_uint16((data) + (offset))) \
    )

/**
 * Put in memory a 16 bits unsigned integer in big-endian order.
 *
 * \param   n       16 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 16
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the most significant
 *                  byte of the 16 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT16_BE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint16((data) + (offset), (uint16_t) (n));     \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint16((data) + (offset), MBEDTLS_BSWAP16((uint16_t) (n))); \
        }                                                                        \
    }

/**
 * Get the unsigned 24 bits integer corresponding to three bytes in
 * big-endian order (MSB first).
 *
 * \param   data    Base address of the memory to get the three bytes from.
 * \param   offset  Offset from \p data of the first and most significant
 *                  byte of the three bytes to build the 24 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT24_BE(data, offset)        \
    (                                              \
        ((uint32_t) (data)[(offset)] << 16)        \
        | ((uint32_t) (data)[(offset) + 1] << 8)   \
        | ((uint32_t) (data)[(offset) + 2])        \
    )

/**
 * Put in memory a 24 bits unsigned integer in big-endian order.
 *
 * \param   n       24 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 24
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the most significant
 *                  byte of the 24 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT24_BE(n, data, offset)                \
    {                                                         \
        (data)[(offset)] = MBEDTLS_BYTE_2(n);                 \
        (data)[(offset) + 1] = MBEDTLS_BYTE_1(n);             \
        (data)[(offset) + 2] = MBEDTLS_BYTE_0(n);             \
    }

/**
 * Get the unsigned 24 bits integer corresponding to three bytes in
 * little-endian order (LSB first).
 *
 * \param   data    Base address of the memory to get the three bytes from.
 * \param   offset  Offset from \p data of the first and least significant
 *                  byte of the three bytes to build the 24 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT24_LE(data, offset)               \
    (                                                     \
        ((uint32_t) (data)[(offset)])                     \
        | ((uint32_t) (data)[(offset) + 1] <<  8)         \
        | ((uint32_t) (data)[(offset) + 2] << 16)         \
    )

/**
 * Put in memory a 24 bits unsigned integer in little-endian order.
 *
 * \param   n       24 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 24
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the least significant
 *                  byte of the 24 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT24_LE(n, data, offset)                \
    {                                                         \
        (data)[(offset)] = MBEDTLS_BYTE_0(n);                 \
        (data)[(offset) + 1] = MBEDTLS_BYTE_1(n);             \
        (data)[(offset) + 2] = MBEDTLS_BYTE_2(n);             \
    }

/**
 * Get the unsigned 64 bits integer corresponding to eight bytes in
 * big-endian order (MSB first).
 *
 * \param   data    Base address of the memory to get the eight bytes from.
 * \param   offset  Offset from \p data of the first and most significant
 *                  byte of the eight bytes to build the 64 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT64_BE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? mbedtls_get_unaligned_uint64((data) + (offset))                  \
        : MBEDTLS_BSWAP64(mbedtls_get_unaligned_uint64((data) + (offset))) \
    )

/**
 * Put in memory a 64 bits unsigned integer in big-endian order.
 *
 * \param   n       64 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 64
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the most significant
 *                  byte of the 64 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT64_BE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint64((data) + (offset), (uint64_t) (n));     \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint64((data) + (offset), MBEDTLS_BSWAP64((uint64_t) (n))); \
        }                                                                        \
    }

/**
 * Get the unsigned 64 bits integer corresponding to eight bytes in
 * little-endian order (LSB first).
 *
 * \param   data    Base address of the memory to get the eight bytes from.
 * \param   offset  Offset from \p data of the first and least significant
 *                  byte of the eight bytes to build the 64 bits unsigned
 *                  integer from.
 */
#define MBEDTLS_GET_UINT64_LE(data, offset)                                \
    ((MBEDTLS_IS_BIG_ENDIAN)                                               \
        ? MBEDTLS_BSWAP64(mbedtls_get_unaligned_uint64((data) + (offset))) \
        : mbedtls_get_unaligned_uint64((data) + (offset))                  \
    )

/**
 * Put in memory a 64 bits unsigned integer in little-endian order.
 *
 * \param   n       64 bits unsigned integer to put in memory.
 * \param   data    Base address of the memory where to put the 64
 *                  bits unsigned integer in.
 * \param   offset  Offset from \p data where to put the least significant
 *                  byte of the 64 bits unsigned integer \p n.
 */
#define MBEDTLS_PUT_UINT64_LE(n, data, offset)                                   \
    {                                                                            \
        if (MBEDTLS_IS_BIG_ENDIAN)                                               \
        {                                                                        \
            mbedtls_put_unaligned_uint64((data) + (offset), MBEDTLS_BSWAP64((uint64_t) (n))); \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            mbedtls_put_unaligned_uint64((data) + (offset), (uint64_t) (n));     \
        }                                                                        \
    }

#endif /* MBEDTLS_LIBRARY_ALIGNMENT_H */
