// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

#include "src/heap/heap-write-barrier.h"
// Include the non-inl header before the rest of the headers.

#include <type_traits>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/heap/heap-layout-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/memory-chunk.h"
#include "src/objects/compressed-slots-inl.h"
#include "src/objects/cpp-heap-object-wrapper.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object-inl.h"

namespace v8::internal {

// static
void WriteBarrier::CombinedWriteBarrierInternalForStickyMarkbits(
    Tagged<HeapObject> host, HeapObjectSlot slot, Tagged<HeapObject> value,
    WriteBarrierMode mode) {
  DCHECK(v8_flags.sticky_mark_bits.value());

  MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);
  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);

  const bool is_marking = host_chunk->IsMarking();

  // TODO(333906585): Support shared barrier.
  if (!HeapLayout::InYoungGeneration(host_chunk, host) &&
      HeapLayout::InYoungGeneration(value_chunk, value)) {
    // Generational or shared heap write barrier (old-to-new or
    // old-to-shared).
    CombinedGenerationalAndSharedBarrierSlow(host, slot.address(), value);
  }
  // Marking barrier: mark value & record slots when marking is on.
  if (V8_UNLIKELY(is_marking)) {
    MarkingSlow(host, HeapObjectSlot(slot), value);
  }
}

// static
void WriteBarrier::CombinedWriteBarrierInternal(Tagged<HeapObject> host,
                                                HeapObjectSlot slot,
                                                Tagged<HeapObject> value,
                                                WriteBarrierMode mode) {
  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);

  if constexpr (v8_flags.sticky_mark_bits.value()) {
    CombinedWriteBarrierInternalForStickyMarkbits(host, slot, value, mode);
    return;
  }

  MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);
  // Fast path: Marking is off and the host objects is either in the young
  // generation or shared space, for which we don't require remembered sets.
  if (V8_LIKELY(!host_chunk->PointersFromHereAreInteresting())) {
    return;
  }

  // Either marking is on, or the host objects is in the old (non-shared)
  // generation for which we record remembered sets.

  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);
  // Old to old writes can bail out when marking is off.
  if (!value_chunk->PointersToHereAreInteresting()) {
    return;
  }

  CombinedWriteBarrierInternalSlow(host, host_chunk, slot, value, value_chunk);
}

// static
inline WriteBarrierMode WriteBarrier::ComputeWriteBarrierModeForObject(
    Tagged<HeapObject> object, const DisallowGarbageCollection& promise) {
  if (v8_flags.disable_write_barriers) {
    return SKIP_WRITE_BARRIER;
  }
  DCHECK(PageFlagsAreConsistent(object));
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  if (chunk->IsMarking()) {
    return UPDATE_WRITE_BARRIER;
  }
  if (HeapLayout::InYoungGeneration(chunk, object)) {
    return SKIP_WRITE_BARRIER_SCOPE;
  }
  return UPDATE_WRITE_BARRIER;
}

// static
inline WriteBarrierModeScope WriteBarrier::GetWriteBarrierModeForObject(
    Tagged<HeapObject> object, const DisallowGarbageCollection& promise) {
  WriteBarrierMode mode = ComputeWriteBarrierModeForObject(object, promise);
  return WriteBarrierModeScope(object, mode);
}

// static
void WriteBarrier::ForRelocInfo(Tagged<InstructionStream> host,
                                RelocInfo* rinfo, Tagged<HeapObject> value,
                                WriteBarrierMode mode) {
  if (IsSkipWriteBarrierMode(mode)) {
#if V8_VERIFY_WRITE_BARRIERS
    VerifySkipWriteBarrier(host, value, mode);
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }

  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);
  GenerationalForRelocInfo(host, rinfo, value);
  SharedForRelocInfo(host, rinfo, value);
  MarkingForRelocInfo(host, rinfo, value);
}

// static
template <typename T>
void WriteBarrier::ForValue(Tagged<HeapObject> host, MaybeObjectSlot slot,
                            Tagged<T> value, WriteBarrierMode mode) {
  if (IsSkipWriteBarrierMode(mode)) {
#if V8_VERIFY_WRITE_BARRIERS
    VerifySkipWriteBarrier(host, value, mode);
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }
  Tagged<HeapObject> value_object;
  if (!value.GetHeapObject(&value_object)) {
    return;
  }
  CombinedWriteBarrierInternal(host, HeapObjectSlot(slot), value_object, mode);
}

