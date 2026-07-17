// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_VISITOR_H_
#define V8_HEAP_CPPGC_HEAP_VISITOR_H_

#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/raw-heap.h"

namespace cppgc {
namespace internal {

// Visitor for heap, which also implements the accept (traverse) interface.
// Implements preorder traversal of the heap. The order of traversal is defined.
// Implemented as a CRTP visitor to avoid virtual calls and support better
// inlining.
template <typename Derived>
class HeapVisitor {
 public:
  void Traverse(RawHeap& heap) {
    if (VisitHeapImpl(heap)) return;
    for (auto& space : heap) {
      Traverse(*space.get());
    }
  }

  void Traverse(BaseSpace& space) {
    const bool is_stopped =
        space.is_large()
            ? VisitLargePageSpaceImpl(LargePageSpace::From(space))
            : VisitNormalPageSpaceImpl(NormalPageSpace::From(space));
    if (is_stopped) return;
    for (auto* page : space) {
      Traverse(*page);
    }
  }

  void Traverse(BasePage& page) {
    if (page.is_large()) {
      auto* large_page = LargePage::From(&page);
      if (VisitLargePageImpl(*large_page)) return;
      VisitHeapObjectHeaderImpl(*large_page->ObjectHeader());
    } else {
      auto* normal_page = NormalPage::From(&page);
      if (VisitNormalPageImpl(*normal_page)) return;
      for (auto& header : *normal_page) {
        VisitHeapObjectHeaderImpl(header);
      }
    }
  }

 protected:
  // Visitor functions return true if no deeper processing is required.
  // Users are supposed to override functions that need special treatment.
  bool VisitHeap(RawHeap&) { return false; }
  bool VisitNormalPageSpace(NormalPageSpace&) { return false; }
  bool VisitLargePageSpace(LargePageSpace&) { return false; }
  bool VisitNormalPage(NormalPage&) { return false; }
  bool VisitLargePage(LargePage&) { return false; }
  bool VisitHeapObjectHeader(HeapObjectHeader&) { return false; }

 private:
  Derived& ToDerived() { return static_cast<Derived&>(*this); }

  bool VisitHeapImpl(RawHeap& heap) { return ToDerived().VisitHeap(heap); }
  bool VisitNormalPageSpaceImpl(NormalPageSpace& space) {
    return ToDerived().VisitNormalPageSpace(space);
  }
  bool VisitLargePageSpaceImpl(LargePageSpace& space) {
    return ToDerived().VisitLargePageSpace(space);
  }
  bool VisitNormalPageImpl(NormalPage& page) {
    return ToDerived().VisitNormalPage(page);
  }
  bool VisitLargePageImpl(LargePage& page) {
    return ToDerived().VisitLargePage(page);
  }
  bool VisitHeapObjectHeaderImpl(HeapObjectHeader& header) {
    return ToDerived().VisitHeapObjectHeader(header);
  }
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_VISITOR_H_
