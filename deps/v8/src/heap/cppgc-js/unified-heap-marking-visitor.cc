// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/unified-heap-marking-visitor.h"

#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/visitor.h"

namespace v8 {
namespace internal {

UnifiedHeapMarkingVisitor::UnifiedHeapMarkingVisitor(
    HeapBase& heap, MarkingState& marking_state,
    UnifiedHeapMarkingState& unified_heap_marking_state)
    : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
      marking_state_(marking_state),
      unified_heap_marking_state_(unified_heap_marking_state) {}

void UnifiedHeapMarkingVisitor::Visit(const void* object,
                                      TraceDescriptor desc) {
  marking_state_.MarkAndPush(object, desc);
}

void UnifiedHeapMarkingVisitor::VisitWeak(const void* object,
                                          TraceDescriptor desc,
                                          WeakCallback weak_callback,
                                          const void* weak_member) {
  marking_state_.RegisterWeakReferenceIfNeeded(object, desc, weak_callback,
                                               weak_member);
}

void UnifiedHeapMarkingVisitor::VisitRoot(const void* object,
                                          TraceDescriptor desc) {
  Visit(object, desc);
}

void UnifiedHeapMarkingVisitor::VisitWeakRoot(const void* object,
                                              TraceDescriptor desc,
                                              WeakCallback weak_callback,
                                              const void* weak_root) {
  marking_state_.InvokeWeakRootsCallbackIfNeeded(object, desc, weak_callback,
                                                 weak_root);
}

void UnifiedHeapMarkingVisitor::RegisterWeakCallback(WeakCallback callback,
                                                     const void* object) {
  marking_state_.RegisterWeakCallback(callback, object);
}

void UnifiedHeapMarkingVisitor::Visit(const internal::JSMemberBase& ref) {
  unified_heap_marking_state_.MarkAndPush(ref);
}

}  // namespace internal
}  // namespace v8