// static
template <typename T>
void WriteBarrier::ForValue(HeapObjectLayout* host, TaggedMemberBase* slot,
                            Tagged<T> value, WriteBarrierMode mode) {
  if (IsSkipWriteBarrierMode(mode)) {
#if V8_VERIFY_WRITE_BARRIERS
    VerifySkipWriteBarrier(host, value, mode);
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }
  Tagged<HeapObject> value_object;
  if (!value.GetHeapObject(&value_object)) {
    return;
  }
  CombinedWriteBarrierInternal(Tagged(host), HeapObjectSlot(ObjectSlot(slot)),
                               value_object, mode);
}

#if V8_VERIFY_WRITE_BARRIERS
// static
template <typename T>
void WriteBarrier::VerifySkipWriteBarrier(Tagged<HeapObject> host,
                                          Tagged<T> value,
                                          WriteBarrierMode mode) {
  if (v8_flags.verify_write_barriers) {
    if (mode == SKIP_WRITE_BARRIER) {
      CHECK(!WriteBarrier::IsRequired(host, value));
    } else if (mode == SKIP_WRITE_BARRIER_FOR_GC) {
      CHECK(Isolate::Current()->heap()->IsInGC());
      CHECK(Isolate::Current()->heap()->tracer()->IsInAtomicPause());
    } else if (mode == UNSAFE_SKIP_WRITE_BARRIER) {
      // C++ write barriers should not need UNSAFE_SKIP_WRITE_BARRIER.
      UNREACHABLE();
    } else {
      CHECK_EQ(mode, SKIP_WRITE_BARRIER_SCOPE);
      CHECK_EQ(LocalHeap::Current()->CurrentObjectForWriteBarrierMode(),
               host.address());
    }
  }
}
#endif  // V8_VERIFY_WRITE_BARRIERS

//   static
void WriteBarrier::ForEphemeronHashTable(Tagged<EphemeronHashTable> host,
                                         ObjectSlot slot, Tagged<Object> value,
                                         WriteBarrierMode mode) {
  if (IsSkipWriteBarrierMode(mode)) {
#if V8_VERIFY_WRITE_BARRIERS
    VerifySkipWriteBarrier(host, value, mode);
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }

  DCHECK_EQ(mode, UPDATE_WRITE_BARRIER);
  if (!value.IsHeapObject()) return;

  MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);

  Tagged<HeapObject> heap_object_value = Cast<HeapObject>(value);
  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(heap_object_value);
  DCHECK(!value_chunk->Metadata()->is_writable_shared());

  const bool pointers_from_here_are_interesting =
      !host_chunk->IsYoungOrSharedChunk();
  const bool is_marking = host_chunk->IsMarking();

  if (is_marking) {
    // Marking barrier: mark value & record slots when marking is on.
    MarkingSlow<RecordYoungSlot::kYes>(host, HeapObjectSlot(slot),
                                       heap_object_value);

    // Only trigger the generation barrier while marking is off. That way we
    // keep the remembered set empty after incremental marking started.
  } else if (pointers_from_here_are_interesting &&
             value_chunk->IsYoungOrSharedChunk()) {
    CombinedGenerationalAndSharedEphemeronBarrierSlow(host, slot.address(),
                                                      heap_object_value);
  }
}

// static
void WriteBarrier::ForExternalPointer(Tagged<HeapObject> host,
                                      ExternalPointerSlot slot,
                                      WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
    SLOW_DCHECK(HeapLayout::InYoungGeneration(host));
    return;
  }
  Marking(host, slot);
}

// static
void WriteBarrier::ForIndirectPointer(Tagged<HeapObject> host,
                                      IndirectPointerSlot slot,
                                      Tagged<HeapObject> value,
                                      WriteBarrierMode mode) {
  // Indirect pointers are only used when the sandbox is enabled.
#ifdef V8_ENABLE_SANDBOX
  if (mode == SKIP_WRITE_BARRIER) {
#if V8_VERIFY_WRITE_BARRIERS
    if (v8_flags.verify_write_barriers) {
      CHECK(!WriteBarrier::IsRequired(host, value));
    }
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }
  // Objects referenced via indirect pointers are currently never allocated in
  // the young generation.
  if (!v8_flags.sticky_mark_bits) {
    DCHECK(!MemoryChunk::FromHeapObject(value)->InYoungGeneration());
  }
  Marking(host, slot);
#else
  UNREACHABLE();
#endif
}

