/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Collection of static dictionary words. */

#ifndef BROTLI_COMMON_DICTIONARY_H_
#define BROTLI_COMMON_DICTIONARY_H_

#include "platform.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct BrotliDictionary {
  /**
   * Number of bits to encode index of dictionary word in a bucket.
   *
   * Specification: Appendix A. Static Dictionary Data
   *
   * Words in a dictionary are bucketed by length.
   * @c 0 means that there are no words of a given length.
   * Dictionary consists of words with length of [4..24] bytes.
   * Values at [0..3] and [25..31] indices should not be addressed.
   */
  uint8_t size_bits_by_length[32];

  /* assert(offset[i + 1] == offset[i] + (bits[i] ? (i << bits[i]) : 0)) */
  uint32_t offsets_by_length[32];

  /* assert(data_size == offsets_by_length[31]) */
  size_t data_size;

  /* Data array is not bound, and should obey to size_bits_by_length values.
     Specified size matches default (RFC 7932) dictionary. Its size is
     defined by data_size */
  const uint8_t* data;
} BrotliDictionary;

BROTLI_COMMON_API const BrotliDictionary* BrotliGetDictionary(void);

/**
 * Sets dictionary data.
 *
 * When dictionary data is already set / present, this method is no-op.
 *
 * Dictionary data MUST be provided before BrotliGetDictionary is invoked.
 * This method is used ONLY in multi-client environment (e.g. C + Java),
 * to reduce storage by sharing single dictionary between implementations.
 */
BROTLI_COMMON_API void BrotliSetDictionaryData(const uint8_t* data);

#define BROTLI_MIN_DICTIONARY_WORD_LENGTH 4
#define BROTLI_MAX_DICTIONARY_WORD_LENGTH 24

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_COMMON_DICTIONARY_H_ */
