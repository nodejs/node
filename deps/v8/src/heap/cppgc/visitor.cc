// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/visitor.h"

#include "src/base/sanitizer/msan.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/object-view.h"
#include "src/heap/cppgc/page-memory.h"

#if defined(CPPGC_CAGED_HEAP)
#include "src/heap/cppgc/caged-heap.h"
#endif  // defined(CPPGC_CAGED_HEAP)

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
#if defined(CPPGC_POINTER_COMPRESSION)
  using PointerType = uint32_t;
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  using PointerType = uintptr_t;
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

  const auto object_view = ObjectView<>(header);
  PointerType* object = reinterpret_cast<PointerType*>(object_view.Start());
  for (size_t i = 0; i < (object_view.Size() / sizeof(PointerType)); ++i) {
    PointerType maybe_ptr = object[i];
#if defined(MEMORY_SANITIZER)
    // |object| may be uninitialized by design or just contain padding bytes.
    // Copy into a local variable that is not poisoned for conservative marking.
    // Copy into a temporary variable to maintain the original MSAN state.
    MSAN_MEMORY_IS_INITIALIZED(&maybe_ptr, sizeof(maybe_ptr));
#endif
    if (maybe_ptr > SentinelPointer::kSentinelValue) {
#if defined(CPPGC_POINTER_COMPRESSION)
      // We know that all on-heap pointers are compressed, so don't check full
      // pointers.
      Address decompressed_ptr =
          static_cast<Address>(CompressedPointer::Decompress(maybe_ptr));
#else   // !defined(CPPGC_POINTER_COMPRESSION)
      Address decompressed_ptr = reinterpret_cast<Address>(maybe_ptr);
#endif  // !defined(CPPGC_POINTER_COMPRESSION)
      conservative_visitor->TraceConservativelyIfNeeded(decompressed_ptr);
    }
  }
}

}  // namespace

void ConservativeTracingVisitor::TryTracePointerConservatively(
    Address pointer) {
#if defined(CPPGC_CAGED_HEAP)
  // TODO(chromium:1056170): Add support for SIMD in stack scanning.
  if (V8_LIKELY(!CagedHeapBase::IsWithinCage(pointer))) return;
#endif  // defined(CPPGC_CAGED_HEAP)

  const BasePage* page = reinterpret_cast<const BasePage*>(
      page_backend_.Lookup(const_cast<ConstAddress>(pointer)));

  if (!page) return;

  DCHECK_EQ(&heap_, &page->heap());

  auto* header = page->TryObjectHeaderFromInnerAddress(pointer);

  if (!header) return;

  TraceConservativelyIfNeeded(*header);
}

void ConservativeTracingVisitor::TraceConservativelyIfNeeded(
    const void* address) {
  auto pointer = reinterpret_cast<Address>(const_cast<void*>(address));
  TryTracePointerConservatively(pointer);
#if defined(CPPGC_POINTER_COMPRESSION)
  // If pointer compression enabled, we may have random compressed pointers on
  // stack (e.g. due to inlined collections). Extract, decompress and trace both
  // halfwords.
  auto decompressed_low =
      reinterpret_cast<Address>(CompressedPointer::Decompress(
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer))));
  if (decompressed_low >
      reinterpret_cast<void*>(SentinelPointer::kSentinelValue))
    TryTracePointerConservatively(decompressed_low);
  auto decompressed_high =
      reinterpret_cast<Address>(CompressedPointer::Decompress(
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer) >>
                                (sizeof(uint32_t) * CHAR_BIT))));
  if (decompressed_high >
      reinterpret_cast<void*>(SentinelPointer::kSentinelValue))
    TryTracePointerConservatively(decompressed_high);
#endif  // defined(CPPGC_POINTER_COMPRESSION)
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
