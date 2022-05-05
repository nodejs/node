// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/unified-heap-marking-visitor.h"

#include "src/heap/cppgc-js/unified-heap-marking-state-inl.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/visitor.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

UnifiedHeapMarkingVisitorBase::UnifiedHeapMarkingVisitorBase(
    HeapBase& heap, cppgc::internal::BasicMarkingState& marking_state,
    UnifiedHeapMarkingState& unified_heap_marking_state)
    : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
      marking_state_(marking_state),
      unified_heap_marking_state_(unified_heap_marking_state) {}

void UnifiedHeapMarkingVisitorBase::Visit(const void* object,
                                          TraceDescriptor desc) {
  marking_state_.MarkAndPush(object, desc);
}

void UnifiedHeapMarkingVisitorBase::VisitWeak(const void* object,
                                              TraceDescriptor desc,
                                              WeakCallback weak_callback,
                                              const void* weak_member) {
  marking_state_.RegisterWeakReferenceIfNeeded(object, desc, weak_callback,
                                               weak_member);
}

void UnifiedHeapMarkingVisitorBase::VisitEphemeron(const void* key,
                                                   const void* value,
                                                   TraceDescriptor value_desc) {
  marking_state_.ProcessEphemeron(key, value, value_desc, *this);
}

void UnifiedHeapMarkingVisitorBase::VisitWeakContainer(
    const void* self, TraceDescriptor strong_desc, TraceDescriptor weak_desc,
    WeakCallback callback, const void* data) {
  marking_state_.ProcessWeakContainer(self, weak_desc, callback, data);
}

void UnifiedHeapMarkingVisitorBase::RegisterWeakCallback(WeakCallback callback,
                                                         const void* object) {
  marking_state_.RegisterWeakCallback(callback, object);
}

void UnifiedHeapMarkingVisitorBase::HandleMovableReference(const void** slot) {
  marking_state_.RegisterMovableReference(slot);
}

void UnifiedHeapMarkingVisitorBase::Visit(const TracedReferenceBase& ref) {
  unified_heap_marking_state_.MarkAndPush(ref);
}

MutatorUnifiedHeapMarkingVisitor::MutatorUnifiedHeapMarkingVisitor(
    HeapBase& heap, MutatorMarkingState& marking_state,
    UnifiedHeapMarkingState& unified_heap_marking_state)
    : UnifiedHeapMarkingVisitorBase(heap, marking_state,
                                    unified_heap_marking_state) {}

void MutatorUnifiedHeapMarkingVisitor::VisitRoot(const void* object,
                                                 TraceDescriptor desc,
                                                 const SourceLocation&) {
  this->Visit(object, desc);
}

void MutatorUnifiedHeapMarkingVisitor::VisitWeakRoot(const void* object,
                                                     TraceDescriptor desc,
                                                     WeakCallback weak_callback,
                                                     const void* weak_root,
                                                     const SourceLocation&) {
  static_cast<MutatorMarkingState&>(marking_state_)
      .InvokeWeakRootsCallbackIfNeeded(object, desc, weak_callback, weak_root);
}

ConcurrentUnifiedHeapMarkingVisitor::ConcurrentUnifiedHeapMarkingVisitor(
    HeapBase& heap, Heap* v8_heap,
    cppgc::internal::ConcurrentMarkingState& marking_state)
    : UnifiedHeapMarkingVisitorBase(heap, marking_state,
                                    concurrent_unified_heap_marking_state_),
      local_marking_worklist_(
          v8_heap ? std::make_unique<MarkingWorklists::Local>(
                        v8_heap->mark_compact_collector()->marking_worklists())
                  : nullptr),
      concurrent_unified_heap_marking_state_(v8_heap,
                                             local_marking_worklist_.get()) {}

ConcurrentUnifiedHeapMarkingVisitor::~ConcurrentUnifiedHeapMarkingVisitor() {
  if (local_marking_worklist_) {
    local_marking_worklist_->Publish();
  }
}

bool ConcurrentUnifiedHeapMarkingVisitor::DeferTraceToMutatorThreadIfConcurrent(
    const void* parameter, cppgc::TraceCallback callback,
    size_t deferred_size) {
  marking_state_.concurrent_marking_bailout_worklist().Push(
      {parameter, callback, deferred_size});
  static_cast<cppgc::internal::ConcurrentMarkingState&>(marking_state_)
      .AccountDeferredMarkedBytes(deferred_size);
  return true;
}

}  // namespace internal
}  // namespace v8
