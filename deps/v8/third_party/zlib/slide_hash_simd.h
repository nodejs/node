/* slide_hash_simd.h
 *
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */

#ifndef SLIDE_HASH_SIMD_H
#define SLIDE_HASH_SIMD_H

#include "deflate.h"

#ifndef INLINE
#if defined(_MSC_VER) && !defined(__clang__)
#define INLINE __inline
#else
#define INLINE inline
#endif
#endif

#if defined(CPU_NO_SIMD)

#error SIMD has been disabled for your build target

#elif defined(DEFLATE_SLIDE_HASH_SSE2)

#include <emmintrin.h>  /* SSE2 */

#define Z_SLIDE_INIT_SIMD(wsize) _mm_set1_epi16((ush)(wsize))

#define Z_SLIDE_HASH_SIMD(table, size, vector_wsize) \
    for (const Posf* const end = table + size; table != end;) { \
        __m128i vO = _mm_loadu_si128((__m128i *)(table + 0)); \
        vO = _mm_subs_epu16(vO, vector_wsize); \
        _mm_storeu_si128((__m128i *)(table + 0), vO); \
        table += 8; \
    }

typedef __m128i z_vec128i_u16x8_t;

#elif defined(DEFLATE_SLIDE_HASH_NEON)

#include <arm_neon.h>  /* NEON */

#define Z_SLIDE_INIT_SIMD(wsize) vdupq_n_u16((ush)(wsize))

#define Z_SLIDE_HASH_SIMD(table, size, vector_wsize) \
    for (const Posf* const end = table + size; table != end;) { \
        uint16x8_t vO = vld1q_u16(table + 0); \
        uint16x8_t v8 = vld1q_u16(table + 8); \
        vO = vqsubq_u16(vO, vector_wsize); \
        v8 = vqsubq_u16(v8, vector_wsize); \
        vst1q_u16(table + 0, vO); \
        vst1q_u16(table + 8, v8); \
        table += 8 + 8; \
    }

typedef uint16x8_t z_vec128i_u16x8_t;

#else

#error slide_hash_simd is not defined for your build target

#endif

/* ===========================================================================
 * Slide the hash table when sliding the window down (could be avoided with 32
 * bit values at the expense of memory usage). We slide even when level == 0 to
 * keep the hash table consistent if we switch back to level > 0 later.
 */
local INLINE void slide_hash_simd(
    Posf *head, Posf *prev, const uInt w_size, const uInt hash_size) {
    /*
     * The SIMD implementation of the hash table slider assumes:
     *
     * 1. hash chain offset is 2 bytes. Should be true as Pos is "ush" type.
     */
    Assert(sizeof(Pos) == 2, "Pos type size error: should be 2 bytes");
    Assert(sizeof(ush) == 2, "ush type size error: should be 2 bytes");

    Assert(hash_size <= (1 << 16), "Hash table maximum size error");
    Assert(hash_size >= (1 << 8), "Hash table minimum size error");
    Assert(w_size == (ush)w_size, "Prev table size error");

    /*
     * 2. The hash & prev table sizes are a multiple of 32 bytes (256 bits),
     * since the NEON table slider moves two 128-bit items per loop (loop is
     * unrolled on NEON for performance, see http://crbug.com/863257).
     */
    Assert(!((hash_size * sizeof(head[0])) & (32 - 1)),
        "Hash table size error: should be a multiple of 32 bytes");
    Assert(!((w_size * sizeof(prev[0])) & (32 - 1)),
        "Prev table size error: should be a multiple of 32 bytes");

    /*
     * Duplicate (ush)w_size in each uint16_t component of a 128-bit vector.
     */
    const z_vec128i_u16x8_t vec_wsize = Z_SLIDE_INIT_SIMD(w_size);

    /*
     * Slide {head,prev} hash chain values: subtracts (ush)w_size from every
     * value with a saturating SIMD subtract, to clamp the result to 0(NIL),
     * to implement slide_hash() `(m >= wsize ? m - wsize : NIL);` code.
     */
    Z_SLIDE_HASH_SIMD(head, hash_size, vec_wsize);
#ifndef FASTEST
    Z_SLIDE_HASH_SIMD(prev, w_size, vec_wsize);
#endif

}

#undef z_vec128i_u16x8_t
#undef Z_SLIDE_HASH_SIMD
#undef Z_SLIDE_INIT_SIMD

#endif  /* SLIDE_HASH_SIMD_H */
