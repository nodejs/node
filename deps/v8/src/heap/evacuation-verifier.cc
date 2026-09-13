// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/evacuation-verifier.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/reloc-info.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/visit-object.h"
#include "src/objects/map-inl.h"

namespace v8::internal {

#ifdef VERIFY_HEAP

EvacuationVerifier::EvacuationVerifier(Heap* heap)
    : ObjectVisitorWithCageBases(heap), heap_(heap) {}

void EvacuationVerifier::Run() {
  CHECK(!heap_->sweeping_in_progress());
  VerifyRoots();
  VerifyEvacuation(heap_->new_space());
  VerifyEvacuation(heap_->old_space());
  VerifyEvacuation(heap_->code_space());
  if (heap_->shared_space()) VerifyEvacuation(heap_->shared_space());
}

void EvacuationVerifier::VisitPointers(Tagged<HeapObject> host,
                                       ObjectSlot start, ObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitPointers(Tagged<HeapObject> host,
                                       MaybeObjectSlot start,
                                       MaybeObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitInstructionStreamPointer(
    Tagged<Code> host, InstructionStreamSlot slot) {
  Tagged<Object> maybe_code = slot.load(code_cage_base());
  Tagged<HeapObject> code;
  // The slot might contain smi during Code creation, so skip it.
  if (maybe_code.GetHeapObject(&code)) {
    VerifyHeapObjectImpl(code);
  }
}

void EvacuationVerifier::VisitRootPointers(Root root, const char* description,
                                           FullObjectSlot start,
                                           FullObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void EvacuationVerifier::VisitMapPointer(Tagged<HeapObject> object) {
  VerifyHeapObjectImpl(object->map());
}

void EvacuationVerifier::VisitCodeTarget(Tagged<InstructionStream> host,
                                         RelocInfo* rinfo) {
  Tagged<InstructionStream> target =
      InstructionStream::FromTargetAddress(rinfo->target_address());
  VerifyHeapObjectImpl(target);
}

void EvacuationVerifier::VisitEmbeddedPointer(Tagged<InstructionStream> host,
                                              RelocInfo* rinfo) {
  VerifyHeapObjectImpl(rinfo->target_object());
}

void EvacuationVerifier::VerifyRoots() {
  heap_->IterateRootsIncludingClients(
      this,
      base::EnumSet<SkipRoot>{SkipRoot::kWeak, SkipRoot::kConservativeStack});
}

void EvacuationVerifier::VerifyEvacuationOnPage(Address start, Address end) {
  Address current = start;
  while (current < end) {
    Tagged<HeapObject> object = HeapObject::FromAddress(current);
    if (!IsFreeSpaceOrFiller(object)) {
      VisitObject(heap_->isolate(), object, this);
    }
    current += ALIGN_TO_ALLOCATION_ALIGNMENT(object->Size());
  }
}

void EvacuationVerifier::VerifyEvacuation(NewSpace* space) {
  if (!space) return;

  if (v8_flags.minor_ms) {
    VerifyEvacuation(PagedNewSpace::From(space)->paged_space());
    return;
  }

  for (NormalPage* p : *space) {
    VerifyEvacuationOnPage(p->area_start(), p->area_end());
  }
}

void EvacuationVerifier::VerifyEvacuation(PagedSpaceBase* space) {
  for (NormalPage* p : *space) {
    if (p->Chunk()->IsEvacuationCandidate()) continue;
    VerifyEvacuationOnPage(p->area_start(), p->area_end());
  }
}

void EvacuationVerifier::VerifyHeapObjectImpl(Tagged<HeapObject> heap_object) {
  if (!ShouldVerifyObject(heap_object)) return;
  CHECK_IMPLIES(
      !v8_flags.sticky_mark_bits && HeapLayout::InYoungGeneration(heap_object),
      Heap::InToPage(heap_object));
  CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(heap_object));
}

bool EvacuationVerifier::ShouldVerifyObject(Tagged<HeapObject> heap_object) {
  const bool in_shared_heap = HeapLayout::InWritableSharedSpace(heap_object);
  return heap_->isolate()->is_shared_space_isolate() ? in_shared_heap
                                                     : !in_shared_heap;
}

template <typename TSlot>
void EvacuationVerifier::VerifyPointersImpl(TSlot start, TSlot end) {
  for (TSlot current = start; current < end; ++current) {
    typename TSlot::TObject object = current.load(cage_base());
#ifdef V8_ENABLE_DIRECT_HANDLE
    if (object.ptr() == kTaggedNullAddress) continue;
#endif
    Tagged<HeapObject> heap_object;
    if (object.GetHeapObjectIfStrong(&heap_object)) {
      VerifyHeapObjectImpl(heap_object);
    }
  }
}

#endif  // VERIFY_HEAP

}  // namespace v8::internal
