// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_UTILS_INL_H_
#define V8_EXECUTION_ISOLATE_UTILS_INL_H_

#include "src/execution/isolate-utils.h"

#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-write-barrier-inl.h"

namespace v8 {
namespace internal {

inline Isolate* GetIsolateForPtrCompr(HeapObject object) {
#ifdef V8_COMPRESS_POINTERS
  return Isolate::FromRoot(GetIsolateRoot(object.ptr()));
#else
  return nullptr;
#endif  // V8_COMPRESS_POINTERS
}

V8_INLINE Heap* GetHeapFromWritableObject(HeapObject object) {
#ifdef V8_COMPRESS_POINTERS
  return GetIsolateFromWritableObject(object)->heap();
#else
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->GetHeap();
#endif  // V8_COMPRESS_POINTERS
}

V8_INLINE Isolate* GetIsolateFromWritableObject(HeapObject object) {
#ifdef V8_COMPRESS_POINTERS
  Isolate* isolate = Isolate::FromRoot(GetIsolateRoot(object.ptr()));
  DCHECK_NOT_NULL(isolate);
  return isolate;
#else
  return Isolate::FromHeap(GetHeapFromWritableObject(object));
#endif  // V8_COMPRESS_POINTERS
}

V8_INLINE bool GetIsolateFromHeapObject(HeapObject object, Isolate** isolate) {
#ifdef V8_COMPRESS_POINTERS
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
#endif  // V8_COMPRESS_POINTERS
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_UTILS_INL_H_
