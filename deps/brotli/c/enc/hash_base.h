/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Basic common hash functions / constants. */

#ifndef THIRD_PARTY_BROTLI_ENC_HASH_BASE_H_
#define THIRD_PARTY_BROTLI_ENC_HASH_BASE_H_

#include "../common/platform.h"

/* kHashMul32 multiplier has these properties:
   * The multiplier must be odd. Otherwise we may lose the highest bit.
   * No long streaks of ones or zeros.
   * There is no effort to ensure that it is a prime, the oddity is enough
     for this use.
   * The number has been tuned heuristically against compression benchmarks. */
static const uint32_t kHashMul32 = 0x1E35A7BD;
static const uint64_t kHashMul64 =
    BROTLI_MAKE_UINT64_T(0x1FE35A7Bu, 0xD3579BD3u);

static BROTLI_INLINE uint32_t Hash14(const uint8_t* data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;
  /* The higher bits contain more mixture from the multiplication,
     so we take our results from there. */
  return h >> (32 - 14);
}

static BROTLI_INLINE uint32_t Hash15(const uint8_t* data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;
  /* The higher bits contain more mixture from the multiplication,
     so we take our results from there. */
  return h >> (32 - 15);
}

#endif  // THIRD_PARTY_BROTLI_ENC_HASH_BASE_H_
