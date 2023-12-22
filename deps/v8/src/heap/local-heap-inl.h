// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_INL_H_
#define V8_HEAP_LOCAL_HEAP_INL_H_

#include <atomic>

#include "src/common/assert-scope.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/heap/zapping.h"

namespace v8 {
namespace internal {

AllocationResult LocalHeap::AllocateRaw(int size_in_bytes, AllocationType type,
                                        AllocationOrigin origin,
                                        AllocationAlignment alignment) {
  DCHECK(!v8_flags.enable_third_party_heap);
#if DEBUG
  VerifyCurrent();
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_IMPLIES(type == AllocationType::kCode || type == AllocationType::kMap,
                 alignment == AllocationAlignment::kTaggedAligned);
  Heap::HeapState state = heap()->gc_state();
  DCHECK(state == Heap::TEAR_DOWN || state == Heap::NOT_IN_GC);
  DCHECK(IsRunning());
#endif

  // Each allocation is supposed to be a safepoint.
  Safepoint();

  bool large_object = size_in_bytes > heap_->MaxRegularHeapObjectSize(type);

  if (type == AllocationType::kCode) {
    CodePageHeaderModificationScope header_modification_scope(
        "Code allocation needs header access.");

    AllocationResult alloc;
    if (large_object) {
      alloc =
          heap()->code_lo_space()->AllocateRawBackground(this, size_in_bytes);
    } else {
      alloc =
          code_space_allocator()->AllocateRaw(size_in_bytes, alignment, origin);
    }
    Tagged<HeapObject> object;
    if (heap::ShouldZapGarbage() && alloc.To(&object) &&
        !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
      heap::ZapCodeBlock(object.address(), size_in_bytes);
    }
    return alloc;
  }

  if (type == AllocationType::kOld) {
    if (large_object)
      return heap()->lo_space()->AllocateRawBackground(this, size_in_bytes);
    else
      return old_space_allocator()->AllocateRaw(size_in_bytes, alignment,
                                                origin);
  }

  if (type == AllocationType::kTrusted) {
    if (large_object)
      return heap()->trusted_lo_space()->AllocateRawBackground(this,
                                                               size_in_bytes);
    else
      return trusted_space_allocator()->AllocateRaw(size_in_bytes, alignment,
                                                    origin);
  }

  DCHECK_EQ(type, AllocationType::kSharedOld);
  if (large_object) {
    return heap()->shared_lo_allocation_space()->AllocateRawBackground(
        this, size_in_bytes);
  } else {
    return shared_old_space_allocator()->AllocateRaw(size_in_bytes, alignment,
                                                     origin);
  }
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
