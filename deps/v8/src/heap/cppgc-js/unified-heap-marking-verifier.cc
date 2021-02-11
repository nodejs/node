

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/unified-heap-marking-verifier.h"

#include "include/v8-cppgc.h"
#include "src/heap/cppgc/marking-verifier.h"

namespace v8 {
namespace internal {

namespace {

class UnifiedHeapVerificationVisitor final : public JSVisitor {
 public:
  explicit UnifiedHeapVerificationVisitor(
      cppgc::internal::VerificationState& state)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        state_(state) {}

  void Visit(const void*, cppgc::TraceDescriptor desc) final {
    state_.VerifyMarked(desc.base_object_payload);
  }

  void VisitWeak(const void*, cppgc::TraceDescriptor desc, cppgc::WeakCallback,
                 const void*) final {
    // Weak objects should have been cleared at this point. As a consequence,
    // all objects found through weak references have to point to live objects
    // at this point.
    state_.VerifyMarked(desc.base_object_payload);
  }

  void VisitWeakContainer(const void* object, cppgc::TraceDescriptor,
                          cppgc::TraceDescriptor weak_desc, cppgc::WeakCallback,
                          const void*) {
    if (!object) return;

    // Contents of weak containers are found themselves through page iteration
    // and are treated strongly, similar to how they are treated strongly when
    // found through stack scanning. The verification here only makes sure that
    // the container itself is properly marked.
    state_.VerifyMarked(weak_desc.base_object_payload);
  }

  void Visit(const TracedReferenceBase& ref) final {
    // TODO(chromium:1056170): Verify V8 object is indeed marked.
  }

 private:
  cppgc::internal::VerificationState& state_;
};

}  // namespace

UnifiedHeapMarkingVerifier::UnifiedHeapMarkingVerifier(
    cppgc::internal::HeapBase& heap_base)
    : MarkingVerifierBase(
          heap_base, std::make_unique<UnifiedHeapVerificationVisitor>(state_)) {
}

void UnifiedHeapMarkingVerifier::SetCurrentParent(
    const cppgc::internal::HeapObjectHeader* parent) {
  state_.SetCurrentParent(parent);
}

}  // namespace internal
}  // namespace v8
