// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/common/code-memory-access-inl.h"
#include "src/common/globals.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/marking-barrier.h"
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
V8_EXPORT_PRIVATE void Heap_CombinedGenerationalAndSharedBarrierSlow(
    HeapObject object, Address slot, HeapObject value);
V8_EXPORT_PRIVATE void Heap_CombinedGenerationalAndSharedEphemeronBarrierSlow(
    EphemeronHashTable table, Address slot, HeapObject value);

V8_EXPORT_PRIVATE void Heap_GenerationalBarrierForCodeSlow(RelocInfo* rinfo,
                                                           HeapObject object);

V8_EXPORT_PRIVATE void Heap_GenerationalEphemeronKeyBarrierSlow(
    Heap* heap, HeapObject table, Address slot);

inline bool IsCodeSpaceObject(HeapObject object);

// Do not use these internal details anywhere outside of this file. These
// internals are only intended to shortcut write barrier checks.
namespace heap_internals {

struct MemoryChunk {
  static constexpr uintptr_t kFlagsOffset = kSizetSize;
  static constexpr uintptr_t kHeapOffset = kSizetSize + kUIntptrSize;
  static constexpr uintptr_t kInWritableSharedSpaceBit = uintptr_t{1} << 0;
  static constexpr uintptr_t kFromPageBit = uintptr_t{1} << 3;
  static constexpr uintptr_t kToPageBit = uintptr_t{1} << 4;
  static constexpr uintptr_t kMarkingBit = uintptr_t{1} << 5;
  static constexpr uintptr_t kReadOnlySpaceBit = uintptr_t{1} << 6;
  static constexpr uintptr_t kIsExecutableBit = uintptr_t{1} << 21;

  V8_INLINE static heap_internals::MemoryChunk* FromHeapObject(
      HeapObject object) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<MemoryChunk*>(object.ptr() & ~kPageAlignmentMask);
  }

  V8_INLINE bool IsMarking() const { return GetFlags() & kMarkingBit; }

  V8_INLINE bool InWritableSharedSpace() const {
    return GetFlags() & kInWritableSharedSpaceBit;
  }

  V8_INLINE bool InYoungGeneration() const {
    if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
    constexpr uintptr_t kYoungGenerationMask = kFromPageBit | kToPageBit;
    return GetFlags() & kYoungGenerationMask;
  }

  // Checks whether chunk is either in young gen or shared heap.
  V8_INLINE bool IsYoungOrSharedChunk() const {
    if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
    constexpr uintptr_t kYoungOrSharedChunkMask =
        kFromPageBit | kToPageBit | kInWritableSharedSpaceBit;
    return GetFlags() & kYoungOrSharedChunkMask;
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

  V8_INLINE bool InCodeSpace() const { return GetFlags() & kIsExecutableBit; }
};

inline void CombinedWriteBarrierInternal(HeapObject host, HeapObjectSlot slot,
                                         HeapObject value,
                                         WriteBarrierMode mode) {
  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);

  heap_internals::MemoryChunk* host_chunk =
      heap_internals::MemoryChunk::FromHeapObject(host);

  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);

  const bool pointers_from_here_are_interesting =
      !host_chunk->IsYoungOrSharedChunk();
  const bool is_marking = host_chunk->IsMarking();

  if (pointers_from_here_are_interesting &&
      value_chunk->IsYoungOrSharedChunk()) {
    // Generational or shared heap write barrier (old-to-new or old-to-shared).
    Heap_CombinedGenerationalAndSharedBarrierSlow(host, slot.address(), value);
  }

  // Marking barrier: mark value & record slots when marking is on.
  if (V8_UNLIKELY(is_marking)) {
    // CodePageHeaderModificationScope is not required because the only case
    // when a InstructionStream value is stored somewhere is during creation of
    // a new InstructionStream object which is then stored to
    // Code's code field and this case is already guarded by
    // CodePageMemoryModificationScope.
    WriteBarrier::MarkingSlow(host, HeapObjectSlot(slot), value);
  }
}

}  // namespace heap_internals

