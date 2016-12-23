// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ALLOCATION_H_
#define V8_ALLOCATION_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

// Called when allocation routines fail to allocate.
// This function should not return, but should terminate the current
// processing.
V8_EXPORT_PRIVATE void FatalProcessOutOfMemory(const char* message);

// Superclass for classes managed with new & delete.
class V8_EXPORT_PRIVATE Malloced {
 public:
  void* operator new(size_t size) { return New(size); }
  void  operator delete(void* p) { Delete(p); }

  static void* New(size_t size);
  static void Delete(void* p);
};


// A macro is used for defining the base class used for embedded instances.
// The reason is some compilers allocate a minimum of one word for the
// superclass. The macro prevents the use of new & delete in debug mode.
// In release mode we are not willing to pay this overhead.

#ifdef DEBUG
// Superclass for classes with instances allocated inside stack
// activations or inside other objects.
class Embedded {
 public:
  void* operator new(size_t size);
  void  operator delete(void* p);
};
#define BASE_EMBEDDED : public Embedded
#else
#define BASE_EMBEDDED
#endif


// Superclass for classes only using static method functions.
// The subclass of AllStatic cannot be instantiated at all.
class AllStatic {
#ifdef DEBUG
 public:
  AllStatic() = delete;
#endif
};


template <typename T>
T* NewArray(size_t size) {
  T* result = new T[size];
  if (result == NULL) FatalProcessOutOfMemory("NewArray");
  return result;
}


template <typename T>
void DeleteArray(T* array) {
  delete[] array;
}


// The normal strdup functions use malloc.  These versions of StrDup
// and StrNDup uses new and calls the FatalProcessOutOfMemory handler
// if allocation fails.
V8_EXPORT_PRIVATE char* StrDup(const char* str);
char* StrNDup(const char* str, int n);


// Allocation policy for allocating in the C free store using malloc
// and free. Used as the default policy for lists.
class FreeStoreAllocationPolicy {
 public:
  INLINE(void* New(size_t size)) { return Malloced::New(size); }
  INLINE(static void Delete(void* p)) { Malloced::Delete(p); }
};


void* AlignedAlloc(size_t size, size_t alignment);
void AlignedFree(void *ptr);

}  // namespace internal
}  // namespace v8

#endif  // V8_ALLOCATION_H_
