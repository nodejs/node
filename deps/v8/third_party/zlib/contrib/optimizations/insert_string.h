/* insert_string.h
 *
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */
#ifdef _MSC_VER
#define INLINE __inline
#else
#define INLINE inline
#endif

#include "cpu_features.h"
/* Optimized insert_string block */
#if defined(CRC32_SIMD_SSE42_PCLMUL) || defined(CRC32_ARMV8_CRC32)
#define TARGET_CPU_WITH_CRC
// clang-format off
#if defined(CRC32_SIMD_SSE42_PCLMUL)
  /* Required to make MSVC bot build pass. */
  #include <smmintrin.h>
  #if defined(__GNUC__) || defined(__clang__)
    #undef TARGET_CPU_WITH_CRC
    #define TARGET_CPU_WITH_CRC __attribute__((target("sse4.2")))
  #endif

  #define _cpu_crc32_u32 _mm_crc32_u32

#elif defined(CRC32_ARMV8_CRC32)
  #if defined(__clang__)
    #undef TARGET_CPU_WITH_CRC
    #define __crc32cw __builtin_arm_crc32cw
  #endif

  #define _cpu_crc32_u32 __crc32cw

  #if defined(__aarch64__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("crc")))
  #else  // !defined(__aarch64__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("armv8-a,crc")))
  #endif  // defined(__aarch64__)
#endif
// clang-format on
TARGET_CPU_WITH_CRC
local INLINE Pos insert_string_optimized(deflate_state* const s,
                                         const Pos str) {
  Pos ret;
  unsigned *ip, val, h = 0;

  ip = (unsigned*)&s->window[str];
  val = *ip;

  if (s->level >= 6)
    val &= 0xFFFFFF;

  /* Unlike the case of data integrity checks for GZIP format where the
   * polynomial used is defined (https://tools.ietf.org/html/rfc1952#page-11),
   * here it is just a hash function for the hash table used while
   * performing compression.
   */
  h = _cpu_crc32_u32(h, val);

  ret = s->head[h & s->hash_mask];
  s->head[h & s->hash_mask] = str;
  s->prev[str & s->w_mask] = ret;
  return ret;
}
#endif /* Optimized insert_string block */

/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to UPDATE_HASH are made with consecutive input
 *    characters, so that a running hash key can be computed from the previous
 *    key instead of complete recalculation each time.
 */
#define UPDATE_HASH(s, h, c) (h = (((h) << s->hash_shift) ^ (c)) & s->hash_mask)

/* ===========================================================================
 * Insert string str in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * If this file is compiled with -DFASTEST, the compression level is forced
 * to 1, and no hash chains are maintained.
 * IN  assertion: all calls to INSERT_STRING are made with consecutive input
 *    characters and the first MIN_MATCH bytes of str are valid (except for
 *    the last MIN_MATCH-1 bytes of the input file).
 */
local INLINE Pos insert_string_c(deflate_state* const s, const Pos str) {
  Pos ret;

  UPDATE_HASH(s, s->ins_h, s->window[(str) + (MIN_MATCH - 1)]);
#ifdef FASTEST
  ret = s->head[s->ins_h];
#else
  ret = s->prev[str & s->w_mask] = s->head[s->ins_h];
#endif
  s->head[s->ins_h] = str;

  return ret;
}

local INLINE Pos insert_string(deflate_state* const s, const Pos str) {
/* String dictionary insertion: faster symbol hashing has a positive impact
 * on data compression speeds (around 20% on Intel and 36% on Arm Cortex big
 * cores).
 * A misfeature is that the generated compressed output will differ from
 * vanilla zlib (even though it is still valid 'DEFLATE-d' content).
 *
 * We offer here a way to disable the optimization if there is the expectation
 * that compressed content should match when compared to vanilla zlib.
 */
#if !defined(CHROMIUM_ZLIB_NO_CASTAGNOLI)
  /* TODO(cavalcantii): unify CPU features code. */
#if defined(CRC32_ARMV8_CRC32)
  if (arm_cpu_enable_crc32)
    return insert_string_optimized(s, str);
#elif defined(CRC32_SIMD_SSE42_PCLMUL)
  if (x86_cpu_enable_simd)
    return insert_string_optimized(s, str);
#endif
#endif
  return insert_string_c(s, str);
}
