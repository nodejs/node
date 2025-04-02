// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-verifier.h"

#include <optional>

#include "src/base/logging.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/object-view.h"

#if defined(CPPGC_CAGED_HEAP)
#include "include/cppgc/internal/caged-heap-local-data.h"
#endif  // defined(CPPGC_CAGED_HEAP)

namespace cppgc {
namespace internal {

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
        parent_
            ? parent_
                  ->GetName(
                      HeapObjectNameForUnnamedObject::kUseClassNameIfSupported)
                  .value
            : "Stack",
        parent_ ? parent_->ObjectStart() : nullptr,
        child_header
            .GetName(HeapObjectNameForUnnamedObject::kUseClassNameIfSupported)
            .value,
        child_header.ObjectStart());
  }
}

MarkingVerifierBase::MarkingVerifierBase(
    HeapBase& heap, CollectionType collection_type,
    VerificationState& verification_state,
    std::unique_ptr<cppgc::Visitor> visitor)
    : ConservativeTracingVisitor(heap, *heap.page_backend(), *visitor),
      verification_state_(verification_state),
      visitor_(std::move(visitor)),
      collection_type_(collection_type) {}

void MarkingVerifierBase::Run(StackState stack_state,
                              std::optional<size_t> expected_marked_bytes) {
  Traverse(heap_.raw_heap());
// Avoid verifying the stack when running with TSAN as the TSAN runtime changes
// stack contents when e.g. working with locks. Specifically, the marker uses
// locks in slow path operations which results in stack changes throughout
// marking. This means that the conservative iteration below may find more
// objects then the regular marker. The difference is benign as the delta of
// objects is not reachable from user code but it prevents verification.
// We also avoid verifying the stack when pointer compression is enabled.
// Currently, verification happens after compaction, V8 compaction can change
// slots on stack, which could lead to false positives in verifier. Those are
// more likely with checking compressed pointers on stack.
// TODO(chromium:1325007): Investigate if Oilpan verification can be moved
// before V8 compaction or compaction never runs with stack.
#if !defined(THREAD_SANITIZER) && !defined(CPPGC_POINTER_COMPRESSION)
  if (stack_state == StackState::kMayContainHeapPointers) {
    in_construction_objects_ = &in_construction_objects_stack_;
    heap_.stack()->IteratePointersUntilMarker(this);
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
#endif  // !defined(THREAD_SANITIZER)
  if (expected_marked_bytes && verifier_found_marked_bytes_are_exact_) {
    // Report differences in marked objects, if possible.
    if (V8_UNLIKELY(expected_marked_bytes.value() !=
                    verifier_found_marked_bytes_) &&
        collection_type_ != CollectionType::kMinor) {
      ReportDifferences(expected_marked_bytes.value());
    }
    CHECK_EQ(expected_marked_bytes.value(), verifier_found_marked_bytes_);
    // Minor GCs use sticky markbits and as such cannot expect that the marked
    // bytes on pages match the marked bytes accumulated by the marker.
    if (collection_type_ != CollectionType::kMinor) {
      CHECK_EQ(expected_marked_bytes.value(),
               verifier_found_marked_bytes_in_pages_);
    }
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

bool MarkingVerifierBase::VisitNormalPage(NormalPage& page) {
  verifier_found_marked_bytes_in_pages_ += page.marked_bytes();
  return false;  // Continue visitation.
}

bool MarkingVerifierBase::VisitLargePage(LargePage& page) {
  verifier_found_marked_bytes_in_pages_ += page.marked_bytes();
  return false;  // Continue visitation.
}

bool MarkingVerifierBase::VisitHeapObjectHeader(HeapObjectHeader& header) {
  // Verify only non-free marked objects.
  if (!header.IsMarked()) return true;

  DCHECK(!header.IsFree());

#if defined(CPPGC_YOUNG_GENERATION)
  if (collection_type_ == CollectionType::kMinor) {
    auto& caged_heap = CagedHeap::Instance();
    const auto age = CagedHeapLocalData::Get().age_table.GetAge(
        caged_heap.OffsetFromAddress(header.ObjectStart()));
    if (age == AgeTable::Age::kOld) {
      // Do not verify old objects.
      return true;
    } else if (age == AgeTable::Age::kMixed) {
      // If the age is not known, the marked bytes may not be exact as possibly
      // old objects are verified as well.
      verifier_found_marked_bytes_are_exact_ = false;
    }
    // Verify young and unknown objects.
  }
#endif  // defined(CPPGC_YOUNG_GENERATION)

  verification_state_.SetCurrentParent(&header);

  if (!header.IsInConstruction()) {
    header.Trace(visitor_.get());
  } else {
    // Dispatches to conservative tracing implementation.
    TraceConservativelyIfNeeded(header);
  }

  verifier_found_marked_bytes_ +=
      ObjectView<>(header).Size() + sizeof(HeapObjectHeader);

  verification_state_.SetCurrentParent(nullptr);

  return true;
}

void MarkingVerifierBase::ReportDifferences(
    size_t expected_marked_bytes) const {
  v8::base::OS::PrintError("\n<--- Mismatch in marking verifier --->\n");
  v8::base::OS::PrintError(
      "Marked bytes: expected %zu vs. verifier found %zu, difference %zd\n",
      expected_marked_bytes, verifier_found_marked_bytes_,
      expected_marked_bytes - verifier_found_marked_bytes_);
  v8::base::OS::PrintError(
      "A list of pages with possibly mismatched marked objects follows.\n");
  for (auto& space : heap_.raw_heap()) {
    for (auto* page : *space) {
      size_t marked_bytes_on_page = 0;
      if (page->is_large()) {
        const auto& large_page = *LargePage::From(page);
        const auto& header = *large_page.ObjectHeader();
        if (header.IsMarked())
          marked_bytes_on_page +=
              ObjectView<>(header).Size() + sizeof(HeapObjectHeader);
        if (marked_bytes_on_page == large_page.marked_bytes()) continue;
        ReportLargePage(large_page, marked_bytes_on_page);
        ReportHeapObjectHeader(header);
      } else {
        const auto& normal_page = *NormalPage::From(page);
        for (const auto& header : normal_page) {
          if (header.IsMarked())
            marked_bytes_on_page +=
                ObjectView<>(header).Size() + sizeof(HeapObjectHeader);
        }
        if (marked_bytes_on_page == normal_page.marked_bytes()) continue;
        ReportNormalPage(normal_page, marked_bytes_on_page);
        for (const auto& header : normal_page) {
          ReportHeapObjectHeader(header);
        }
      }
    }
  }
}

void MarkingVerifierBase::ReportNormalPage(const NormalPage& page,
                                           size_t marked_bytes_on_page) const {
  v8::base::OS::PrintError(
      "\nNormal page in space %zu:\n"
      "Marked bytes: expected %zu vs. verifier found %zu, difference %zd\n",
      page.space().index(), page.marked_bytes(), marked_bytes_on_page,
      page.marked_bytes() - marked_bytes_on_page);
}

void MarkingVerifierBase::ReportLargePage(const LargePage& page,
                                          size_t marked_bytes_on_page) const {
  v8::base::OS::PrintError(
      "\nLarge page in space %zu:\n"
      "Marked bytes: expected %zu vs. verifier found %zu, difference %zd\n",
      page.space().index(), page.marked_bytes(), marked_bytes_on_page,
      page.marked_bytes() - marked_bytes_on_page);
}

void MarkingVerifierBase::ReportHeapObjectHeader(
    const HeapObjectHeader& header) const {
  const char* name =
      header.IsFree()
          ? "free space"
          : header
                .GetName(
                    HeapObjectNameForUnnamedObject::kUseClassNameIfSupported)
                .value;
  v8::base::OS::PrintError("- %s at %p, size %zu, %s\n", name,
                           header.ObjectStart(), header.ObjectSize(),
                           header.IsMarked() ? "marked" : "unmarked");
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

MarkingVerifier::MarkingVerifier(HeapBase& heap_base,
                                 CollectionType collection_type)
    : MarkingVerifierBase(heap_base, collection_type, state_,
                          std::make_unique<VerificationVisitor>(state_)) {}

}  // namespace internal
}  // namespace cppgc
