/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "./encoder_dict.h"

#include "../common/dictionary.h"
#include "../common/transform.h"
#include "./dictionary_hash.h"
#include "./hash.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

void BrotliInitEncoderDictionary(BrotliEncoderDictionary* dict) {
  dict->words = BrotliGetDictionary();

  dict->hash_table = kStaticDictionaryHash;
  dict->buckets = kStaticDictionaryBuckets;
  dict->dict_words = kStaticDictionaryWords;

  dict->cutoffTransformsCount = kCutoffTransformsCount;
  dict->cutoffTransforms = kCutoffTransforms;

}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
