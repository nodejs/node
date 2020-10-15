// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-visitor.h"

#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-state.h"

namespace cppgc {
namespace internal {

MarkingVisitor::MarkingVisitor(HeapBase& heap, MarkingState& marking_state)
    : marking_state_(marking_state) {}

void MarkingVisitor::Visit(const void* object, TraceDescriptor desc) {
  marking_state_.MarkAndPush(object, desc);
}

void MarkingVisitor::VisitWeak(const void* object, TraceDescriptor desc,
                               WeakCallback weak_callback,
                               const void* weak_member) {
  marking_state_.RegisterWeakReferenceIfNeeded(object, desc, weak_callback,
                                               weak_member);
}

void MarkingVisitor::VisitRoot(const void* object, TraceDescriptor desc) {
  Visit(object, desc);
}

void MarkingVisitor::VisitWeakRoot(const void* object, TraceDescriptor desc,
                                   WeakCallback weak_callback,
                                   const void* weak_root) {
  marking_state_.InvokeWeakRootsCallbackIfNeeded(object, desc, weak_callback,
                                                 weak_root);
}

void MarkingVisitor::RegisterWeakCallback(WeakCallback callback,
                                          const void* object) {
  marking_state_.RegisterWeakCallback(callback, object);
}

ConservativeMarkingVisitor::ConservativeMarkingVisitor(
    HeapBase& heap, MarkingState& marking_state, cppgc::Visitor& visitor)
    : ConservativeTracingVisitor(heap, *heap.page_backend(), visitor),
      marking_state_(marking_state) {}

void ConservativeMarkingVisitor::VisitConservatively(
    HeapObjectHeader& header, TraceConservativelyCallback callback) {
  marking_state_.MarkNoPush(header);
  callback(this, header);
  marking_state_.AccountMarkedBytes(header);
}

void ConservativeMarkingVisitor::VisitPointer(const void* address) {
  TraceConservativelyIfNeeded(address);
}

}  // namespace internal
}  // namespace cppgc
