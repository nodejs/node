/* Copyright 2016 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Macros for memory management. */

#ifndef BROTLI_ENC_MEMORY_H_
#define BROTLI_ENC_MEMORY_H_

#include <string.h>  /* memcpy */

#include "../common/platform.h"
#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if !defined(BROTLI_ENCODER_CLEANUP_ON_OOM) && \
    !defined(BROTLI_ENCODER_EXIT_ON_OOM)
#define BROTLI_ENCODER_EXIT_ON_OOM
#endif

typedef struct MemoryManager {
  brotli_alloc_func alloc_func;
  brotli_free_func free_func;
  void* opaque;
#if !defined(BROTLI_ENCODER_EXIT_ON_OOM)
  BROTLI_BOOL is_oom;
  size_t perm_allocated;
  size_t new_allocated;
  size_t new_freed;
  void* pointers[256];
#endif  /* BROTLI_ENCODER_EXIT_ON_OOM */
} MemoryManager;

BROTLI_INTERNAL void BrotliInitMemoryManager(
    MemoryManager* m, brotli_alloc_func alloc_func, brotli_free_func free_func,
    void* opaque);

BROTLI_INTERNAL void* BrotliAllocate(MemoryManager* m, size_t n);
#define BROTLI_ALLOC(M, T, N)                               \
  ((N) > 0 ? ((T*)BrotliAllocate((M), (N) * sizeof(T))) : NULL)

BROTLI_INTERNAL void BrotliFree(MemoryManager* m, void* p);
#define BROTLI_FREE(M, P) { \
  BrotliFree((M), (P));     \
  P = NULL;                 \
}

#if defined(BROTLI_ENCODER_EXIT_ON_OOM)
#define BROTLI_IS_OOM(M) (!!0)
#else  /* BROTLI_ENCODER_EXIT_ON_OOM */
#define BROTLI_IS_OOM(M) (!!(M)->is_oom)
#endif  /* BROTLI_ENCODER_EXIT_ON_OOM */

BROTLI_INTERNAL void BrotliWipeOutMemoryManager(MemoryManager* m);

/*
Dynamically grows array capacity to at least the requested size
M: MemoryManager
T: data type
A: array
C: capacity
R: requested size
*/
#define BROTLI_ENSURE_CAPACITY(M, T, A, C, R) {  \
  if (C < (R)) {                                 \
    size_t _new_size = (C == 0) ? (R) : C;       \
    T* new_array;                                \
    while (_new_size < (R)) _new_size *= 2;      \
    new_array = BROTLI_ALLOC((M), T, _new_size); \
    if (!BROTLI_IS_OOM(M) && C != 0)             \
      memcpy(new_array, A, C * sizeof(T));       \
    BROTLI_FREE((M), A);                         \
    A = new_array;                               \
    C = _new_size;                               \
  }                                              \
}

/*
Appends value and dynamically grows array capacity when needed
M: MemoryManager
T: data type
A: array
C: array capacity
S: array size
V: value to append
*/
#define BROTLI_ENSURE_CAPACITY_APPEND(M, T, A, C, S, V) { \
  (S)++;                                                  \
  BROTLI_ENSURE_CAPACITY(M, T, A, C, S);                  \
  A[(S) - 1] = (V);                                       \
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_MEMORY_H_ */
