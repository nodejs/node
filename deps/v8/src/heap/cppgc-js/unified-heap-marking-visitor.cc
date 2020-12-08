// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/unified-heap-marking-visitor.h"

#include "include/v8.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/visitor.h"

namespace v8 {
namespace internal {

UnifiedHeapMarkingVisitorBase::UnifiedHeapMarkingVisitorBase(
    HeapBase& heap, cppgc::internal::MarkingStateBase& marking_state,
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
                                                   TraceDescriptor value_desc) {
  marking_state_.ProcessEphemeron(key, value_desc);
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

namespace {
void DeferredTraceTracedReference(cppgc::Visitor* visitor, const void* ref) {
  static_cast<JSVisitor*>(visitor)->Trace(
      *static_cast<const TracedReferenceBase*>(ref));
}
}  // namespace

void UnifiedHeapMarkingVisitorBase::Visit(const TracedReferenceBase& ref) {
  bool should_defer_tracing = DeferTraceToMutatorThreadIfConcurrent(
      &ref, DeferredTraceTracedReference, 0);

  if (!should_defer_tracing) unified_heap_marking_state_.MarkAndPush(ref);
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
    HeapBase& heap, cppgc::internal::ConcurrentMarkingState& marking_state,
    UnifiedHeapMarkingState& unified_heap_marking_state)
    : UnifiedHeapMarkingVisitorBase(heap, marking_state,
                                    unified_heap_marking_state) {}

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