// static
template <typename T, IndirectPointerTag kTag>
void WriteBarrier::ForIndirectPointer(HeapObjectLayout* host,
                                      TrustedPointerMember<T, kTag>* slot,
                                      Tagged<T> value, WriteBarrierMode mode) {
  // Indirect pointers are only used when the sandbox is enabled.
#ifdef V8_ENABLE_SANDBOX
  // TODO(leszeks): Avoid the cast to Address here, pass a pointer to the actual
  // handle field.
  ForIndirectPointer(Tagged(host),
                     IndirectPointerSlot(reinterpret_cast<Address>(slot), kTag),
                     value, mode);
#else
  UNREACHABLE();
#endif
}

// static
void WriteBarrier::ForJSDispatchHandle(Tagged<HeapObject> host,
                                       JSDispatchHandle handle,
                                       WriteBarrierMode mode) {
#if V8_VERIFY_WRITE_BARRIERS
  if (v8_flags.verify_write_barriers) {
    CHECK(WriteBarrier::VerifyDispatchHandleMarkingState(host, handle, mode));
  }
#endif  // V8_VERIFY_WRITE_BARRIERS
  if (mode == SKIP_WRITE_BARRIER) {
    return;
  }
  Marking(host, handle);
}

// static
void WriteBarrier::ForProtectedPointer(Tagged<TrustedObject> host,
                                       ProtectedPointerSlot slot,
                                       Tagged<TrustedObject> value,
                                       WriteBarrierMode mode) {
  if (mode == SKIP_WRITE_BARRIER) {
#if V8_VERIFY_WRITE_BARRIERS
    if (v8_flags.verify_write_barriers) {
      CHECK(!WriteBarrier::IsRequired(host, value));
    }
#endif  // V8_VERIFY_WRITE_BARRIERS
    return;
  }
  // Protected pointers are only used within trusted and shared trusted space.
  DCHECK_IMPLIES(!v8_flags.sticky_mark_bits,
                 !MemoryChunk::FromHeapObject(value)->InYoungGeneration());
  if (MemoryChunk::FromHeapObject(value)->InWritableSharedSpace()) {
    SharedSlow(host, slot, value);
  }
  Marking(host, slot, value);
}

// static
void WriteBarrier::GenerationalForRelocInfo(Tagged<InstructionStream> host,
                                            RelocInfo* rinfo,
                                            Tagged<HeapObject> object) {
  if (!HeapLayout::InYoungGeneration(object)) {
    return;
  }
  GenerationalBarrierForCodeSlow(host, rinfo, object);
}

// static
bool WriteBarrier::IsMarking(Tagged<HeapObject> object) {
  return MemoryChunk::FromHeapObject(object)->IsMarking();
}

void WriteBarrier::MarkingForTesting(Tagged<HeapObject> host, ObjectSlot slot,
                                     Tagged<Object> value) {
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value.IsHeapObject()) {
    return;
  }
  Tagged<HeapObject> value_heap_object = Cast<HeapObject>(value);
  Marking(host, HeapObjectSlot(slot), value_heap_object);
}

void WriteBarrier::Marking(Tagged<HeapObject> host, MaybeObjectSlot slot,
                           Tagged<MaybeObject> value) {
  Tagged<HeapObject> value_heap_object;
  if (!value.GetHeapObject(&value_heap_object)) {
    return;
  }
  // This barrier is called from generated code and from C++ code.
  // There must be no stores of InstructionStream values from generated code and
  // all stores of InstructionStream values in C++ must be handled by
  // CombinedWriteBarrierInternal().
  DCHECK(!TrustedHeapLayout::InCodeSpace(value_heap_object));
  Marking(host, HeapObjectSlot(slot), value_heap_object);
}

void WriteBarrier::Marking(Tagged<HeapObject> host, HeapObjectSlot slot,
                           Tagged<HeapObject> value) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, slot, value);
}

void WriteBarrier::MarkingForRelocInfo(Tagged<InstructionStream> host,
                                       RelocInfo* reloc_info,
                                       Tagged<HeapObject> value) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, reloc_info, value);
}

