// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/allocation.h"

#include <stdlib.h>  // For free, malloc.
#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/utils.h"

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

namespace v8 {
namespace internal {

void* Malloced::New(size_t size) {
  void* result = malloc(size);
  if (result == NULL) {
    v8::internal::FatalProcessOutOfMemory("Malloced operator new");
  }
  return result;
}


void Malloced::Delete(void* p) {
  free(p);
}


void Malloced::FatalProcessOutOfMemory() {
  v8::internal::FatalProcessOutOfMemory("Out of memory");
}


#ifdef DEBUG

static void* invalid = static_cast<void*>(NULL);

void* Embedded::operator new(size_t size) {
  UNREACHABLE();
  return invalid;
}


void Embedded::operator delete(void* p) {
  UNREACHABLE();
}


void* AllStatic::operator new(size_t size) {
  UNREACHABLE();
  return invalid;
}


void AllStatic::operator delete(void* p) {
  UNREACHABLE();
}

#endif


char* StrDup(const char* str) {
  int length = StrLength(str);
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}


char* StrNDup(const char* str, int n) {
  int length = StrLength(str);
  if (n < length) length = n;
  char* result = NewArray<char>(length + 1);
  MemCopy(result, str, length);
  result[length] = '\0';
  return result;
}


void* AlignedAlloc(size_t size, size_t alignment) {
  DCHECK_LE(V8_ALIGNOF(void*), alignment);
  DCHECK(base::bits::IsPowerOfTwo32(alignment));
  void* ptr;
#if V8_OS_WIN
  ptr = _aligned_malloc(size, alignment);
#elif V8_LIBC_BIONIC
  // posix_memalign is not exposed in some Android versions, so we fall back to
  // memalign. See http://code.google.com/p/android/issues/detail?id=35391.
  ptr = memalign(alignment, size);
#else
  if (posix_memalign(&ptr, alignment, size)) ptr = NULL;
#endif
  if (ptr == NULL) FatalProcessOutOfMemory("AlignedAlloc");
  return ptr;
}


void AlignedFree(void *ptr) {
#if V8_OS_WIN
  _aligned_free(ptr);
#elif V8_LIBC_BIONIC
  // Using free is not correct in general, but for V8_LIBC_BIONIC it is.
  free(ptr);
#else
  free(ptr);
#endif
}

} }  // namespace v8::internal
