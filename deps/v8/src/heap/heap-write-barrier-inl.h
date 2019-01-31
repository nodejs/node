// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/heap/heap-write-barrier.h"

#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/objects/code.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

// Do not use these internal details anywhere outside of this file. These
// internals are only intended to shortcut write barrier checks.
namespace heap_internals {

struct MemoryChunk {
  static constexpr uintptr_t kFlagsOffset = sizeof(size_t);
  static constexpr uintptr_t kHeapOffset =
      kFlagsOffset + kUIntptrSize + 4 * kSystemPointerSize;
  static constexpr uintptr_t kMarkingBit = uintptr_t{1} << 18;
  static constexpr uintptr_t kFromSpaceBit = uintptr_t{1} << 3;
  static constexpr uintptr_t kToSpaceBit = uintptr_t{1} << 4;

  V8_INLINE static heap_internals::MemoryChunk* FromHeapObject(
      HeapObject object) {
    return reinterpret_cast<MemoryChunk*>(object->ptr() & ~kPageAlignmentMask);
  }

  V8_INLINE bool IsMarking() const { return GetFlags() & kMarkingBit; }

  V8_INLINE bool InNewSpace() const {
    constexpr uintptr_t kNewSpaceMask = kFromSpaceBit | kToSpaceBit;
    return GetFlags() & kNewSpaceMask;
  }

  V8_INLINE uintptr_t GetFlags() const {
    return *reinterpret_cast<const uintptr_t*>(reinterpret_cast<Address>(this) +
                                               kFlagsOffset);
  }

  V8_INLINE Heap* GetHeap() {
    Heap* heap = *reinterpret_cast<Heap**>(reinterpret_cast<Address>(this) +
                                           kHeapOffset);
    SLOW_DCHECK(heap != nullptr);
    return heap;
  }
};

inline void GenerationalBarrierInternal(HeapObject object, Address slot,
                                        HeapObject value) {
  DCHECK(Heap::PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  if (!value_chunk->InNewSpace() || object_chunk->InNewSpace()) return;

  Heap::GenerationalBarrierSlow(object, slot, value);
}

inline void MarkingBarrierInternal(HeapObject object, Address slot,
                                   HeapObject value) {
  DCHECK(Heap::PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);

  if (!value_chunk->IsMarking()) return;

  Heap::MarkingBarrierSlow(object, slot, value);
}

}  // namespace heap_internals

inline void WriteBarrierForCode(Code host, RelocInfo* rinfo, Object value) {
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  HeapObject object = HeapObject::cast(value);
  GenerationalBarrierForCode(host, rinfo, object);
  MarkingBarrierForCode(host, rinfo, object);
}

inline void WriteBarrierForCode(Code host) {
  Heap::WriteBarrierForCodeSlow(host);
}

inline void GenerationalBarrier(HeapObject object, ObjectSlot slot,
                                Object value) {
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  heap_internals::GenerationalBarrierInternal(object, slot.address(),
                                              HeapObject::cast(value));
}

inline void GenerationalBarrier(HeapObject object, MaybeObjectSlot slot,
                                MaybeObject value) {
  HeapObject value_heap_object;
  if (!value->GetHeapObject(&value_heap_object)) return;
  heap_internals::GenerationalBarrierInternal(object, slot.address(),
                                              value_heap_object);
}

inline void GenerationalBarrierForElements(Heap* heap, FixedArray array,
                                           int offset, int length) {
  heap_internals::MemoryChunk* array_chunk =
      heap_internals::MemoryChunk::FromHeapObject(array);
  if (array_chunk->InNewSpace()) return;

  Heap::GenerationalBarrierForElementsSlow(heap, array, offset, length);
}

inline void GenerationalBarrierForCode(Code host, RelocInfo* rinfo,
                                       HeapObject object) {
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->InNewSpace()) return;
  Heap::GenerationalBarrierForCodeSlow(host, rinfo, object);
}

inline void MarkingBarrier(HeapObject object, ObjectSlot slot, Object value) {
  DCHECK_IMPLIES(slot.address() != kNullAddress, !HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  heap_internals::MarkingBarrierInternal(object, slot.address(),
                                         HeapObject::cast(value));
}

inline void MarkingBarrier(HeapObject object, MaybeObjectSlot slot,
                           MaybeObject value) {
  HeapObject value_heap_object;
  if (!value->GetHeapObject(&value_heap_object)) return;
  heap_internals::MarkingBarrierInternal(object, slot.address(),
                                         value_heap_object);
}

inline void MarkingBarrierForElements(Heap* heap, HeapObject object) {
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->IsMarking()) return;

  Heap::MarkingBarrierForElementsSlow(heap, object);
}

inline void MarkingBarrierForCode(Code host, RelocInfo* rinfo,
                                  HeapObject object) {
  DCHECK(!HasWeakHeapObjectTag(object.ptr()));
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->IsMarking()) return;
  Heap::MarkingBarrierForCodeSlow(host, rinfo, object);
}

inline void MarkingBarrierForDescriptorArray(Heap* heap, HeapObject host,
                                             HeapObject descriptor_array,
                                             int number_of_own_descriptors) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(descriptor_array);
  if (!chunk->IsMarking()) return;

  Heap::MarkingBarrierForDescriptorArraySlow(heap, host, descriptor_array,
                                             number_of_own_descriptors);
}

inline Heap* GetHeapFromWritableObject(const HeapObject object) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->GetHeap();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
