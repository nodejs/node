/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "static_init.h"

#include "../common/platform.h"
#include "../common/static_init.h"

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
#include "../common/dictionary.h"
#include "dictionary_hash.h"
#include "static_dict_lut.h"
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
static BROTLI_BOOL DoBrotliEncoderStaticInit(void) {
  const BrotliDictionary* dict = BrotliGetDictionary();
  BROTLI_BOOL ok = BrotliEncoderInitStaticDictionaryLut(
      dict, kStaticDictionaryBuckets, kStaticDictionaryWords);
  if (!ok) return BROTLI_FALSE;
  ok = BrotliEncoderInitDictionaryHash(dict, kStaticDictionaryHashWords,
                                       kStaticDictionaryHashLengths);
  if (!ok) return BROTLI_FALSE;
  return BROTLI_TRUE;
}
#endif  /* BROTLI_STATIC_INIT_NONE */

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_EARLY)
static BROTLI_BOOL kEarlyInitOk;
static __attribute__((constructor)) void BrotliEncoderStaticInitEarly(void) {
  kEarlyInitOk = DoBrotliEncoderStaticInit();
}
#elif (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_LAZY)
static BROTLI_BOOL kLazyInitOk;
void BrotliEncoderLazyStaticInitInner(void) {
  kLazyInitOk = DoBrotliEncoderStaticInit();
}
#endif  /* BROTLI_STATIC_INIT_EARLY */

BROTLI_BOOL BrotliEncoderEnsureStaticInit(void) {
#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_NONE)
  return BROTLI_TRUE;
#elif (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_EARLY)
  return kEarlyInitOk;
#else
  return kLazyInitOk;
#endif
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif
