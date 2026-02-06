/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Lookup table for static dictionary and transforms. */

#ifndef BROTLI_ENC_STATIC_DICT_LUT_H_
#define BROTLI_ENC_STATIC_DICT_LUT_H_

#include "../common/dictionary.h"
#include "../common/platform.h"
#include "../common/static_init.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct DictWord {
  /* Highest bit is used to indicate end of bucket. */
  uint8_t len;
  uint8_t transform;
  uint16_t idx;
} DictWord;

/* Buckets are Hash15 results. */
#define BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS 32768
#define BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS 31705

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
BROTLI_INTERNAL BROTLI_BOOL BrotliEncoderInitStaticDictionaryLut(
    const BrotliDictionary* dictionary, uint16_t* buckets, DictWord* words);
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint16_t
    kStaticDictionaryBuckets[BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS];
BROTLI_INTERNAL extern BROTLI_MODEL("small") DictWord
    kStaticDictionaryWords[BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS];
#else
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint16_t
    kStaticDictionaryBuckets[BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS];
BROTLI_INTERNAL extern const BROTLI_MODEL("small") DictWord
    kStaticDictionaryWords[BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_STATIC_DICT_LUT_H_ */
