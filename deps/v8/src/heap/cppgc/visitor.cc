// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/visitor.h"

#include "src/base/sanitizer/msan.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/object-view.h"
#include "src/heap/cppgc/page-memory.h"

namespace cppgc {

#ifdef V8_ENABLE_CHECKS
void Visitor::CheckObjectNotInConstruction(const void* address) {
  // TODO(chromium:1056170): |address| is an inner pointer of an object. Check
  // that the object is not in construction.
}
#endif  // V8_ENABLE_CHECKS

namespace internal {

ConservativeTracingVisitor::ConservativeTracingVisitor(
    HeapBase& heap, PageBackend& page_backend, cppgc::Visitor& visitor)
    : heap_(heap), page_backend_(page_backend), visitor_(visitor) {}

namespace {

void TraceConservatively(ConservativeTracingVisitor* conservative_visitor,
                         const HeapObjectHeader& header) {
  const auto object_view = ObjectView(header);
  Address* object = reinterpret_cast<Address*>(object_view.Start());
  for (size_t i = 0; i < (object_view.Size() / sizeof(Address)); ++i) {
    Address maybe_ptr = object[i];
#if defined(MEMORY_SANITIZER)
    // |object| may be uninitialized by design or just contain padding bytes.
    // Copy into a local variable that is not poisoned for conservative marking.
    // Copy into a temporary variable to maintain the original MSAN state.
    MSAN_MEMORY_IS_INITIALIZED(&maybe_ptr, sizeof(maybe_ptr));
#endif
    if (maybe_ptr) {
      conservative_visitor->TraceConservativelyIfNeeded(maybe_ptr);
    }
  }
}

}  // namespace

void ConservativeTracingVisitor::TraceConservativelyIfNeeded(
    const void* address) {
  const BasePage* page = reinterpret_cast<const BasePage*>(
      page_backend_.Lookup(static_cast<ConstAddress>(address)));

  if (!page) return;

  DCHECK_EQ(&heap_, &page->heap());

  auto* header = page->TryObjectHeaderFromInnerAddress(
      const_cast<Address>(reinterpret_cast<ConstAddress>(address)));

  if (!header) return;

  TraceConservativelyIfNeeded(*header);
}

void ConservativeTracingVisitor::TraceConservativelyIfNeeded(
    HeapObjectHeader& header) {
  if (!header.IsInConstruction<AccessMode::kNonAtomic>()) {
    VisitFullyConstructedConservatively(header);
  } else {
    VisitInConstructionConservatively(header, TraceConservatively);
  }
}

void ConservativeTracingVisitor::VisitFullyConstructedConservatively(
    HeapObjectHeader& header) {
  visitor_.Visit(
      header.ObjectStart(),
      {header.ObjectStart(),
       GlobalGCInfoTable::GCInfoFromIndex(header.GetGCInfoIndex()).trace});
}

}  // namespace internal
}  // namespace cppgc
