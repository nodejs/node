/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "dictionary.h"
#include "platform.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if !defined(BROTLI_EXTERNAL_DICTIONARY_DATA)
/* Embed kBrotliDictionaryData */
#include "dictionary_inc.h"
static const BROTLI_MODEL("small") BrotliDictionary kBrotliDictionary = {
#else
static BROTLI_MODEL("small") BrotliDictionary kBrotliDictionary = {
#endif
  /* size_bits_by_length */
  {
    0, 0, 0, 0, 10, 10, 11, 11,
    10, 10, 10, 10, 10, 9, 9, 8,
    7, 7, 8, 7, 7, 6, 6, 5,
    5, 0, 0, 0, 0, 0, 0, 0
  },

  /* offsets_by_length */
  {
    0, 0, 0, 0, 0, 4096, 9216, 21504,
    35840, 44032, 53248, 63488, 74752, 87040, 93696, 100864,
    104704, 106752, 108928, 113536, 115968, 118528, 119872, 121280,
    122016, 122784, 122784, 122784, 122784, 122784, 122784, 122784
  },

  /* data_size ==  sizeof(kBrotliDictionaryData) */
  122784,

  /* data */
#if defined(BROTLI_EXTERNAL_DICTIONARY_DATA)
  NULL
#else
  kBrotliDictionaryData
#endif
};

const BrotliDictionary* BrotliGetDictionary(void) {
  return &kBrotliDictionary;
}

void BrotliSetDictionaryData(const uint8_t* data) {
#if defined(BROTLI_EXTERNAL_DICTIONARY_DATA)
  if (!!data && !kBrotliDictionary.data) {
    kBrotliDictionary.data = data;
  }
#else
  BROTLI_UNUSED(data);  // Appease -Werror=unused-parameter
#endif
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
