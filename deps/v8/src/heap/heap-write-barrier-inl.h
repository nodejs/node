// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/heap/heap-write-barrier.h"

#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/compressed-slots-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/slots-inl.h"

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

V8_EXPORT_PRIVATE void Heap_MarkingBarrierForArrayBufferExtensionSlow(
    HeapObject object, ArrayBufferExtension* extension);

V8_EXPORT_PRIVATE void Heap_GenerationalBarrierForCodeSlow(Code host,
                                                           RelocInfo* rinfo,
                                                           HeapObject object);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierForCodeSlow(Code host,
                                                      RelocInfo* rinfo,
                                                      HeapObject object);
V8_EXPORT_PRIVATE void Heap_MarkingBarrierForDescriptorArraySlow(
    Heap* heap, HeapObject host, HeapObject descriptor_array,
    int number_of_own_descriptors);

V8_EXPORT_PRIVATE void Heap_GenerationalEphemeronKeyBarrierSlow(
    Heap* heap, EphemeronHashTable table, Address slot);

// Do not use these internal details anywhere outside of this file. These
// internals are only intended to shortcut write barrier checks.
namespace heap_internals {

struct MemoryChunk {
  static constexpr uintptr_t kFlagsOffset = kSizetSize;
  static constexpr uintptr_t kHeapOffset =
      kSizetSize + kUIntptrSize + kSystemPointerSize;
  static constexpr uintptr_t kMarkingBit = uintptr_t{1} << 18;
  static constexpr uintptr_t kFromPageBit = uintptr_t{1} << 3;
  static constexpr uintptr_t kToPageBit = uintptr_t{1} << 4;
  static constexpr uintptr_t kReadOnlySpaceBit = uintptr_t{1} << 21;

  V8_INLINE static heap_internals::MemoryChunk* FromHeapObject(
      HeapObject object) {
    return reinterpret_cast<MemoryChunk*>(object.ptr() & ~kPageAlignmentMask);
  }

  V8_INLINE bool IsMarking() const { return GetFlags() & kMarkingBit; }

  V8_INLINE bool InYoungGeneration() const {
    if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
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
    DCHECK_NOT_NULL(heap);
    return heap;
  }

  V8_INLINE bool InReadOnlySpace() const {
    return GetFlags() & kReadOnlySpaceBit;
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

  Heap_GenerationalEphemeronKeyBarrierSlow(table_chunk->GetHeap(), table, slot);
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
  if (!value.IsHeapObject()) return;
  WriteBarrierForCode(host, rinfo, HeapObject::cast(value));
}

inline void WriteBarrierForCode(Code host, RelocInfo* rinfo, HeapObject value) {
  GenerationalBarrierForCode(host, rinfo, value);
  MarkingBarrierForCode(host, rinfo, value);
}

inline void WriteBarrierForCode(Code host) {
  Heap_WriteBarrierForCodeSlow(host);
}

inline void MarkingBarrierForArrayBufferExtension(
    HeapObject object, ArrayBufferExtension* extension) {
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!extension || !object_chunk->IsMarking()) return;
  Heap_MarkingBarrierForArrayBufferExtensionSlow(object, extension);
}

inline void GenerationalBarrier(HeapObject object, ObjectSlot slot,
                                Object value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value.IsHeapObject()) return;
  GenerationalBarrier(object, slot, HeapObject::cast(value));
}

inline void GenerationalBarrier(HeapObject object, ObjectSlot slot,
                                HeapObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK(!HasWeakHeapObjectTag(*slot));
  heap_internals::GenerationalBarrierInternal(object, slot.address(),
                                              HeapObject::cast(value));
}

inline void GenerationalEphemeronKeyBarrier(EphemeronHashTable table,
                                            ObjectSlot slot, Object value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  DCHECK(value.IsHeapObject());
  heap_internals::GenerationalEphemeronKeyBarrierInternal(
      table, slot.address(), HeapObject::cast(value));
}

inline void GenerationalBarrier(HeapObject object, MaybeObjectSlot slot,
                                MaybeObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  HeapObject value_heap_object;
  if (!value->GetHeapObject(&value_heap_object)) return;
  heap_internals::GenerationalBarrierInternal(object, slot.address(),
                                              value_heap_object);
}

inline void GenerationalBarrierForCode(Code host, RelocInfo* rinfo,
                                       HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->InYoungGeneration()) return;
  Heap_GenerationalBarrierForCodeSlow(host, rinfo, object);
}

inline void MarkingBarrier(HeapObject object, ObjectSlot slot, Object value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value.IsHeapObject()) return;
  MarkingBarrier(object, slot, HeapObject::cast(value));
}

inline void MarkingBarrier(HeapObject object, ObjectSlot slot,
                           HeapObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK_IMPLIES(slot.address() != kNullAddress, !HasWeakHeapObjectTag(*slot));
  heap_internals::MarkingBarrierInternal(object, slot.address(),
                                         HeapObject::cast(value));
}

inline void MarkingBarrier(HeapObject object, MaybeObjectSlot slot,
                           MaybeObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  HeapObject value_heap_object;
  if (!value->GetHeapObject(&value_heap_object)) return;
  heap_internals::MarkingBarrierInternal(object, slot.address(),
                                         value_heap_object);
}

inline void MarkingBarrierForCode(Code host, RelocInfo* rinfo,
                                  HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  DCHECK(!HasWeakHeapObjectTag(object));
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->IsMarking()) return;
  Heap_MarkingBarrierForCodeSlow(host, rinfo, object);
}

inline void MarkingBarrierForDescriptorArray(Heap* heap, HeapObject host,
                                             HeapObject descriptor_array,
                                             int number_of_own_descriptors) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(descriptor_array);
  if (!chunk->IsMarking()) return;

  Heap_MarkingBarrierForDescriptorArraySlow(heap, host, descriptor_array,
                                            number_of_own_descriptors);
}

inline WriteBarrierMode GetWriteBarrierModeForObject(
    HeapObject object, const DisallowHeapAllocation* promise) {
  if (FLAG_disable_write_barriers) return SKIP_WRITE_BARRIER;
  DCHECK(Heap_PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (chunk->IsMarking()) return UPDATE_WRITE_BARRIER;
  if (chunk->InYoungGeneration()) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}

inline bool ObjectInYoungGeneration(Object object) {
  // TODO(rong): Fix caller of this function when we deploy
  // v8_use_third_party_heap.
  if (FLAG_single_generation) return false;
  if (object.IsSmi()) return false;
  return heap_internals::MemoryChunk::FromHeapObject(HeapObject::cast(object))
      ->InYoungGeneration();
}

inline bool IsReadOnlyHeapObject(HeapObject object) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->InReadOnlySpace();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
