// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-verifier.h"

#include "src/base/logging.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-visitor.h"

namespace cppgc {
namespace internal {

MarkingVerifierBase::MarkingVerifierBase(
    HeapBase& heap, std::unique_ptr<cppgc::Visitor> visitor)
    : ConservativeTracingVisitor(heap, *heap.page_backend(), *visitor.get()),
      visitor_(std::move(visitor)) {}

void MarkingVerifierBase::Run(Heap::Config::StackState stack_state) {
  Traverse(&heap_.raw_heap());
  if (stack_state == Heap::Config::StackState::kMayContainHeapPointers) {
    in_construction_objects_ = &in_construction_objects_stack_;
    heap_.stack()->IteratePointers(this);
    CHECK_EQ(in_construction_objects_stack_, in_construction_objects_heap_);
  }
}

void VerificationState::VerifyMarked(const void* base_object_payload) const {
  const HeapObjectHeader& child_header =
      HeapObjectHeader::FromPayload(base_object_payload);

  if (!child_header.IsMarked()) {
    FATAL(
        "MarkingVerifier: Encountered unmarked object.\n"
        "#\n"
        "# Hint:\n"
        "#   %s\n"
        "#     \\-> %s",
        parent_->GetName().value, child_header.GetName().value);
  }
}

void MarkingVerifierBase::VisitInConstructionConservatively(
    HeapObjectHeader& header, TraceConservativelyCallback callback) {
  CHECK(header.IsMarked());
  in_construction_objects_->insert(&header);
  callback(this, header);
}

void MarkingVerifierBase::VisitPointer(const void* address) {
  TraceConservativelyIfNeeded(address);
}

bool MarkingVerifierBase::VisitHeapObjectHeader(HeapObjectHeader* header) {
  // Verify only non-free marked objects.
  if (!header->IsMarked()) return true;

  DCHECK(!header->IsFree());

  SetCurrentParent(header);

  if (!header->IsInConstruction()) {
    header->Trace(visitor_.get());
  } else {
    // Dispatches to conservative tracing implementation.
    TraceConservativelyIfNeeded(*header);
  }

  return true;
}

namespace {

class VerificationVisitor final : public cppgc::Visitor {
 public:
  explicit VerificationVisitor(VerificationState& state)
      : cppgc::Visitor(VisitorFactory::CreateKey()), state_(state) {}

  void Visit(const void*, TraceDescriptor desc) final {
    state_.VerifyMarked(desc.base_object_payload);
  }

  void VisitWeak(const void*, TraceDescriptor desc, WeakCallback,
                 const void*) final {
    // Weak objects should have been cleared at this point. As a consequence,
    // all objects found through weak references have to point to live objects
    // at this point.
    state_.VerifyMarked(desc.base_object_payload);
  }

  void VisitWeakContainer(const void* object, TraceDescriptor,
                          TraceDescriptor weak_desc, WeakCallback,
                          const void*) {
    if (!object) return;

    // Contents of weak containers are found themselves through page iteration
    // and are treated strongly, similar to how they are treated strongly when
    // found through stack scanning. The verification here only makes sure that
    // the container itself is properly marked.
    state_.VerifyMarked(weak_desc.base_object_payload);
  }

 private:
  VerificationState& state_;
};

}  // namespace

MarkingVerifier::MarkingVerifier(HeapBase& heap_base)
    : MarkingVerifierBase(heap_base,
                          std::make_unique<VerificationVisitor>(state_)) {}

void MarkingVerifier::SetCurrentParent(const HeapObjectHeader* parent) {
  state_.SetCurrentParent(parent);
}

}  // namespace internal
}  // namespace cppgc
