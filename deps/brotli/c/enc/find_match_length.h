/* Copyright 2010 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Function to find maximal matching prefixes of strings. */

#ifndef BROTLI_ENC_FIND_MATCH_LENGTH_H_
#define BROTLI_ENC_FIND_MATCH_LENGTH_H_

#include "../common/platform.h"
#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Separate implementation for little-endian 64-bit targets, for speed. */
#if defined(BROTLI_TZCNT64) && BROTLI_64_BITS && BROTLI_LITTLE_ENDIAN
static BROTLI_INLINE size_t FindMatchLengthWithLimit(const uint8_t* s1,
                                                     const uint8_t* s2,
                                                     size_t limit) {
  size_t matched = 0;
  size_t limit2 = (limit >> 3) + 1;  /* + 1 is for pre-decrement in while */
  while (BROTLI_PREDICT_TRUE(--limit2)) {
    if (BROTLI_PREDICT_FALSE(BROTLI_UNALIGNED_LOAD64LE(s2) ==
                      BROTLI_UNALIGNED_LOAD64LE(s1 + matched))) {
      s2 += 8;
      matched += 8;
    } else {
      uint64_t x = BROTLI_UNALIGNED_LOAD64LE(s2) ^
          BROTLI_UNALIGNED_LOAD64LE(s1 + matched);
      size_t matching_bits = (size_t)BROTLI_TZCNT64(x);
      matched += matching_bits >> 3;
      return matched;
    }
  }
  limit = (limit & 7) + 1;  /* + 1 is for pre-decrement in while */
  while (--limit) {
    if (BROTLI_PREDICT_TRUE(s1[matched] == *s2)) {
      ++s2;
      ++matched;
    } else {
      return matched;
    }
  }
  return matched;
}
#else
static BROTLI_INLINE size_t FindMatchLengthWithLimit(const uint8_t* s1,
                                                     const uint8_t* s2,
                                                     size_t limit) {
  size_t matched = 0;
  const uint8_t* s2_limit = s2 + limit;
  const uint8_t* s2_ptr = s2;
  /* Find out how long the match is. We loop over the data 32 bits at a
     time until we find a 32-bit block that doesn't match; then we find
     the first non-matching bit and use that to calculate the total
     length of the match. */
  while (s2_ptr <= s2_limit - 4 &&
         BrotliUnalignedRead32(s2_ptr) ==
         BrotliUnalignedRead32(s1 + matched)) {
    s2_ptr += 4;
    matched += 4;
  }
  while ((s2_ptr < s2_limit) && (s1[matched] == *s2_ptr)) {
    ++s2_ptr;
    ++matched;
  }
  return matched;
}
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_FIND_MATCH_LENGTH_H_ */
