// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/sweeper.h"

#include <vector>

#include "src/heap/cppgc/free-list.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/object-start-bitmap-inl.h"
#include "src/heap/cppgc/object-start-bitmap.h"
#include "src/heap/cppgc/raw-heap.h"
#include "src/heap/cppgc/sanitizers.h"

namespace cppgc {
namespace internal {

namespace {

class ObjectStartBitmapVerifier
    : private HeapVisitor<ObjectStartBitmapVerifier> {
  friend class HeapVisitor<ObjectStartBitmapVerifier>;

 public:
  void Verify(RawHeap* heap) { Traverse(heap); }

 private:
  bool VisitNormalPage(NormalPage* page) {
    // Remember bitmap and reset previous pointer.
    bitmap_ = &page->object_start_bitmap();
    prev_ = nullptr;
    return false;
  }

  bool VisitHeapObjectHeader(HeapObjectHeader* header) {
    if (header->IsLargeObject()) return true;

    auto* raw_header = reinterpret_cast<ConstAddress>(header);
    CHECK(bitmap_->CheckBit(raw_header));
    if (prev_) {
      CHECK_EQ(prev_, bitmap_->FindHeader(raw_header - 1));
    }
    prev_ = header;
    return true;
  }

  ObjectStartBitmap* bitmap_ = nullptr;
  HeapObjectHeader* prev_ = nullptr;
};

struct SpaceState {
  BaseSpace::Pages unswept_pages;
};
using SpaceStates = std::vector<SpaceState>;

bool SweepNormalPage(NormalPage* page) {
  constexpr auto kAtomicAccess = HeapObjectHeader::AccessMode::kAtomic;

  auto* space = NormalPageSpace::From(page->space());
  ObjectStartBitmap& bitmap = page->object_start_bitmap();
  bitmap.Clear();

  Address start_of_gap = page->PayloadStart();
  for (Address begin = page->PayloadStart(), end = page->PayloadEnd();
       begin != end;) {
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(begin);
    const size_t size = header->GetSize();
    // Check if this is a free list entry.
    if (header->IsFree<kAtomicAccess>()) {
      SET_MEMORY_INACCESIBLE(header, std::min(kFreeListEntrySize, size));
      begin += size;
      continue;
    }
    // Check if object is not marked (not reachable).
    if (!header->IsMarked<kAtomicAccess>()) {
      header->Finalize();
      SET_MEMORY_INACCESIBLE(header, size);
      begin += size;
      continue;
    }
    // The object is alive.
    const Address header_address = reinterpret_cast<Address>(header);
    if (start_of_gap != header_address) {
      space->AddToFreeList(start_of_gap,
                           static_cast<size_t>(header_address - start_of_gap));
    }
    header->Unmark<kAtomicAccess>();
    bitmap.SetBit(begin);
    begin += size;
    start_of_gap = begin;
  }

  if (start_of_gap != page->PayloadStart() &&
      start_of_gap != page->PayloadEnd()) {
    space->AddToFreeList(
        start_of_gap, static_cast<size_t>(page->PayloadEnd() - start_of_gap));
  }

  const bool is_empty = (start_of_gap == page->PayloadStart());
  return is_empty;
}

// This visitor:
// - resets linear allocation buffers and clears free lists for all spaces;
// - moves all Heap pages to local Sweeper's state (SpaceStates).
class PrepareForSweepVisitor final
    : public HeapVisitor<PrepareForSweepVisitor> {
 public:
  explicit PrepareForSweepVisitor(SpaceStates* states) : states_(states) {}

  bool VisitNormalPageSpace(NormalPageSpace* space) {
    space->ResetLinearAllocationBuffer();
    space->free_list().Clear();
    (*states_)[space->index()].unswept_pages = space->RemoveAllPages();
    return true;
  }

  bool VisitLargePageSpace(LargePageSpace* space) {
    (*states_)[space->index()].unswept_pages = space->RemoveAllPages();

    return true;
  }

 private:
  SpaceStates* states_;
};

class MutatorThreadSweepVisitor final
    : private HeapVisitor<MutatorThreadSweepVisitor> {
  friend class HeapVisitor<MutatorThreadSweepVisitor>;

 public:
  explicit MutatorThreadSweepVisitor(SpaceStates* space_states) {
    for (SpaceState& state : *space_states) {
      for (BasePage* page : state.unswept_pages) {
        Traverse(page);
      }
      state.unswept_pages.clear();
    }
  }

 private:
  bool VisitNormalPage(NormalPage* page) {
    const bool is_empty = SweepNormalPage(page);
    if (is_empty) {
      NormalPage::Destroy(page);
    } else {
      page->space()->AddPage(page);
    }
    return true;
  }

  bool VisitLargePage(LargePage* page) {
    if (page->ObjectHeader()->IsMarked()) {
      page->space()->AddPage(page);
    } else {
      page->ObjectHeader()->Finalize();
      LargePage::Destroy(page);
    }
    return true;
  }
};

}  // namespace

class Sweeper::SweeperImpl final {
 public:
  explicit SweeperImpl(RawHeap* heap) : heap_(heap) {
    space_states_.resize(heap_->size());
  }

  void Start(Config config) {
    is_in_progress_ = true;
#if DEBUG
    ObjectStartBitmapVerifier().Verify(heap_);
#endif
    PrepareForSweepVisitor(&space_states_).Traverse(heap_);
    if (config == Config::kAtomic) {
      Finish();
    } else {
      DCHECK_EQ(Config::kIncrementalAndConcurrent, config);
      // TODO(chromium:1056170): Schedule concurrent sweeping.
    }
  }

  void Finish() {
    if (!is_in_progress_) return;

    MutatorThreadSweepVisitor s(&space_states_);

    is_in_progress_ = false;
  }

 private:
  SpaceStates space_states_;
  RawHeap* heap_;
  bool is_in_progress_ = false;
};

Sweeper::Sweeper(RawHeap* heap) : impl_(std::make_unique<SweeperImpl>(heap)) {}
Sweeper::~Sweeper() = default;

void Sweeper::Start(Config config) { impl_->Start(config); }
void Sweeper::Finish() { impl_->Finish(); }

}  // namespace internal
}  // namespace cppgc
