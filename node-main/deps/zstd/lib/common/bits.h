/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_BITS_H
#define ZSTD_BITS_H

#include "mem.h"

MEM_STATIC unsigned ZSTD_countTrailingZeros32_fallback(U32 val)
{
    assert(val != 0);
    {
        static const U32 DeBruijnBytePos[32] = {0, 1, 28, 2, 29, 14, 24, 3,
                                                30, 22, 20, 15, 25, 17, 4, 8,
                                                31, 27, 13, 23, 21, 19, 16, 7,
                                                26, 12, 18, 6, 11, 5, 10, 9};
        return DeBruijnBytePos[((U32) ((val & -(S32) val) * 0x077CB531U)) >> 27];
    }
}

MEM_STATIC unsigned ZSTD_countTrailingZeros32(U32 val)
{
    assert(val != 0);
#if defined(_MSC_VER)
#  if STATIC_BMI2
    return (unsigned)_tzcnt_u32(val);
#  else
    if (val != 0) {
        unsigned long r;
        _BitScanForward(&r, val);
        return (unsigned)r;
    } else {
        __assume(0); /* Should not reach this code path */
    }
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
    return (unsigned)__builtin_ctz(val);
#elif defined(__ICCARM__)
    return (unsigned)__builtin_ctz(val);
#else
    return ZSTD_countTrailingZeros32_fallback(val);
#endif
}

MEM_STATIC unsigned ZSTD_countLeadingZeros32_fallback(U32 val)
{
    assert(val != 0);
    {
        static const U32 DeBruijnClz[32] = {0, 9, 1, 10, 13, 21, 2, 29,
                                            11, 14, 16, 18, 22, 25, 3, 30,
                                            8, 12, 20, 28, 15, 17, 24, 7,
                                            19, 27, 23, 6, 26, 5, 4, 31};
        val |= val >> 1;
        val |= val >> 2;
        val |= val >> 4;
        val |= val >> 8;
        val |= val >> 16;
        return 31 - DeBruijnClz[(val * 0x07C4ACDDU) >> 27];
    }
}

MEM_STATIC unsigned ZSTD_countLeadingZeros32(U32 val)
{
    assert(val != 0);
#if defined(_MSC_VER)
#  if STATIC_BMI2
    return (unsigned)_lzcnt_u32(val);
#  else
    if (val != 0) {
        unsigned long r;
        _BitScanReverse(&r, val);
        return (unsigned)(31 - r);
    } else {
        __assume(0); /* Should not reach this code path */
    }
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
    return (unsigned)__builtin_clz(val);
#elif defined(__ICCARM__)
    return (unsigned)__builtin_clz(val);
#else
    return ZSTD_countLeadingZeros32_fallback(val);
#endif
}

MEM_STATIC unsigned ZSTD_countTrailingZeros64(U64 val)
{
    assert(val != 0);
#if defined(_MSC_VER) && defined(_WIN64)
#  if STATIC_BMI2
    return (unsigned)_tzcnt_u64(val);
#  else
    if (val != 0) {
        unsigned long r;
        _BitScanForward64(&r, val);
        return (unsigned)r;
    } else {
        __assume(0); /* Should not reach this code path */
    }
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 4) && defined(__LP64__)
    return (unsigned)__builtin_ctzll(val);
#elif defined(__ICCARM__)
    return (unsigned)__builtin_ctzll(val);
#else
    {
        U32 mostSignificantWord = (U32)(val >> 32);
        U32 leastSignificantWord = (U32)val;
        if (leastSignificantWord == 0) {
            return 32 + ZSTD_countTrailingZeros32(mostSignificantWord);
        } else {
            return ZSTD_countTrailingZeros32(leastSignificantWord);
        }
    }
#endif
}

MEM_STATIC unsigned ZSTD_countLeadingZeros64(U64 val)
{
    assert(val != 0);
#if defined(_MSC_VER) && defined(_WIN64)
#  if STATIC_BMI2
    return (unsigned)_lzcnt_u64(val);
#  else
    if (val != 0) {
        unsigned long r;
        _BitScanReverse64(&r, val);
        return (unsigned)(63 - r);
    } else {
        __assume(0); /* Should not reach this code path */
    }
#  endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
    return (unsigned)(__builtin_clzll(val));
#elif defined(__ICCARM__)
    return (unsigned)(__builtin_clzll(val));
#else
    {
        U32 mostSignificantWord = (U32)(val >> 32);
        U32 leastSignificantWord = (U32)val;
        if (mostSignificantWord == 0) {
            return 32 + ZSTD_countLeadingZeros32(leastSignificantWord);
        } else {
            return ZSTD_countLeadingZeros32(mostSignificantWord);
        }
    }
#endif
}

MEM_STATIC unsigned ZSTD_NbCommonBytes(size_t val)
{
    if (MEM_isLittleEndian()) {
        if (MEM_64bits()) {
            return ZSTD_countTrailingZeros64((U64)val) >> 3;
        } else {
            return ZSTD_countTrailingZeros32((U32)val) >> 3;
        }
    } else {  /* Big Endian CPU */
        if (MEM_64bits()) {
            return ZSTD_countLeadingZeros64((U64)val) >> 3;
        } else {
            return ZSTD_countLeadingZeros32((U32)val) >> 3;
        }
    }
}

MEM_STATIC unsigned ZSTD_highbit32(U32 val)   /* compress, dictBuilder, decodeCorpus */
{
    assert(val != 0);
    return 31 - ZSTD_countLeadingZeros32(val);
}

/* ZSTD_rotateRight_*():
 * Rotates a bitfield to the right by "count" bits.
 * https://en.wikipedia.org/w/index.php?title=Circular_shift&oldid=991635599#Implementing_circular_shifts
 */
MEM_STATIC
U64 ZSTD_rotateRight_U64(U64 const value, U32 count) {
    assert(count < 64);
    count &= 0x3F; /* for fickle pattern recognition */
    return (value >> count) | (U64)(value << ((0U - count) & 0x3F));
}

MEM_STATIC
U32 ZSTD_rotateRight_U32(U32 const value, U32 count) {
    assert(count < 32);
    count &= 0x1F; /* for fickle pattern recognition */
    return (value >> count) | (U32)(value << ((0U - count) & 0x1F));
}

MEM_STATIC
U16 ZSTD_rotateRight_U16(U16 const value, U32 count) {
    assert(count < 16);
    count &= 0x0F; /* for fickle pattern recognition */
    return (value >> count) | (U16)(value << ((0U - count) & 0x0F));
}

#endif /* ZSTD_BITS_H */
