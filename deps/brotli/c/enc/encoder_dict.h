/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#ifndef BROTLI_ENC_ENCODER_DICT_H_
#define BROTLI_ENC_ENCODER_DICT_H_

#include "../common/dictionary.h"
#include "../common/platform.h"
#include <brotli/types.h>
#include "./static_dict_lut.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Dictionary data (words and transforms) for 1 possible context */
typedef struct BrotliEncoderDictionary {
  const BrotliDictionary* words;

  /* cut off for fast encoder */
  uint32_t cutoffTransformsCount;
  uint64_t cutoffTransforms;

  /* from dictionary_hash.h, for fast encoder */
  const uint16_t* hash_table;

  /* from static_dict_lut.h, for slow encoder */
  const uint16_t* buckets;
  const DictWord* dict_words;
} BrotliEncoderDictionary;

BROTLI_INTERNAL void BrotliInitEncoderDictionary(BrotliEncoderDictionary* dict);


#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_ENCODER_DICT_H_ */
