// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-verifier.h"

#include "src/base/logging.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap.h"

namespace cppgc {
namespace internal {

MarkingVerifier::MarkingVerifier(HeapBase& heap,
                                 Heap::Config::StackState stack_state)
    : cppgc::Visitor(VisitorFactory::CreateKey()),
      ConservativeTracingVisitor(heap, *heap.page_backend(), *this) {
  Traverse(&heap.raw_heap());
  if (stack_state == Heap::Config::StackState::kMayContainHeapPointers)
    heap.stack()->IteratePointers(this);
}

void MarkingVerifier::Visit(const void* object, TraceDescriptor desc) {
  VerifyChild(desc.base_object_payload);
}

void MarkingVerifier::VisitWeak(const void* object, TraceDescriptor desc,
                                WeakCallback, const void*) {
  // Weak objects should have been cleared at this point. As a consequence, all
  // objects found through weak references have to point to live objects at this
  // point.
  VerifyChild(desc.base_object_payload);
}

void MarkingVerifier::VerifyChild(const void* base_object_payload) {
  const HeapObjectHeader& child_header =
      HeapObjectHeader::FromPayload(base_object_payload);

  CHECK(child_header.IsMarked());
}

void MarkingVerifier::VisitConservatively(
    HeapObjectHeader& header, TraceConservativelyCallback callback) {
  CHECK(header.IsMarked());
}

void MarkingVerifier::VisitPointer(const void* address) {
  TraceConservativelyIfNeeded(address);
}

bool MarkingVerifier::VisitHeapObjectHeader(HeapObjectHeader* header) {
  // Verify only non-free marked objects.
  if (!header->IsMarked()) return true;

  DCHECK(!header->IsFree());

  GlobalGCInfoTable::GCInfoFromIndex(header->GetGCInfoIndex())
      .trace(this, header->Payload());
  return true;
}

}  // namespace internal
}  // namespace cppgc
