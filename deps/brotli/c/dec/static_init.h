/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Central point for static initialization. */

#ifndef THIRD_PARTY_BROTLI_DEC_STATIC_INIT_H_
#define THIRD_PARTY_BROTLI_DEC_STATIC_INIT_H_

#include "../common/platform.h"
#include "../common/static_init.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_LAZY)
BROTLI_INTERNAL void BrotliDecoderLazyStaticInitInner(void);
BROTLI_INTERNAL void BrotliDecoderLazyStaticInit(void);
#endif  /* BROTLI_STATIC_INIT */

BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderEnsureStaticInit(void);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  // THIRD_PARTY_BROTLI_DEC_STATIC_INIT_H_
