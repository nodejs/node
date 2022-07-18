// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/promote-young-generation.h"

#include "src/heap/concurrent-marking.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/large-spaces.h"
#include "src/heap/mark-compact.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces-inl.h"

namespace v8 {
namespace internal {

void PromoteYoungGenerationGC::EvacuateYoungGeneration() {
  TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_FAST_PROMOTE);
  base::MutexGuard guard(heap_->relocation_mutex());
  // Young generation garbage collection is orthogonal from full GC marking. It
  // is possible that objects that are currently being processed for marking are
  // reclaimed in the young generation GC that interleaves concurrent marking.
  // Pause concurrent markers to allow processing them using
  // `UpdateMarkingWorklistAfterYoungGenGC()`.
  ConcurrentMarking::PauseScope pause_js_marking(heap_->concurrent_marking());
  CppHeap::PauseConcurrentMarkingScope pause_cpp_marking(
      CppHeap::From(heap_->cpp_heap()));
  if (!FLAG_concurrent_marking) {
    DCHECK(heap_->fast_promotion_mode_);
    DCHECK(heap_->CanPromoteYoungAndExpandOldGeneration(0));
  }

  NewSpace* new_space = heap_->new_space();
  // Move pages from new->old generation.
  PageRange range(new_space->first_allocatable_address(), new_space->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* p = (*++it)->prev_page();
    new_space->PromotePageToOldSpace(p);
    if (heap_->incremental_marking()->IsMarking())
      heap_->mark_compact_collector()->RecordLiveSlotsOnPage(p);
    p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
  }

  // Reset new space.
  new_space->EvacuatePrologue();
  if (!new_space->EnsureCurrentCapacity()) {
    V8::FatalProcessOutOfMemory(
        heap_->isolate(), "NewSpace::EnsureCurrentCapacity", V8::kHeapOOM);
  }
  new_space->EvacuateEpilogue();

  for (auto it = heap_->new_lo_space()->begin();
       it != heap_->new_lo_space()->end();) {
    LargePage* page = *it;
    // Increment has to happen after we save the page, because it is going to
    // be removed below.
    it++;
    heap_->lo_space()->PromoteNewLargeObject(page);
  }

  // Fix up special trackers.
  heap_->external_string_table_.PromoteYoung();
  // GlobalHandles are updated in PostGarbageCollectionProcessing

  size_t promoted = heap_->new_space()->Size() + heap_->new_lo_space()->Size();
  heap_->IncrementYoungSurvivorsCounter(promoted);
  heap_->IncrementPromotedObjectsSize(promoted);
  heap_->IncrementSemiSpaceCopiedObjectSize(0);
}

}  // namespace internal
}  // namespace v8
