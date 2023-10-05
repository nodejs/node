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
class BasicMarkingState;
class MutatorMarkingState;
class ConcurrentMarkingState;

class V8_EXPORT_PRIVATE MarkingVisitorBase : public VisitorBase {
 public:
  MarkingVisitorBase(HeapBase&, BasicMarkingState&);
  ~MarkingVisitorBase() override = default;

 protected:
  void Visit(const void*, TraceDescriptor) final;
  void VisitMultipleUncompressedMember(const void*, size_t,
                                       TraceDescriptorCallback) final;
#if defined(CPPGC_POINTER_COMPRESSION)
  void VisitMultipleCompressedMember(const void*, size_t,
                                     TraceDescriptorCallback) final;
#endif  // defined(CPPGC_POINTER_COMPRESSION)
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void VisitEphemeron(const void*, const void*, TraceDescriptor) final;
  void VisitWeakContainer(const void* object, TraceDescriptor strong_desc,
                          TraceDescriptor weak_desc, WeakCallback callback,
                          const void* data) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;
  void HandleMovableReference(const void**) final;

  BasicMarkingState& marking_state_;
};

class V8_EXPORT_PRIVATE MutatorMarkingVisitor : public MarkingVisitorBase {
 public:
  MutatorMarkingVisitor(HeapBase&, MutatorMarkingState&);
  ~MutatorMarkingVisitor() override = default;
};

class V8_EXPORT_PRIVATE ConcurrentMarkingVisitor final
    : public MarkingVisitorBase {
 public:
  ConcurrentMarkingVisitor(HeapBase&, ConcurrentMarkingState&);
  ~ConcurrentMarkingVisitor() override = default;

 protected:
  bool DeferTraceToMutatorThreadIfConcurrent(const void*, TraceCallback,
                                             size_t) final;
};

class V8_EXPORT_PRIVATE RootMarkingVisitor : public RootVisitorBase {
 public:
  explicit RootMarkingVisitor(MutatorMarkingState&);
  ~RootMarkingVisitor() override = default;

 protected:
  void VisitRoot(const void*, TraceDescriptor, const SourceLocation&) final;
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback, const void*,
                     const SourceLocation&) final;

  MutatorMarkingState& mutator_marking_state_;
};

class ConservativeMarkingVisitor : public ConservativeTracingVisitor,
                                   public heap::base::StackVisitor {
 public:
  ConservativeMarkingVisitor(HeapBase&, MutatorMarkingState&, cppgc::Visitor&);
  ~ConservativeMarkingVisitor() override = default;

 private:
  void VisitFullyConstructedConservatively(HeapObjectHeader&) final;
  void VisitInConstructionConservatively(HeapObjectHeader&,
                                         TraceConservativelyCallback) final;
  void VisitPointer(const void*) final;

  MutatorMarkingState& marking_state_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MARKING_VISITOR_H_
