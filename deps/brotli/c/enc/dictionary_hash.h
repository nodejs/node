/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Hash table on the 4-byte prefixes of static dictionary words. */

#ifndef BROTLI_ENC_DICTIONARY_HASH_H_
#define BROTLI_ENC_DICTIONARY_HASH_H_

#include "../common/platform.h"
#include "../common/static_init.h"

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
#include "../common/dictionary.h"
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Bucket is (Hash14 * 2 + length_lt_8); in other words we reserve 2 buckets
   for each hash - one for shorter words and one for longer words. */
#define BROTLI_ENC_NUM_HASH_BUCKETS 32768

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
BROTLI_BOOL BROTLI_INTERNAL BrotliEncoderInitDictionaryHash(
    const BrotliDictionary* dictionary, uint16_t* words, uint8_t* lengths);
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint16_t
    kStaticDictionaryHashWords[BROTLI_ENC_NUM_HASH_BUCKETS];
BROTLI_INTERNAL extern BROTLI_MODEL("small") uint8_t
    kStaticDictionaryHashLengths[BROTLI_ENC_NUM_HASH_BUCKETS];
#else
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint16_t
    kStaticDictionaryHashWords[BROTLI_ENC_NUM_HASH_BUCKETS];
BROTLI_INTERNAL extern const BROTLI_MODEL("small") uint8_t
    kStaticDictionaryHashLengths[BROTLI_ENC_NUM_HASH_BUCKETS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_DICTIONARY_HASH_H_ */