inline void WriteBarrierForCode(InstructionStream host, RelocInfo* rinfo,
                                Object value, WriteBarrierMode mode) {
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value.IsHeapObject()) return;
  WriteBarrierForCode(host, rinfo, HeapObject::cast(value));
}

inline void WriteBarrierForCode(InstructionStream host, RelocInfo* rinfo,
                                HeapObject value, WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
    SLOW_DCHECK(!WriteBarrier::IsRequired(host, value));
    return;
  }

  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);
  GenerationalBarrierForCode(rinfo, value);
  WriteBarrier::Shared(host, rinfo, value);
  WriteBarrier::Marking(host, rinfo, value);
}

inline void CombinedWriteBarrier(HeapObject host, ObjectSlot slot, Object value,
                                 WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
    SLOW_DCHECK(!WriteBarrier::IsRequired(host, value));
    return;
  }

  if (!value.IsHeapObject()) return;
  heap_internals::CombinedWriteBarrierInternal(host, HeapObjectSlot(slot),
                                               HeapObject::cast(value), mode);
}

inline void CombinedWriteBarrier(HeapObject host, MaybeObjectSlot slot,
                                 MaybeObject value, WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
    SLOW_DCHECK(!WriteBarrier::IsRequired(host, value));
    return;
  }

  HeapObject value_object;
  if (!value->GetHeapObject(&value_object)) return;
  heap_internals::CombinedWriteBarrierInternal(host, HeapObjectSlot(slot),
                                               value_object, mode);
}

inline void CombinedEphemeronWriteBarrier(EphemeronHashTable host,
                                          ObjectSlot slot, Object value,
                                          WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
    SLOW_DCHECK(!WriteBarrier::IsRequired(host, value));
    return;
  }

  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);
  if (!value.IsHeapObject()) return;

  heap_internals::MemoryChunk* host_chunk =
      heap_internals::MemoryChunk::FromHeapObject(host);

  HeapObject heap_object_value = HeapObject::cast(value);
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(heap_object_value);

  const bool pointers_from_here_are_interesting =
      !host_chunk->IsYoungOrSharedChunk();
  const bool is_marking = host_chunk->IsMarking();

  if (pointers_from_here_are_interesting &&
      value_chunk->IsYoungOrSharedChunk()) {
    Heap_CombinedGenerationalAndSharedEphemeronBarrierSlow(host, slot.address(),
                                                           heap_object_value);
  }

  // Marking barrier: mark value & record slots when marking is on.
  if (is_marking) {
    // Currently InstructionStream values are never stored in EphemeronTables.
    // If this ever changes then the CodePageHeaderModificationScope might be
    // required here.
    DCHECK(!IsCodeSpaceObject(heap_object_value));
    WriteBarrier::MarkingSlow(host, HeapObjectSlot(slot), heap_object_value);
  }
}

inline void GenerationalBarrierForCode(RelocInfo* rinfo, HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (!object_chunk->InYoungGeneration()) return;
  Heap_GenerationalBarrierForCodeSlow(rinfo, object);
}

inline WriteBarrierMode GetWriteBarrierModeForObject(
    HeapObject object, const DisallowGarbageCollection* promise) {
  if (v8_flags.disable_write_barriers) return SKIP_WRITE_BARRIER;
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
  if (v8_flags.single_generation) return false;
  if (object.IsSmi()) return false;
  return heap_internals::MemoryChunk::FromHeapObject(HeapObject::cast(object))
      ->InYoungGeneration();
}

inline bool IsReadOnlyHeapObject(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return ReadOnlyHeap::Contains(object);
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->InReadOnlySpace();
}

inline bool IsCodeSpaceObject(HeapObject object) {
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->InCodeSpace();
}

bool WriteBarrier::IsMarking(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return false;
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->IsMarking();
}

void WriteBarrier::Marking(HeapObject host, ObjectSlot slot, Object value) {
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value.IsHeapObject()) return;
  HeapObject value_heap_object = HeapObject::cast(value);
  // Currently this marking barrier is never used for InstructionStream values.
  // If this ever changes then the CodePageHeaderModificationScope might be
  // required here.
  DCHECK(!IsCodeSpaceObject(value_heap_object));
  Marking(host, HeapObjectSlot(slot), value_heap_object);
}

