// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-visitor.h"

#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/internal/accessors.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap.h"

namespace cppgc {
namespace internal {

// static
bool MarkingVisitor::IsInConstruction(const HeapObjectHeader& header) {
  return header.IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>();
}

MarkingVisitor::MarkingVisitor(Marker* marking_handler, int task_id)
    : marker_(marking_handler),
      marking_worklist_(marking_handler->marking_worklist(), task_id),
      not_fully_constructed_worklist_(
          marking_handler->not_fully_constructed_worklist(), task_id),
      weak_callback_worklist_(marking_handler->weak_callback_worklist(),
                              task_id) {}

void MarkingVisitor::AccountMarkedBytes(const HeapObjectHeader& header) {
  marked_bytes_ +=
      header.IsLargeObject()
          ? reinterpret_cast<const LargePage*>(BasePage::FromPayload(&header))
                ->PayloadSize()
          : header.GetSize();
}

void MarkingVisitor::Visit(const void* object, TraceDescriptor desc) {
  DCHECK_NOT_NULL(object);
  if (desc.base_object_payload ==
      cppgc::GarbageCollectedMixin::kNotFullyConstructedObject) {
    // This means that the objects are not-yet-fully-constructed. See comments
    // on GarbageCollectedMixin for how those objects are handled.
    not_fully_constructed_worklist_.Push(object);
    return;
  }
  MarkHeader(&HeapObjectHeader::FromPayload(
                 const_cast<void*>(desc.base_object_payload)),
             desc);
}

void MarkingVisitor::VisitWeak(const void* object, TraceDescriptor desc,
                               WeakCallback weak_callback,
                               const void* weak_member) {
  // Filter out already marked values. The write barrier for WeakMember
  // ensures that any newly set value after this point is kept alive and does
  // not require the callback.
  if (desc.base_object_payload !=
          cppgc::GarbageCollectedMixin::kNotFullyConstructedObject &&
      HeapObjectHeader::FromPayload(desc.base_object_payload)
          .IsMarked<HeapObjectHeader::AccessMode::kAtomic>())
    return;
  RegisterWeakCallback(weak_callback, weak_member);
}

void MarkingVisitor::VisitRoot(const void* object, TraceDescriptor desc) {
  Visit(object, desc);
}

void MarkingVisitor::VisitWeakRoot(const void* object, TraceDescriptor desc,
                                   WeakCallback weak_callback,
                                   const void* weak_root) {
  if (desc.base_object_payload ==
      cppgc::GarbageCollectedMixin::kNotFullyConstructedObject) {
    // This method is only called at the end of marking. If the object is in
    // construction, then it should be reachable from the stack.
    return;
  }
  // Since weak roots arev only traced at the end of marking, we can execute
  // the callback instead of registering it.
  weak_callback(LivenessBrokerFactory::Create(), weak_root);
}

void MarkingVisitor::MarkHeader(HeapObjectHeader* header,
                                TraceDescriptor desc) {
  DCHECK(header);
  DCHECK_NOT_NULL(desc.callback);

  if (IsInConstruction(*header)) {
    not_fully_constructed_worklist_.Push(header->Payload());
  } else if (MarkHeaderNoTracing(header)) {
    marking_worklist_.Push(desc);
  }
}

bool MarkingVisitor::MarkHeaderNoTracing(HeapObjectHeader* header) {
  DCHECK(header);
  // A GC should only mark the objects that belong in its heap.
  DCHECK_EQ(marker_->heap(), BasePage::FromPayload(header)->heap());
  // Never mark free space objects. This would e.g. hint to marking a promptly
  // freed backing store.
  DCHECK(!header->IsFree());

  return header->TryMarkAtomic();
}

void MarkingVisitor::RegisterWeakCallback(WeakCallback callback,
                                          const void* object) {
  weak_callback_worklist_.Push({callback, object});
}

void MarkingVisitor::FlushWorklists() {
  marking_worklist_.FlushToGlobal();
  not_fully_constructed_worklist_.FlushToGlobal();
  weak_callback_worklist_.FlushToGlobal();
}

void MarkingVisitor::DynamicallyMarkAddress(ConstAddress address) {
  for (auto* header : marker_->heap()->objects()) {
    if (address >= header->Payload() &&
        address < (header->Payload() + header->GetSize())) {
      header->TryMarkAtomic();
    }
  }
  // TODO(chromium:1056170): Implement dynamically getting HeapObjectHeader
  // for handling previously_not_fully_constructed objects. Requires object
  // start bitmap.
}

void MarkingVisitor::VisitPointer(const void* address) {
  for (auto* header : marker_->heap()->objects()) {
    if (address >= header->Payload() &&
        address < (header->Payload() + header->GetSize())) {
      header->TryMarkAtomic();
    }
  }
  // TODO(chromium:1056170): Implement proper conservative scanning for
  // on-stack objects. Requires page bloom filter.
}

MutatorThreadMarkingVisitor::MutatorThreadMarkingVisitor(Marker* marker)
    : MarkingVisitor(marker, Marker::kMutatorThreadId) {}

}  // namespace internal
}  // namespace cppgc
