// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_INL_H_
#define V8_HEAP_LOCAL_HEAP_INL_H_

#include <atomic>

#include "src/common/assert-scope.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap.h"
#include "src/heap/large-spaces.h"
#include "src/heap/local-heap.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/parked-scope.h"
#include "src/heap/zapping.h"

namespace v8 {
namespace internal {

AllocationResult LocalHeap::AllocateRaw(int size_in_bytes, AllocationType type,
                                        AllocationOrigin origin,
                                        AllocationAlignment alignment) {
  return heap_allocator_.AllocateRaw(size_in_bytes, type, origin, alignment);
}

template <typename LocalHeap::AllocationRetryMode mode>
Tagged<HeapObject> LocalHeap::AllocateRawWith(int object_size,
                                              AllocationType type,
                                              AllocationOrigin origin,
                                              AllocationAlignment alignment) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  DCHECK(!v8_flags.enable_third_party_heap);
  AllocationResult result = AllocateRaw(object_size, type, origin, alignment);
  Tagged<HeapObject> object;
  if (result.To(&object)) return object;
  result =
      PerformCollectionAndAllocateAgain(object_size, type, origin, alignment);
  if (result.To(&object)) return object;

  switch (mode) {
    case kRetryOrFail:
      heap_->FatalProcessOutOfMemory("LocalHeap: allocation failed");
    case kLightRetry:
      return HeapObject();
  }
}

Address LocalHeap::AllocateRawOrFail(int object_size, AllocationType type,
                                     AllocationOrigin origin,
                                     AllocationAlignment alignment) {
  return AllocateRawWith<kRetryOrFail>(object_size, type, origin, alignment)
      .address();
}

template <typename Callback>
V8_INLINE void LocalHeap::ParkAndExecuteCallback(Callback callback) {
  ParkedScope parked(this);
  // Provide the parked scope as a witness, if the callback expects it.
  if constexpr (std::is_invocable_v<Callback, const ParkedScope&>) {
    callback(parked);
  } else {
    callback();
  }
}

template <typename Callback>
V8_INLINE void LocalHeap::BlockWhileParked(Callback callback) {
  if (is_main_thread()) {
    BlockMainThreadWhileParked(callback);
  } else {
    ParkAndExecuteCallback(callback);
  }
}

template <typename Callback>
V8_INLINE void LocalHeap::BlockMainThreadWhileParked(Callback callback) {
  ExecuteWithStackMarker(
      [this, callback]() { ParkAndExecuteCallback(callback); });
}

template <typename Callback>
V8_INLINE void LocalHeap::ExecuteWithStackMarker(Callback callback) {
  // Conservative stack scanning is only performed for main threads, therefore
  // this method should only be invoked from the main thread. In this case,
  // heap()->stack() below is the stack object of the main thread that has last
  // entered the isolate.
  DCHECK(is_main_thread());
  heap()->stack().SetMarkerIfNeededAndCallback(callback);
}

template <typename Callback>
V8_INLINE void LocalHeap::ExecuteWithStackMarkerIfNeeded(Callback callback) {
  if (is_main_thread()) {
    ExecuteWithStackMarker(callback);
  } else {
    callback();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_INL_H_