void WriteBarrier::Marking(HeapObject host, MaybeObjectSlot slot,
                           MaybeObject value) {
  HeapObject value_heap_object;
  if (!value->GetHeapObject(&value_heap_object)) return;
  // This barrier is called from generated code and from C++ code.
  // There must be no stores of InstructionStream values from generated code and
  // all stores of InstructionStream values in C++ must be handled by
  // CombinedWriteBarrierInternal().
  DCHECK(!IsCodeSpaceObject(value_heap_object));
  Marking(host, HeapObjectSlot(slot), value_heap_object);
}

void WriteBarrier::Marking(HeapObject host, HeapObjectSlot slot,
                           HeapObject value) {
  if (!IsMarking(host)) return;
  MarkingSlow(host, slot, value);
}

void WriteBarrier::Marking(InstructionStream host, RelocInfo* reloc_info,
                           HeapObject value) {
  if (!IsMarking(host)) return;
  MarkingSlow(host, reloc_info, value);
}

void WriteBarrier::Shared(InstructionStream host, RelocInfo* reloc_info,
                          HeapObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;

  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);
  if (!value_chunk->InWritableSharedSpace()) return;

  SharedSlow(reloc_info, value);
}

void WriteBarrier::Marking(JSArrayBuffer host,
                           ArrayBufferExtension* extension) {
  if (!extension || !IsMarking(host)) return;
  MarkingSlow(host, extension);
}

void WriteBarrier::Marking(DescriptorArray descriptor_array,
                           int number_of_own_descriptors) {
  if (!IsMarking(descriptor_array)) return;
  MarkingSlow(descriptor_array, number_of_own_descriptors);
}

// static
void WriteBarrier::MarkingFromGlobalHandle(Object value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  if (!value.IsHeapObject()) return;
  MarkingSlowFromGlobalHandle(HeapObject::cast(value));
}

// static
void WriteBarrier::CombinedBarrierFromInternalFields(JSObject host,
                                                     void* value) {
  CombinedBarrierFromInternalFields(host, 1, &value);
}

// static
void WriteBarrier::CombinedBarrierFromInternalFields(JSObject host, size_t argc,
                                                     void** values) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  if (V8_LIKELY(!IsMarking(host))) {
    GenerationalBarrierFromInternalFields(host, argc, values);
    return;
  }
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(host);
  if (marking_barrier->is_minor()) {
    // TODO(v8:13012): We do not currently mark Oilpan objects while MinorMC is
    // active. Once Oilpan uses a generational GC with incremental marking and
    // unified heap, this barrier will be needed again.
    return;
  }
  MarkingSlowFromInternalFields(marking_barrier->heap(), host);
}

// static
void WriteBarrier::GenerationalBarrierFromInternalFields(JSObject host,
                                                         void* value) {
  GenerationalBarrierFromInternalFields(host, 1, &value);
}

// static
void WriteBarrier::GenerationalBarrierFromInternalFields(JSObject host,
                                                         size_t argc,
                                                         void** values) {
  auto* memory_chunk = MemoryChunk::FromHeapObject(host);
  if (V8_LIKELY(memory_chunk->InYoungGeneration())) return;
  auto* cpp_heap = memory_chunk->heap()->cpp_heap();
  if (!cpp_heap) return;
  for (size_t i = 0; i < argc; ++i) {
    if (!values[i]) continue;
    v8::internal::CppHeap::From(cpp_heap)->RememberCrossHeapReferenceIfNeeded(
        host, values[i]);
  }
}

#ifdef ENABLE_SLOW_DCHECKS
// static
template <typename T>
bool WriteBarrier::IsRequired(HeapObject host, T value) {
  if (BasicMemoryChunk::FromHeapObject(host)->InYoungGeneration()) return false;
  if (value.IsSmi()) return false;
  if (value.IsCleared()) return false;
  HeapObject target = value.GetHeapObject();
  if (ReadOnlyHeap::Contains(target)) return false;
  return !IsImmortalImmovableHeapObject(target);
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
