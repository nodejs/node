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
namespace internal {
class MarkingState;
}  // namespace internal
}  // namespace cppgc

namespace v8 {
namespace internal {

using cppgc::TraceDescriptor;
using cppgc::WeakCallback;
using cppgc::internal::HeapBase;
using cppgc::internal::MarkingState;

class V8_EXPORT_PRIVATE UnifiedHeapMarkingVisitor : public JSVisitor {
 public:
  UnifiedHeapMarkingVisitor(HeapBase&, MarkingState&, UnifiedHeapMarkingState&);
  ~UnifiedHeapMarkingVisitor() override = default;

 private:
  // C++ handling.
  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void VisitRoot(const void*, TraceDescriptor) final;
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback,
                     const void*) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;

  // JS handling.
  void Visit(const internal::JSMemberBase& ref) final;

  MarkingState& marking_state_;
  UnifiedHeapMarkingState& unified_heap_marking_state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