void WriteBarrier::SharedForRelocInfo(Tagged<InstructionStream> host,
                                      RelocInfo* reloc_info,
                                      Tagged<HeapObject> value) {
  MemoryChunk* value_chunk = MemoryChunk::FromHeapObject(value);
  if (!value_chunk->InWritableSharedSpace()) {
    return;
  }
  SharedSlow(host, reloc_info, value);
}

void WriteBarrier::ForArrayBufferExtension(Tagged<JSArrayBuffer> host,
                                           ArrayBufferExtension* extension) {
  if (!extension || V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, extension);
}

void WriteBarrier::ForDescriptorArray(Tagged<DescriptorArray> descriptor_array,
                                      int number_of_own_descriptors) {
  if (V8_LIKELY(!IsMarking(descriptor_array))) {
    return;
  }
  MarkingSlow(descriptor_array, number_of_own_descriptors);
}

void WriteBarrier::Marking(Tagged<HeapObject> host, ExternalPointerSlot slot) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, slot);
}

void WriteBarrier::Marking(Tagged<HeapObject> host, IndirectPointerSlot slot) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, slot);
}

void WriteBarrier::Marking(Tagged<TrustedObject> host,
                           ProtectedPointerSlot slot,
                           Tagged<TrustedObject> value) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, slot, value);
}

void WriteBarrier::Marking(Tagged<HeapObject> host, JSDispatchHandle handle) {
  if (V8_LIKELY(!IsMarking(host))) {
    return;
  }
  MarkingSlow(host, handle);
}

// static
void WriteBarrier::MarkingFromTracedHandle(Tagged<Object> value) {
  if (!value.IsHeapObject()) {
    return;
  }
  MarkingSlowFromTracedHandle(Cast<HeapObject>(value));
}

// static
void WriteBarrier::ForCppHeapPointer(Tagged<CppHeapPointerWrapperObjectT> host,
                                     CppHeapPointerSlot slot, void* value) {
  // Note: this is currently a combined barrier for marking both the
  // CppHeapPointerTable entry and the referenced object.

  if (V8_LIKELY(!IsMarking(host))) {
#if defined(CPPGC_YOUNG_GENERATION)
    // There is no young-gen CppHeapPointerTable space so we should not mark
    // the table entry in this case.
    if (value) {
      GenerationalBarrierForCppHeapPointer(host, value);
    }
#endif
    return;
  }
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(host);
  if (marking_barrier->is_minor()) {
    // TODO(v8:13012): We do not currently mark Oilpan objects while MinorMS is
    // active. Once Oilpan uses a generational GC with incremental marking and
    // unified heap, this barrier will be needed again.
    return;
  }

  MarkingSlowFromCppHeapWrappable(marking_barrier->heap(), host, slot, value);
}

// static
void WriteBarrier::GenerationalBarrierForCppHeapPointer(
    Tagged<CppHeapPointerWrapperObjectT> host, void* value) {
  if (!value) {
    return;
  }
  auto* memory_chunk = MemoryChunk::FromHeapObject(host);
  if (V8_LIKELY(HeapLayout::InYoungGeneration(memory_chunk, host))) {
    return;
  }
  auto* cpp_heap = memory_chunk->Metadata()->heap()->cpp_heap();
  v8::internal::CppHeap::From(cpp_heap)->RememberCrossHeapReferenceIfNeeded(
      host, value);
}

#ifdef V8_VERIFY_WRITE_BARRIERS
// static
template <typename T>
bool WriteBarrier::IsRequired(const HeapObjectLayout* host, T value) {
  return IsRequiredCommon(host, value);
}

// static
template <typename T>
bool WriteBarrier::IsRequired(Tagged<HeapObject> host, T value) {
  return IsRequiredCommon(host, value);
}

// static
template <typename HostType, typename ValueType>
bool WriteBarrier::IsRequiredCommon(HostType host, ValueType value) {
  if (IsSmi(value)) {
    return false;
  }
  if (value.IsCleared()) {
    return false;
  }
  Tagged<HeapObject> target = value.GetHeapObject();
  if (ReadOnlyHeap::Contains(target)) {
    return false;
  }
  if constexpr (std::is_same_v<HostType, const HeapObjectLayout*>) {
    return !IsMostRecentYoungAllocation(host->address());
  } else {
    return !IsMostRecentYoungAllocation(host.address());
  }
}
#endif  // V8_VERIFY_WRITE_BARRIERS

}  // namespace v8::internal

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
