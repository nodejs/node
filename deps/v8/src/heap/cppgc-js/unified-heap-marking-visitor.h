// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "include/cppgc/trace-trait.h"
#include "include/v8-cppgc.h"
#include "src/base/macros.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"

namespace cppgc {

class SourceLocation;

namespace internal {
class ConcurrentMarkingState;
class MarkingStateBase;
class MutatorMarkingState;
}  // namespace internal
}  // namespace cppgc

namespace v8 {
namespace internal {

using cppgc::SourceLocation;
using cppgc::TraceDescriptor;
using cppgc::WeakCallback;
using cppgc::internal::HeapBase;
using cppgc::internal::MutatorMarkingState;

class V8_EXPORT_PRIVATE UnifiedHeapMarkingVisitorBase : public JSVisitor {
 public:
  UnifiedHeapMarkingVisitorBase(HeapBase&, cppgc::internal::MarkingStateBase&,
                                UnifiedHeapMarkingState&);
  ~UnifiedHeapMarkingVisitorBase() override = default;

 protected:
  // C++ handling.
  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void VisitEphemeron(const void*, TraceDescriptor) final;
  void VisitWeakContainer(const void* self, TraceDescriptor strong_desc,
                          TraceDescriptor weak_desc, WeakCallback callback,
                          const void* data) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;
  void HandleMovableReference(const void**) final;

  // JS handling.
  void Visit(const TracedReferenceBase& ref) final;

  cppgc::internal::MarkingStateBase& marking_state_;
  UnifiedHeapMarkingState& unified_heap_marking_state_;
};

class V8_EXPORT_PRIVATE MutatorUnifiedHeapMarkingVisitor final
    : public UnifiedHeapMarkingVisitorBase {
 public:
  MutatorUnifiedHeapMarkingVisitor(HeapBase&, MutatorMarkingState&,
                                   UnifiedHeapMarkingState&);
  ~MutatorUnifiedHeapMarkingVisitor() override = default;

 protected:
  void VisitRoot(const void*, TraceDescriptor, const SourceLocation&) final;
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback, const void*,
                     const SourceLocation&) final;
};

class V8_EXPORT_PRIVATE ConcurrentUnifiedHeapMarkingVisitor final
    : public UnifiedHeapMarkingVisitorBase {
 public:
  ConcurrentUnifiedHeapMarkingVisitor(HeapBase&,
                                      cppgc::internal::ConcurrentMarkingState&,
                                      UnifiedHeapMarkingState&);
  ~ConcurrentUnifiedHeapMarkingVisitor() override = default;

 protected:
  void VisitRoot(const void*, TraceDescriptor, const SourceLocation&) final {
    UNREACHABLE();
  }
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback, const void*,
                     const SourceLocation&) final {
    UNREACHABLE();
  }

  bool DeferTraceToMutatorThreadIfConcurrent(const void*, cppgc::TraceCallback,
                                             size_t) final;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
