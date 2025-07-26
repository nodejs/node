// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_UTILS_INL_H_
#define V8_EXECUTION_ISOLATE_UTILS_INL_H_

#include "src/execution/isolate-utils.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/sandbox/isolate.h"

namespace v8::internal {

// TODO(396607238): Replace all callers with `Isolate::Current()->heap()`.
V8_INLINE Heap* GetHeapFromWritableObject(Tagged<HeapObject> object) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  // Do not use this method on shared objects. This method would always return
  // the shared space isolate for shared objects. However, on worker isolates
  // this might be different from the current isolate. In such cases either
  // require the current isolate as an additional argument from the caller or
  // use Isolate::Current(). From there you can access the shared space isolate
  // with `isolate->shared_space_isolate()` if needed.
  DCHECK(!chunk->InWritableSharedSpace());
  Heap* heap = chunk->GetHeap();
  // See the TODO above: The heap/isolate returned here must match TLS.
  CHECK_EQ(heap->isolate(), Isolate::TryGetCurrent());
  return heap;
}

// TODO(396607238): Replace all callers with `Isolate::Current()`.
V8_INLINE Isolate* GetIsolateFromWritableObject(Tagged<HeapObject> object) {
  return Isolate::FromHeap(GetHeapFromWritableObject(object));
}

V8_INLINE Heap* GetHeapFromWritableObject(const HeapObjectLayout& object) {
  return GetHeapFromWritableObject(Tagged(&object));
}

V8_INLINE Isolate* GetIsolateFromWritableObject(
    const HeapObjectLayout& object) {
  return GetIsolateFromWritableObject(Tagged(&object));
}

V8_INLINE bool GetIsolateFromHeapObject(Tagged<HeapObject> object,
                                        Isolate** isolate) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  if (chunk->InReadOnlySpace()) {
    *isolate = nullptr;
    return false;
  }
  *isolate = Isolate::FromHeap(chunk->GetHeap());
  return true;
}

}  // namespace v8::internal

#endif  // V8_EXECUTION_ISOLATE_UTILS_INL_H_
