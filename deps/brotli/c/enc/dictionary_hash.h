/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Hash table on the 4-byte prefixes of static dictionary words. */

#ifndef BROTLI_ENC_DICTIONARY_HASH_H_
#define BROTLI_ENC_DICTIONARY_HASH_H_

#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

extern const uint16_t kStaticDictionaryHash[32768];

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_DICTIONARY_HASH_H_ */
