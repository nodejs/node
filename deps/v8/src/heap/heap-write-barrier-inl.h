// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/heap/heap-write-barrier.h"

#include "src/globals.h"
// TODO(jkummerow): Get rid of this by moving GetIsolateFromWritableObject
// elsewhere.
#include "src/isolate.h"
#include "src/objects/code.h"
#include "src/objects/compressed-slots-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

// Defined in heap.cc.
V8_EXPORT_PRIVATE bool Heap_PageFlagsAreConsistent(HeapObject object);
V8_EXPORT_PRIVATE void Heap_GenerationalBarrierSlow(HeapObject object,
                                                    Address slot,
                                                    HeapObject value);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierSlow(HeapObject object, Address slot,
                                               HeapObject value);
V8_EXPORT_PRIVATE void Heap_WriteBarrierForCodeSlow(Code host);
V8_EXPORT_PRIVATE void Heap_GenerationalBarrierForCodeSlow(Code host,
                                                           RelocInfo* rinfo,
                                                           HeapObject object);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierForCodeSlow(Code host,
                                                      RelocInfo* rinfo,
                                                      HeapObject object);
V8_EXPORT_PRIVATE void Heap_GenerationalBarrierForElementsSlow(Heap* heap,
                                                               FixedArray array,
                                                               int offset,
                                                               int length);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierForElementsSlow(Heap* heap,
                                                          HeapObject object);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierForDescriptorArraySlow(
    Heap* heap, HeapObject host, HeapObject descriptor_array,
    int number_of_own_descriptors);

// Do not use these internal details anywhere outside of this file. These
// internals are only intended to shortcut write barrier checks.
namespace heap_internals {

struct Space {
  static constexpr uintptr_t kIdOffset = 9 * kSystemPointerSize;
  V8_INLINE AllocationSpace identity() {
    return *reinterpret_cast<AllocationSpace*>(reinterpret_cast<Address>(this) +
                                               kIdOffset);
  }
};

struct MemoryChunk {
  static constexpr uintptr_t kFlagsOffset = sizeof(size_t);
  static constexpr uintptr_t kHeapOffset =
      kFlagsOffset + kUIntptrSize + 4 * kSystemPointerSize;
  static constexpr uintptr_t kOwnerOffset =
      kHeapOffset + 2 * kSystemPointerSize;
  static constexpr uintptr_t kMarkingBit = uintptr_t{1} << 18;
  static constexpr uintptr_t kFromPageBit = uintptr_t{1} << 3;
  static constexpr uintptr_t kToPageBit = uintptr_t{1} << 4;

  V8_INLINE static heap_internals::MemoryChunk* FromHeapObject(
      HeapObject object) {
    return reinterpret_cast<MemoryChunk*>(object->ptr() & ~kPageAlignmentMask);
  }

  V8_INLINE bool IsMarking() const { return GetFlags() & kMarkingBit; }

  V8_INLINE bool InYoungGeneration() const {
    constexpr uintptr_t kYoungGenerationMask = kFromPageBit | kToPageBit;
    return GetFlags() & kYoungGenerationMask;
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

  V8_INLINE Space* GetOwner() {
    return *reinterpret_cast<Space**>(reinterpret_cast<Address>(this) +
                                      kOwnerOffset);
  }
};

inline void GenerationalBarrierInternal(HeapObject object, Address slot,
                                        HeapObject value) {
  DCHECK(Heap_PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  if (!value_chunk->InYoungGeneration() || object_chunk->InYoungGeneration()) {
    return;
  }

  Heap_GenerationalBarrierSlow(object, slot, value);
}

inline void GenerationalEphemeronKeyBarrierInternal(EphemeronHashTable table,
                                                    Address slot,
                                                    HeapObject value) {
  DCHECK(Heap::PageFlagsAreConsistent(table));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);
  heap_internals::MemoryChunk* table_chunk =
      heap_internals::MemoryChunk::FromHeapObject(table);

  if (!value_chunk->InYoungGeneration() || table_chunk->InYoungGeneration()) {
    return;
  }

  Heap* heap = GetHeapFromWritableObject(table);
  heap->RecordEphemeronKeyWrite(table, slot);
}

inline void MarkingBarrierInternal(HeapObject object, Address slot,
                                   HeapObject value) {
  DCHECK(Heap_PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);

  if (!value_chunk->IsMarking()) return;

  Heap_MarkingBarrierSlow(object, slot, value);
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
  Heap_WriteBarrierForCodeSlow(host);
}

inline void GenerationalBarrier(HeapObject object, ObjectSlot slot,
                                Object value) {
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  heap_internals::GenerationalBarrierInternal(object, slot.address(),
                                              HeapObject::cast(value));
}

inline void GenerationalEphemeronKeyBarrier(EphemeronHashTable table,
                                            ObjectSlot slot, Object value) {
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  DCHECK(value->IsHeapObject());
  heap_internals::GenerationalEphemeronKeyBarrierInternal(
      table, slot.address(), HeapObject::cast(value));
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
  if (array_chunk->InYoungGeneration()) return;

  Heap_GenerationalBarrierForElementsSlow(heap, array, offset, length);
}

inline void GenerationalBarrierForCode(Code host, RelocInfo* rinfo,
                                       HeapObject object) {
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->InYoungGeneration()) return;
  Heap_GenerationalBarrierForCodeSlow(host, rinfo, object);
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

  Heap_MarkingBarrierForElementsSlow(heap, object);
}

inline void MarkingBarrierForCode(Code host, RelocInfo* rinfo,
                                  HeapObject object) {
  DCHECK(!HasWeakHeapObjectTag(object.ptr()));
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->IsMarking()) return;
  Heap_MarkingBarrierForCodeSlow(host, rinfo, object);
}

inline void MarkingBarrierForDescriptorArray(Heap* heap, HeapObject host,
                                             HeapObject descriptor_array,
                                             int number_of_own_descriptors) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(descriptor_array);
  if (!chunk->IsMarking()) return;

  Heap_MarkingBarrierForDescriptorArraySlow(heap, host, descriptor_array,
                                            number_of_own_descriptors);
}

inline WriteBarrierMode GetWriteBarrierModeForObject(
    HeapObject object, const DisallowHeapAllocation* promise) {
  DCHECK(Heap_PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (chunk->IsMarking()) return UPDATE_WRITE_BARRIER;
  if (chunk->InYoungGeneration()) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}

inline bool ObjectInYoungGeneration(const Object object) {
  if (object.IsSmi()) return false;
  return heap_internals::MemoryChunk::FromHeapObject(HeapObject::cast(object))
      ->InYoungGeneration();
}

inline Heap* GetHeapFromWritableObject(const HeapObject object) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->GetHeap();
}

inline bool GetIsolateFromWritableObject(HeapObject obj, Isolate** isolate) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(obj);
  if (chunk->GetOwner()->identity() == RO_SPACE) {
    *isolate = nullptr;
    return false;
  }
  *isolate = Isolate::FromHeap(chunk->GetHeap());
  return true;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
