/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/*
   Central point for static initialization.

   In case of "lazy" mode `BrotliXxxLazyStaticInit` is not provided by the
   library. Embedder is responsible for providing it. This function should call
   `BrotliXxxLazyStaticInitInner` on the first invocation. This function should
   not return until execution of `BrotliXxxLazyStaticInitInner` is finished.
   In C or before C++11 it is possible to call `BrotliXxxLazyStaticInitInner`
   on start-up path and then `BrotliEncoderLazyStaticInit` is could be no-op;
   another option is to use available thread execution controls to meet the
   requirements. For possible C++11 implementation see static_init_lazy.cc.
*/

#ifndef THIRD_PARTY_BROTLI_COMMON_STATIC_INIT_H_
#define THIRD_PARTY_BROTLI_COMMON_STATIC_INIT_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Static data is "initialized" in compile time. */
#define BROTLI_STATIC_INIT_NONE 0
/* Static data is initialized before "main". */
#define BROTLI_STATIC_INIT_EARLY 1
/* Static data is initialized when first encoder is created. */
#define BROTLI_STATIC_INIT_LAZY 2

#define BROTLI_STATIC_INIT_DEFAULT BROTLI_STATIC_INIT_NONE

#if !defined(BROTLI_STATIC_INIT)
#define BROTLI_STATIC_INIT BROTLI_STATIC_INIT_DEFAULT
#endif

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE) && \
    (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_EARLY) && \
    (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_LAZY)
#error Invalid value for BROTLI_STATIC_INIT
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_EARLY)
#if defined(BROTLI_EXTERNAL_DICTIONARY_DATA)
#error BROTLI_STATIC_INIT_EARLY will fail with BROTLI_EXTERNAL_DICTIONARY_DATA
#endif
#endif  /* BROTLI_STATIC_INIT */

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  // THIRD_PARTY_BROTLI_COMMON_STATIC_INIT_H_
