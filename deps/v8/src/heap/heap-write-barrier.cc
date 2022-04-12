// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-write-barrier.h"

#include "src/heap/embedder-tracing.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/marking-barrier.h"
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

MarkingBarrier* WriteBarrier::CurrentMarkingBarrier(Heap* heap) {
  return current_marking_barrier ? current_marking_barrier
                                 : heap->marking_barrier();
}

void WriteBarrier::SetForThread(MarkingBarrier* marking_barrier) {
  DCHECK_NULL(current_marking_barrier);
  current_marking_barrier = marking_barrier;
}

void WriteBarrier::ClearForThread(MarkingBarrier* marking_barrier) {
  DCHECK_EQ(current_marking_barrier, marking_barrier);
  current_marking_barrier = nullptr;
}

void WriteBarrier::MarkingSlow(Heap* heap, HeapObject host, HeapObjectSlot slot,
                               HeapObject value) {
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, slot, value);
}

// static
void WriteBarrier::MarkingSlowFromGlobalHandle(Heap* heap, HeapObject value) {
  heap->marking_barrier()->WriteWithoutHost(value);
}

// static
void WriteBarrier::MarkingSlowFromInternalFields(Heap* heap, JSObject host) {
  auto* local_embedder_heap_tracer = heap->local_embedder_heap_tracer();
  if (!local_embedder_heap_tracer->InUse()) return;

  local_embedder_heap_tracer->EmbedderWriteBarrier(heap, host);
}

void WriteBarrier::MarkingSlow(Heap* heap, Code host, RelocInfo* reloc_info,
                               HeapObject value) {
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, reloc_info, value);
}

void WriteBarrier::MarkingSlow(Heap* heap, JSArrayBuffer host,
                               ArrayBufferExtension* extension) {
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, extension);
}

void WriteBarrier::MarkingSlow(Heap* heap, DescriptorArray descriptor_array,
                               int number_of_own_descriptors) {
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
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
  WriteBarrier::Marking(host, slot, MaybeObject(value));
  // Called by WriteBarrierCodeStubAssembler, which doesnt accept void type
  return 0;
}

#ifdef ENABLE_SLOW_DCHECKS
bool WriteBarrier::IsImmortalImmovableHeapObject(HeapObject object) {
  BasicMemoryChunk* basic_chunk = BasicMemoryChunk::FromHeapObject(object);
  // All objects in readonly space are immortal and immovable.
  if (basic_chunk->InReadOnlySpace()) return true;
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  // There are also objects in "regular" spaces which are immortal and
  // immovable. Objects on a page that can get compacted are movable and can be
  // filtered out.
  if (!chunk->IsFlagSet(MemoryChunk::NEVER_EVACUATE)) return false;
  // Now we know the object is immovable, check whether it is also immortal.
  // Builtins are roots and therefore always kept alive by the GC.
  return object.IsCode() && Code::cast(object).is_builtin();
}
#endif

}  // namespace internal
}  // namespace v8
