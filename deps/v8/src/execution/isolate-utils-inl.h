// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_UTILS_INL_H_
#define V8_EXECUTION_ISOLATE_UTILS_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate-utils.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-write-barrier-inl.h"

namespace v8 {
namespace internal {

inline constexpr IsolateRoot GetIsolateForPtrComprFromOnHeapAddress(
    Address address) {
#ifdef V8_COMPRESS_POINTERS
  return IsolateRoot(GetIsolateRootAddress(address));
#else
  return IsolateRoot();
#endif  // V8_COMPRESS_POINTERS
}

inline IsolateRoot GetIsolateForPtrCompr(HeapObject object) {
  return GetIsolateForPtrComprFromOnHeapAddress(object.ptr());
}

V8_INLINE Heap* GetHeapFromWritableObject(HeapObject object) {
  // Avoid using the below GetIsolateFromWritableObject because we want to be
  // able to get the heap, but not the isolate, for off-thread objects.

#if defined V8_ENABLE_THIRD_PARTY_HEAP
  return Heap::GetIsolateFromWritableObject(object)->heap();
#elif defined V8_COMPRESS_POINTERS
  Isolate* isolate =
      Isolate::FromRootAddress(GetIsolateRootAddress(object.ptr()));
  DCHECK_NOT_NULL(isolate);
  return isolate->heap();
#else
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->GetHeap();
#endif  // V8_COMPRESS_POINTERS || V8_ENABLE_THIRD_PARTY_HEAP
}

V8_INLINE Isolate* GetIsolateFromWritableObject(HeapObject object) {
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  return Heap::GetIsolateFromWritableObject(object);
#elif defined V8_COMPRESS_POINTERS
  Isolate* isolate =
      Isolate::FromRootAddress(GetIsolateRootAddress(object.ptr()));
  DCHECK_NOT_NULL(isolate);
  return isolate;
#else
  return Isolate::FromHeap(GetHeapFromWritableObject(object));
#endif  // V8_COMPRESS_POINTERS, V8_ENABLE_THIRD_PARTY_HEAP
}

V8_INLINE bool GetIsolateFromHeapObject(HeapObject object, Isolate** isolate) {
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  *isolate = Heap::GetIsolateFromWritableObject(object);
  return true;
#elif defined V8_COMPRESS_POINTERS
  *isolate = GetIsolateFromWritableObject(object);
  return true;
#else
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (chunk->InReadOnlySpace()) {
    *isolate = nullptr;
    return false;
  }
  *isolate = Isolate::FromHeap(chunk->GetHeap());
  return true;
#endif  // V8_COMPRESS_POINTERS, V8_ENABLE_THIRD_PARTY_HEAP
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_UTILS_INL_H_
