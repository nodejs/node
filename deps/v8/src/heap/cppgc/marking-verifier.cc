// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-verifier.h"

#include "src/base/logging.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/object-view.h"

namespace cppgc {
namespace internal {

MarkingVerifierBase::MarkingVerifierBase(
    HeapBase& heap, VerificationState& verification_state,
    std::unique_ptr<cppgc::Visitor> visitor)
    : ConservativeTracingVisitor(heap, *heap.page_backend(), *visitor.get()),
      verification_state_(verification_state),
      visitor_(std::move(visitor)) {}

void MarkingVerifierBase::Run(Heap::Config::StackState stack_state,
                              uintptr_t stack_end,
                              size_t expected_marked_bytes) {
  Traverse(heap_.raw_heap());
  if (stack_state == Heap::Config::StackState::kMayContainHeapPointers) {
    in_construction_objects_ = &in_construction_objects_stack_;
    heap_.stack()->IteratePointersUnsafe(this, stack_end);
    // The objects found through the unsafe iteration are only a subset of the
    // regular iteration as they miss objects held alive only from callee-saved
    // registers that are never pushed on the stack and SafeStack.
    CHECK_LE(in_construction_objects_stack_.size(),
             in_construction_objects_heap_.size());
    for (auto* header : in_construction_objects_stack_) {
      CHECK_NE(in_construction_objects_heap_.end(),
               in_construction_objects_heap_.find(header));
    }
  }
#ifdef CPPGC_VERIFY_LIVE_BYTES
  CHECK_EQ(expected_marked_bytes, found_marked_bytes_);
#endif  // CPPGC_VERIFY_LIVE_BYTES
}

void VerificationState::VerifyMarked(const void* base_object_payload) const {
  const HeapObjectHeader& child_header =
      HeapObjectHeader::FromObject(base_object_payload);

  if (!child_header.IsMarked()) {
    FATAL(
        "MarkingVerifier: Encountered unmarked object.\n"
        "#\n"
        "# Hint:\n"
        "#   %s (%p)\n"
        "#     \\-> %s (%p)",
        parent_ ? parent_->GetName().value : "Stack",
        parent_ ? parent_->ObjectStart() : nullptr,
        child_header.GetName().value, child_header.ObjectStart());
  }
}

void MarkingVerifierBase::VisitInConstructionConservatively(
    HeapObjectHeader& header, TraceConservativelyCallback callback) {
  if (in_construction_objects_->find(&header) !=
      in_construction_objects_->end())
    return;
  in_construction_objects_->insert(&header);

  // Stack case: Parent is stack and this is merely ensuring that the object
  // itself is marked. If the object is marked, then it is being processed by
  // the on-heap phase.
  if (verification_state_.IsParentOnStack()) {
    verification_state_.VerifyMarked(header.ObjectStart());
    return;
  }

  // Heap case: Dispatching parent object that must be marked (pre-condition).
  CHECK(header.IsMarked());
  callback(this, header);
}

void MarkingVerifierBase::VisitPointer(const void* address) {
  // Entry point for stack walk. The conservative visitor dispatches as follows:
  // - Fully constructed objects: Visit()
  // - Objects in construction: VisitInConstructionConservatively()
  TraceConservativelyIfNeeded(address);
}

bool MarkingVerifierBase::VisitHeapObjectHeader(HeapObjectHeader& header) {
  // Verify only non-free marked objects.
  if (!header.IsMarked()) return true;

  DCHECK(!header.IsFree());

  verification_state_.SetCurrentParent(&header);

  if (!header.IsInConstruction()) {
    header.Trace(visitor_.get());
  } else {
    // Dispatches to conservative tracing implementation.
    TraceConservativelyIfNeeded(header);
  }

  found_marked_bytes_ += ObjectView(header).Size() + sizeof(HeapObjectHeader);

  verification_state_.SetCurrentParent(nullptr);

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
                          const void*) final {
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
    : MarkingVerifierBase(heap_base, state_,
                          std::make_unique<VerificationVisitor>(state_)) {}

}  // namespace internal
}  // namespace cppgc
