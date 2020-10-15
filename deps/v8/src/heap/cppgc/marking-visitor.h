// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_VISITOR_H_
#define V8_HEAP_CPPGC_MARKING_VISITOR_H_

#include "include/cppgc/trace-trait.h"
#include "src/base/macros.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc/visitor.h"

namespace cppgc {
namespace internal {

class HeapBase;
class HeapObjectHeader;
class Marker;
class MarkingState;

class V8_EXPORT_PRIVATE MarkingVisitor : public VisitorBase {
 public:
  MarkingVisitor(HeapBase&, MarkingState&);
  ~MarkingVisitor() override = default;

 private:
  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void VisitRoot(const void*, TraceDescriptor) final;
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback,
                     const void*) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;

  MarkingState& marking_state_;
};

class ConservativeMarkingVisitor : public ConservativeTracingVisitor,
                                   public heap::base::StackVisitor {
 public:
  ConservativeMarkingVisitor(HeapBase&, MarkingState&, cppgc::Visitor&);
  ~ConservativeMarkingVisitor() override = default;

 private:
  void VisitConservatively(HeapObjectHeader&,
                           TraceConservativelyCallback) final;
  void VisitPointer(const void*) final;

  MarkingState& marking_state_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_VISITOR_H_
