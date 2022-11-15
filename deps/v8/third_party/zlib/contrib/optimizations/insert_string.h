/* insert_string.h
 *
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the Chromium source repository LICENSE file.
 */

#ifndef INSERT_STRING_H
#define INSERT_STRING_H

#ifndef INLINE
#if defined(_MSC_VER) && !defined(__clang__)
#define INLINE __inline
#else
#define INLINE inline
#endif
#endif

#include "cpu_features.h"

// clang-format off
#if defined(CRC32_SIMD_SSE42_PCLMUL)
  #include <smmintrin.h>  /* Required to make MSVC bot build pass. */

  #if defined(__clang__) || defined(__GNUC__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("sse4.2")))
  #else
    #define TARGET_CPU_WITH_CRC
  #endif

  /* CRC32C uint32_t */
  #define _cpu_crc32c_hash_u32 _mm_crc32_u32

#elif defined(CRC32_ARMV8_CRC32)
  #if defined(__clang__)
    #define __crc32cw __builtin_arm_crc32cw
  #elif defined(__GNUC__)
    #define __crc32cw __builtin_aarch64_crc32cw
  #endif

  #if defined(__aarch64__) && defined(__clang__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("crc")))
  #elif defined(__aarch64__) && defined(__GNUC__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("+crc")))
  #elif defined(__clang__) // !defined(__aarch64__)
    #define TARGET_CPU_WITH_CRC __attribute__((target("armv8-a,crc")))
  #endif  // defined(__aarch64__)

  /* CRC32C uint32_t */
  #define _cpu_crc32c_hash_u32 __crc32cw

#endif
// clang-format on

#if defined(TARGET_CPU_WITH_CRC)

TARGET_CPU_WITH_CRC
local INLINE Pos insert_string_simd(deflate_state* const s, const Pos str) {
  Pos ret;
  unsigned *ip, val, h = 0;

  ip = (unsigned*)&s->window[str];
  val = *ip;

  if (s->level >= 6)
    val &= 0xFFFFFF;

  /* Compute hash from the CRC32C of |val|. */
  h = _cpu_crc32c_hash_u32(h, val);

  ret = s->head[h & s->hash_mask];
  s->head[h & s->hash_mask] = str;
  s->prev[str & s->w_mask] = ret;
  return ret;
}

#endif // TARGET_CPU_WITH_CRC

/**
 * Some applications need to match zlib DEFLATE output exactly [3]. Use the
 * canonical zlib Rabin-Karp rolling hash [1,2] in that case.
 *
 *  [1] For a description of the Rabin and Karp algorithm, see "Algorithms"
 *      book by R. Sedgewick, Addison-Wesley, p252.
 *  [2] https://www.euccas.me/zlib/#zlib_rabin_karp and also "rolling hash"
 *      https://en.wikipedia.org/wiki/Rolling_hash
 *  [3] crbug.com/1316541 AOSP incremental client APK package OTA upgrades.
 */
#ifdef CHROMIUM_ZLIB_NO_CASTAGNOLI
#define USE_ZLIB_RABIN_KARP_ROLLING_HASH
#endif

/* ===========================================================================
 * Update a hash value with the given input byte (Rabin-Karp rolling hash).
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
/* insert_string_simd string dictionary insertion: SIMD crc32c symbol hasher
 * significantly improves data compression speed.
 *
 * Note: the generated compressed output is a valid DEFLATE stream, but will
 * differ from canonical zlib output.
 */
#if defined(USE_ZLIB_RABIN_KARP_ROLLING_HASH)
/* So this build-time option can be used to disable the crc32c hash, and use
 * the Rabin-Karp hash instead.
 */ /* FALLTHROUGH Rabin-Karp */
#elif defined(TARGET_CPU_WITH_CRC) && defined(CRC32_SIMD_SSE42_PCLMUL)
  if (x86_cpu_enable_simd)
    return insert_string_simd(s, str);
#elif defined(TARGET_CPU_WITH_CRC) && defined(CRC32_ARMV8_CRC32)
  if (arm_cpu_enable_crc32)
    return insert_string_simd(s, str);
#endif
  return insert_string_c(s, str); /* Rabin-Karp */
}

#endif /* INSERT_STRING_H */
