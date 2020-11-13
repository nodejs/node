// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MARKING_VERIFIER_H_
#define V8_HEAP_CPPGC_MARKING_VERIFIER_H_

#include <unordered_set>

#include "src/heap/base/stack.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/visitor.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE MarkingVerifier final
    : private HeapVisitor<MarkingVerifier>,
      public cppgc::Visitor,
      public ConservativeTracingVisitor,
      public heap::base::StackVisitor {
  friend class HeapVisitor<MarkingVerifier>;

 public:
  explicit MarkingVerifier(HeapBase&, Heap::Config::StackState);

  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;

 private:
  void VerifyChild(const void*);

  void VisitConservatively(HeapObjectHeader&,
                           TraceConservativelyCallback) final;
  void VisitPointer(const void*) final;

  bool VisitHeapObjectHeader(HeapObjectHeader*);

  std::unordered_set<const HeapObjectHeader*> in_construction_objects_heap_;
  std::unordered_set<const HeapObjectHeader*> in_construction_objects_stack_;
  std::unordered_set<const HeapObjectHeader*>* in_construction_objects_ =
      &in_construction_objects_heap_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_VERIFIER_H_
