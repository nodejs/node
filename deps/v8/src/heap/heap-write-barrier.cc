// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-write-barrier.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/remembered-set.h"
#include "src/objects/code-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots-inl.h"

namespace v8 {
namespace internal {

namespace {
thread_local MarkingBarrier* current_marking_barrier = nullptr;
}  // namespace

MarkingBarrier* WriteBarrier::CurrentMarkingBarrier(
    HeapObject verification_candidate) {
  MarkingBarrier* marking_barrier = current_marking_barrier;
  DCHECK_NOT_NULL(marking_barrier);
#if DEBUG
  if (!verification_candidate.is_null() &&
      !verification_candidate.InAnySharedSpace()) {
    Heap* host_heap =
        MemoryChunk::FromHeapObject(verification_candidate)->heap();
    LocalHeap* local_heap = LocalHeap::Current();
    if (!local_heap) local_heap = host_heap->main_thread_local_heap();
    DCHECK_EQ(marking_barrier, local_heap->marking_barrier());
  }
#endif  // DEBUG
  return marking_barrier;
}

MarkingBarrier* WriteBarrier::SetForThread(MarkingBarrier* marking_barrier) {
  MarkingBarrier* existing = current_marking_barrier;
  current_marking_barrier = marking_barrier;
  return existing;
}

void WriteBarrier::MarkingSlow(HeapObject host, HeapObjectSlot slot,
                               HeapObject value) {
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(host);
  marking_barrier->Write(host, slot, value);
}

// static
void WriteBarrier::MarkingSlowFromGlobalHandle(HeapObject value) {
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(value);
  marking_barrier->WriteWithoutHost(value);
}

// static
void WriteBarrier::MarkingSlowFromInternalFields(Heap* heap, JSObject host) {
  if (auto* cpp_heap = heap->cpp_heap()) {
    CppHeap::From(cpp_heap)->WriteBarrier(host);
  }
}

void WriteBarrier::MarkingSlow(InstructionStream host, RelocInfo* reloc_info,
                               HeapObject value) {
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(host);
  marking_barrier->Write(host, reloc_info, value);
}

void WriteBarrier::SharedSlow(RelocInfo* reloc_info, HeapObject value) {
  MarkCompactCollector::RecordRelocSlotInfo info =
      MarkCompactCollector::ProcessRelocInfo(reloc_info, value);

  base::MutexGuard write_scope(info.memory_chunk->mutex());
  RememberedSet<OLD_TO_SHARED>::InsertTyped(info.memory_chunk, info.slot_type,
                                            info.offset);
}

void WriteBarrier::MarkingSlow(JSArrayBuffer host,
                               ArrayBufferExtension* extension) {
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(host);
  marking_barrier->Write(host, extension);
}

void WriteBarrier::MarkingSlow(DescriptorArray descriptor_array,
                               int number_of_own_descriptors) {
  MarkingBarrier* marking_barrier = CurrentMarkingBarrier(descriptor_array);
  marking_barrier->Write(descriptor_array, number_of_own_descriptors);
}

int WriteBarrier::MarkingFromCode(Address raw_host, Address raw_slot) {
  HeapObject host = HeapObject::cast(Object(raw_host));
  MaybeObjectSlot slot(raw_slot);
  Address value = (*slot).ptr();

#ifdef V8_MAP_PACKING
  if (slot.address() == host.address()) {
    // Clear metadata bits and fix object tag.
    value = (value & ~Internals::kMapWordMetadataMask &
             ~Internals::kMapWordXorMask) |
            (uint64_t)kHeapObjectTag;
  }
#endif

#if DEBUG
  Heap* heap = MemoryChunk::FromHeapObject(host)->heap();
  DCHECK(heap->incremental_marking()->IsMarking());

  // We will only reach local objects here while incremental marking in the
  // current isolate is enabled. However, we might still reach objects in the
  // shared space but only from the shared space isolate (= the main isolate).
  MarkingBarrier* barrier = CurrentMarkingBarrier(host);
  DCHECK_IMPLIES(host.InWritableSharedSpace(),
                 barrier->heap()->isolate()->is_shared_space_isolate());
  barrier->AssertMarkingIsActivated();
#endif  // DEBUG

  WriteBarrier::Marking(host, slot, MaybeObject(value));
  // Called by WriteBarrierCodeStubAssembler, which doesn't accept void type
  return 0;
}

int WriteBarrier::SharedMarkingFromCode(Address raw_host, Address raw_slot) {
  HeapObject host = HeapObject::cast(Object(raw_host));
  MaybeObjectSlot slot(raw_slot);
  Address raw_value = (*slot).ptr();
  MaybeObject value(raw_value);

  DCHECK(host.InWritableSharedSpace());

#if DEBUG
  Heap* heap = MemoryChunk::FromHeapObject(host)->heap();
  DCHECK(heap->incremental_marking()->IsMarking());
  Isolate* isolate = heap->isolate();
  DCHECK(isolate->is_shared_space_isolate());

  // The shared marking barrier will only be reached from client isolates (=
  // worker isolates).
  MarkingBarrier* barrier = CurrentMarkingBarrier(host);
  DCHECK(!barrier->heap()->isolate()->is_shared_space_isolate());
  barrier->AssertSharedMarkingIsActivated();
#endif  // DEBUG

  WriteBarrier::Marking(host, slot, MaybeObject(value));

  // Called by WriteBarrierCodeStubAssembler, which doesn't accept void type
  return 0;
}

int WriteBarrier::SharedFromCode(Address raw_host, Address raw_slot) {
  HeapObject host = HeapObject::cast(Object(raw_host));

  if (!host.InWritableSharedSpace()) {
    Heap::SharedHeapBarrierSlow(host, raw_slot);
  }

  // Called by WriteBarrierCodeStubAssembler, which doesn't accept void type
  return 0;
}

#ifdef ENABLE_SLOW_DCHECKS
bool WriteBarrier::IsImmortalImmovableHeapObject(HeapObject object) {
  BasicMemoryChunk* basic_chunk = BasicMemoryChunk::FromHeapObject(object);
  // All objects in readonly space are immortal and immovable.
  return basic_chunk->InReadOnlySpace();
}
#endif

}  // namespace internal
}  // namespace v8
