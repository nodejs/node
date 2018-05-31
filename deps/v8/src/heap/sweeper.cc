// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/sweeper.h"

#include "src/base/template-utils.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/remembered-set.h"
#include "src/objects-inl.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

Sweeper::PauseOrCompleteScope::PauseOrCompleteScope(Sweeper* sweeper)
    : sweeper_(sweeper) {
  sweeper_->stop_sweeper_tasks_.SetValue(true);
  if (!sweeper_->sweeping_in_progress()) return;

  sweeper_->AbortAndWaitForTasks();

  // Complete sweeping if there's nothing more to do.
  if (sweeper_->IsDoneSweeping()) {
    sweeper_->heap_->mark_compact_collector()->EnsureSweepingCompleted();
    DCHECK(!sweeper_->sweeping_in_progress());
  } else {
    // Unless sweeping is complete the flag still indicates that the sweeper
    // is enabled. It just cannot use tasks anymore.
    DCHECK(sweeper_->sweeping_in_progress());
  }
}

Sweeper::PauseOrCompleteScope::~PauseOrCompleteScope() {
  sweeper_->stop_sweeper_tasks_.SetValue(false);
  if (!sweeper_->sweeping_in_progress()) return;

  sweeper_->StartSweeperTasks();
}

Sweeper::FilterSweepingPagesScope::FilterSweepingPagesScope(
    Sweeper* sweeper, const PauseOrCompleteScope& pause_or_complete_scope)
    : sweeper_(sweeper),
      pause_or_complete_scope_(pause_or_complete_scope),
      sweeping_in_progress_(sweeper_->sweeping_in_progress()) {
  USE(pause_or_complete_scope_);
  if (!sweeping_in_progress_) return;

  int old_space_index = GetSweepSpaceIndex(OLD_SPACE);
  old_space_sweeping_list_ =
      std::move(sweeper_->sweeping_list_[old_space_index]);
  sweeper_->sweeping_list_[old_space_index].clear();
}

Sweeper::FilterSweepingPagesScope::~FilterSweepingPagesScope() {
  DCHECK_EQ(sweeping_in_progress_, sweeper_->sweeping_in_progress());
  if (!sweeping_in_progress_) return;

  sweeper_->sweeping_list_[GetSweepSpaceIndex(OLD_SPACE)] =
      std::move(old_space_sweeping_list_);
  // old_space_sweeping_list_ does not need to be cleared as we don't use it.
}

class Sweeper::SweeperTask final : public CancelableTask {
 public:
  SweeperTask(Isolate* isolate, Sweeper* sweeper,
              base::Semaphore* pending_sweeper_tasks,
              base::AtomicNumber<intptr_t>* num_sweeping_tasks,
              AllocationSpace space_to_start)
      : CancelableTask(isolate),
        sweeper_(sweeper),
        pending_sweeper_tasks_(pending_sweeper_tasks),
        num_sweeping_tasks_(num_sweeping_tasks),
        space_to_start_(space_to_start),
        tracer_(isolate->heap()->tracer()) {}

  virtual ~SweeperTask() {}

 private:
  void RunInternal() final {
    TRACE_BACKGROUND_GC(tracer_,
                        GCTracer::BackgroundScope::MC_BACKGROUND_SWEEPING);
    DCHECK(IsValidSweepingSpace(space_to_start_));
    const int offset = space_to_start_ - FIRST_GROWABLE_PAGED_SPACE;
    for (int i = 0; i < kNumberOfSweepingSpaces; i++) {
      const AllocationSpace space_id = static_cast<AllocationSpace>(
          FIRST_GROWABLE_PAGED_SPACE +
          ((i + offset) % kNumberOfSweepingSpaces));
      // Do not sweep code space concurrently.
      if (space_id == CODE_SPACE) continue;
      DCHECK(IsValidSweepingSpace(space_id));
      sweeper_->SweepSpaceFromTask(space_id);
    }
    num_sweeping_tasks_->Decrement(1);
    pending_sweeper_tasks_->Signal();
  }

  Sweeper* const sweeper_;
  base::Semaphore* const pending_sweeper_tasks_;
  base::AtomicNumber<intptr_t>* const num_sweeping_tasks_;
  AllocationSpace space_to_start_;
  GCTracer* const tracer_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};

class Sweeper::IncrementalSweeperTask final : public CancelableTask {
 public:
  IncrementalSweeperTask(Isolate* isolate, Sweeper* sweeper)
      : CancelableTask(isolate), isolate_(isolate), sweeper_(sweeper) {}

  virtual ~IncrementalSweeperTask() {}

 private:
  void RunInternal() final {
    VMState<GC> state(isolate_);
    TRACE_EVENT_CALL_STATS_SCOPED(isolate_, "v8", "V8.Task");

    sweeper_->incremental_sweeper_pending_ = false;

    if (sweeper_->sweeping_in_progress()) {
      if (!sweeper_->SweepSpaceIncrementallyFromTask(CODE_SPACE)) {
        sweeper_->ScheduleIncrementalSweepingTask();
      }
    }
  }

  Isolate* const isolate_;
  Sweeper* const sweeper_;
  DISALLOW_COPY_AND_ASSIGN(IncrementalSweeperTask);
};

void Sweeper::StartSweeping() {
  CHECK(!stop_sweeper_tasks_.Value());
  sweeping_in_progress_ = true;
  iterability_in_progress_ = true;
  MajorNonAtomicMarkingState* marking_state =
      heap_->mark_compact_collector()->non_atomic_marking_state();
  ForAllSweepingSpaces([this, marking_state](AllocationSpace space) {
    int space_index = GetSweepSpaceIndex(space);
    std::sort(sweeping_list_[space_index].begin(),
              sweeping_list_[space_index].end(),
              [marking_state](Page* a, Page* b) {
                return marking_state->live_bytes(a) <
                       marking_state->live_bytes(b);
              });
  });
}

void Sweeper::StartSweeperTasks() {
  DCHECK_EQ(0, num_tasks_);
  DCHECK_EQ(0, num_sweeping_tasks_.Value());
  if (FLAG_concurrent_sweeping && sweeping_in_progress_ &&
      !heap_->delay_sweeper_tasks_for_testing_) {
    ForAllSweepingSpaces([this](AllocationSpace space) {
      DCHECK(IsValidSweepingSpace(space));
      num_sweeping_tasks_.Increment(1);
      auto task = base::make_unique<SweeperTask>(
          heap_->isolate(), this, &pending_sweeper_tasks_semaphore_,
          &num_sweeping_tasks_, space);
      DCHECK_LT(num_tasks_, kMaxSweeperTasks);
      task_ids_[num_tasks_++] = task->id();
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    });
    ScheduleIncrementalSweepingTask();
  }
}

void Sweeper::SweepOrWaitUntilSweepingCompleted(Page* page) {
  if (!page->SweepingDone()) {
    ParallelSweepPage(page, page->owner()->identity());
    if (!page->SweepingDone()) {
      // We were not able to sweep that page, i.e., a concurrent
      // sweeper thread currently owns this page. Wait for the sweeper
      // thread to be done with this page.
      page->WaitUntilSweepingCompleted();
    }
  }
}

Page* Sweeper::GetSweptPageSafe(PagedSpace* space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  SweptList& list = swept_list_[GetSweepSpaceIndex(space->identity())];
  if (!list.empty()) {
    auto last_page = list.back();
    list.pop_back();
    return last_page;
  }
  return nullptr;
}

void Sweeper::AbortAndWaitForTasks() {
  if (!FLAG_concurrent_sweeping) return;

  for (int i = 0; i < num_tasks_; i++) {
    if (heap_->isolate()->cancelable_task_manager()->TryAbort(task_ids_[i]) !=
        CancelableTaskManager::kTaskAborted) {
      pending_sweeper_tasks_semaphore_.Wait();
    } else {
      // Aborted case.
      num_sweeping_tasks_.Decrement(1);
    }
  }
  num_tasks_ = 0;
  DCHECK_EQ(0, num_sweeping_tasks_.Value());
}

void Sweeper::EnsureCompleted() {
  if (!sweeping_in_progress_) return;

  EnsureIterabilityCompleted();

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  ForAllSweepingSpaces(
      [this](AllocationSpace space) { ParallelSweepSpace(space, 0); });

  AbortAndWaitForTasks();

  ForAllSweepingSpaces([this](AllocationSpace space) {
    CHECK(sweeping_list_[GetSweepSpaceIndex(space)].empty());
  });
  sweeping_in_progress_ = false;
}

bool Sweeper::AreSweeperTasksRunning() {
  return num_sweeping_tasks_.Value() != 0;
}

int Sweeper::RawSweep(Page* p, FreeListRebuildingMode free_list_mode,
                      FreeSpaceTreatmentMode free_space_mode) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(free_list_mode == IGNORE_FREE_LIST || space->identity() == OLD_SPACE ||
         space->identity() == CODE_SPACE || space->identity() == MAP_SPACE);
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());

  // TODO(ulan): we don't have to clear type old-to-old slots in code space
  // because the concurrent marker doesn't mark code objects. This requires
  // the write barrier for code objects to check the color of the code object.
  bool non_empty_typed_slots = p->typed_slot_set<OLD_TO_NEW>() != nullptr ||
                               p->typed_slot_set<OLD_TO_OLD>() != nullptr;

  // The free ranges map is used for filtering typed slots.
  std::map<uint32_t, uint32_t> free_ranges;

  // Before we sweep objects on the page, we free dead array buffers which
  // requires valid mark bits.
  ArrayBufferTracker::FreeDead(p, marking_state_);

  Address free_start = p->area_start();
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize));

  // If we use the skip list for code space pages, we have to lock the skip
  // list because it could be accessed concurrently by the runtime or the
  // deoptimizer.
  const bool rebuild_skip_list =
      space->identity() == CODE_SPACE && p->skip_list() != nullptr;
  SkipList* skip_list = p->skip_list();
  if (rebuild_skip_list) {
    skip_list->Clear();
  }

  intptr_t live_bytes = 0;
  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;
  int curr_region = -1;

  // Set the allocated_bytes counter to area_size. The free operations below
  // will decrease the counter to actual live bytes.
  p->ResetAllocatedBytes();

  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(p, marking_state_->bitmap(p))) {
    HeapObject* const object = object_and_size.first;
    DCHECK(marking_state_->IsBlack(object));
    Address free_end = object->address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xCC, size);
      }
      if (free_list_mode == REBUILD_FREE_LIST) {
        freed_bytes = reinterpret_cast<PagedSpace*>(space)->Free(
            free_start, size, SpaceAccountingMode::kSpaceUnaccounted);
        max_freed_bytes = Max(freed_bytes, max_freed_bytes);
      } else {
        p->heap()->CreateFillerObjectAt(
            free_start, static_cast<int>(size), ClearRecordedSlots::kNo,
            ClearFreedMemoryMode::kClearFreedMemory);
      }
      RememberedSet<OLD_TO_NEW>::RemoveRange(p, free_start, free_end,
                                             SlotSet::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_OLD>::RemoveRange(p, free_start, free_end,
                                             SlotSet::KEEP_EMPTY_BUCKETS);
      if (non_empty_typed_slots) {
        free_ranges.insert(std::pair<uint32_t, uint32_t>(
            static_cast<uint32_t>(free_start - p->address()),
            static_cast<uint32_t>(free_end - p->address())));
      }
    }
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    live_bytes += size;
    if (rebuild_skip_list) {
      int new_region_start = SkipList::RegionNumber(free_end);
      int new_region_end =
          SkipList::RegionNumber(free_end + size - kPointerSize);
      if (new_region_start != curr_region || new_region_end != curr_region) {
        skip_list->AddObject(free_end, size);
        curr_region = new_region_end;
      }
    }
    free_start = free_end + size;
  }

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xCC, size);
    }
    if (free_list_mode == REBUILD_FREE_LIST) {
      freed_bytes = reinterpret_cast<PagedSpace*>(space)->Free(
          free_start, size, SpaceAccountingMode::kSpaceUnaccounted);
      max_freed_bytes = Max(freed_bytes, max_freed_bytes);
    } else {
      p->heap()->CreateFillerObjectAt(free_start, static_cast<int>(size),
                                      ClearRecordedSlots::kNo,
                                      ClearFreedMemoryMode::kClearFreedMemory);
    }

    RememberedSet<OLD_TO_NEW>::RemoveRange(p, free_start, p->area_end(),
                                           SlotSet::KEEP_EMPTY_BUCKETS);
    RememberedSet<OLD_TO_OLD>::RemoveRange(p, free_start, p->area_end(),
                                           SlotSet::KEEP_EMPTY_BUCKETS);
    if (non_empty_typed_slots) {
      free_ranges.insert(std::pair<uint32_t, uint32_t>(
          static_cast<uint32_t>(free_start - p->address()),
          static_cast<uint32_t>(p->area_end() - p->address())));
    }
  }

  // Clear invalid typed slots after collection all free ranges.
  if (!free_ranges.empty()) {
    TypedSlotSet* old_to_new = p->typed_slot_set<OLD_TO_NEW>();
    if (old_to_new != nullptr) {
      old_to_new->RemoveInvaldSlots(free_ranges);
    }
    TypedSlotSet* old_to_old = p->typed_slot_set<OLD_TO_OLD>();
    if (old_to_old != nullptr) {
      old_to_old->RemoveInvaldSlots(free_ranges);
    }
  }

  marking_state_->bitmap(p)->Clear();
  if (free_list_mode == IGNORE_FREE_LIST) {
    marking_state_->SetLiveBytes(p, 0);
    // We did not free memory, so have to adjust allocated bytes here.
    intptr_t freed_bytes = p->area_size() - live_bytes;
    p->DecreaseAllocatedBytes(freed_bytes);
  } else {
    // Keep the old live bytes counter of the page until RefillFreeList, where
    // the space size is refined.
    // The allocated_bytes() counter is precisely the total size of objects.
    DCHECK_EQ(live_bytes, p->allocated_bytes());
  }
  p->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
  if (free_list_mode == IGNORE_FREE_LIST) return 0;
  return static_cast<int>(FreeList::GuaranteedAllocatable(max_freed_bytes));
}

void Sweeper::SweepSpaceFromTask(AllocationSpace identity) {
  Page* page = nullptr;
  while (!stop_sweeper_tasks_.Value() &&
         ((page = GetSweepingPageSafe(identity)) != nullptr)) {
    ParallelSweepPage(page, identity);
  }
}

bool Sweeper::SweepSpaceIncrementallyFromTask(AllocationSpace identity) {
  if (Page* page = GetSweepingPageSafe(identity)) {
    ParallelSweepPage(page, identity);
  }
  return sweeping_list_[GetSweepSpaceIndex(identity)].empty();
}

int Sweeper::ParallelSweepSpace(AllocationSpace identity,
                                int required_freed_bytes, int max_pages) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = GetSweepingPageSafe(identity)) != nullptr) {
    int freed = ParallelSweepPage(page, identity);
    pages_freed += 1;
    DCHECK_GE(freed, 0);
    max_freed = Max(max_freed, freed);
    if ((required_freed_bytes) > 0 && (max_freed >= required_freed_bytes))
      return max_freed;
    if ((max_pages > 0) && (pages_freed >= max_pages)) return max_freed;
  }
  return max_freed;
}

int Sweeper::ParallelSweepPage(Page* page, AllocationSpace identity) {
  // Early bailout for pages that are swept outside of the regular sweeping
  // path. This check here avoids taking the lock first, avoiding deadlocks.
  if (page->SweepingDone()) return 0;

  DCHECK(IsValidSweepingSpace(identity));
  int max_freed = 0;
  {
    base::LockGuard<base::Mutex> guard(page->mutex());
    // If this page was already swept in the meantime, we can return here.
    if (page->SweepingDone()) return 0;

    // If the page is a code page, the CodePageMemoryModificationScope changes
    // the page protection mode from rx -> rw while sweeping.
    CodePageMemoryModificationScope code_page_scope(page);

    DCHECK_EQ(Page::kSweepingPending,
              page->concurrent_sweeping_state().Value());
    page->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
    const FreeSpaceTreatmentMode free_space_mode =
        Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
    max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode);
    DCHECK(page->SweepingDone());

    // After finishing sweeping of a page we clean up its remembered set.
    TypedSlotSet* typed_slot_set = page->typed_slot_set<OLD_TO_NEW>();
    if (typed_slot_set) {
      typed_slot_set->FreeToBeFreedChunks();
    }
    SlotSet* slot_set = page->slot_set<OLD_TO_NEW>();
    if (slot_set) {
      slot_set->FreeToBeFreedBuckets();
    }
  }

  {
    base::LockGuard<base::Mutex> guard(&mutex_);
    swept_list_[GetSweepSpaceIndex(identity)].push_back(page);
  }
  return max_freed;
}

void Sweeper::ScheduleIncrementalSweepingTask() {
  if (!incremental_sweeper_pending_) {
    incremental_sweeper_pending_ = true;
    IncrementalSweeperTask* task =
        new IncrementalSweeperTask(heap_->isolate(), this);
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap_->isolate());
    V8::GetCurrentPlatform()->CallOnForegroundThread(isolate, task);
  }
}

void Sweeper::AddPage(AllocationSpace space, Page* page,
                      Sweeper::AddPageMode mode) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  DCHECK(!FLAG_concurrent_sweeping || !AreSweeperTasksRunning());
  if (mode == Sweeper::REGULAR) {
    PrepareToBeSweptPage(space, page);
  } else {
    // Page has been temporarily removed from the sweeper. Accounting already
    // happened when the page was initially added, so it is skipped here.
    DCHECK_EQ(Sweeper::READD_TEMPORARY_REMOVED_PAGE, mode);
  }
  DCHECK_EQ(Page::kSweepingPending, page->concurrent_sweeping_state().Value());
  sweeping_list_[GetSweepSpaceIndex(space)].push_back(page);
}

void Sweeper::PrepareToBeSweptPage(AllocationSpace space, Page* page) {
  DCHECK_GE(page->area_size(),
            static_cast<size_t>(marking_state_->live_bytes(page)));
  DCHECK_EQ(Page::kSweepingDone, page->concurrent_sweeping_state().Value());
  page->ForAllFreeListCategories(
      [](FreeListCategory* category) { DCHECK(!category->is_linked()); });
  page->concurrent_sweeping_state().SetValue(Page::kSweepingPending);
  heap_->paged_space(space)->IncreaseAllocatedBytes(
      marking_state_->live_bytes(page), page);
}

Page* Sweeper::GetSweepingPageSafe(AllocationSpace space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  int space_index = GetSweepSpaceIndex(space);
  Page* page = nullptr;
  if (!sweeping_list_[space_index].empty()) {
    page = sweeping_list_[space_index].front();
    sweeping_list_[space_index].pop_front();
  }
  return page;
}

void Sweeper::EnsurePageIsIterable(Page* page) {
  AllocationSpace space = page->owner()->identity();
  if (IsValidSweepingSpace(space)) {
    SweepOrWaitUntilSweepingCompleted(page);
  } else {
    DCHECK(IsValidIterabilitySpace(space));
    EnsureIterabilityCompleted();
  }
}

void Sweeper::EnsureIterabilityCompleted() {
  if (!iterability_in_progress_) return;

  if (FLAG_concurrent_sweeping && iterability_task_started_) {
    if (heap_->isolate()->cancelable_task_manager()->TryAbort(
            iterability_task_id_) != CancelableTaskManager::kTaskAborted) {
      iterability_task_semaphore_.Wait();
    }
    iterability_task_started_ = false;
  }

  for (Page* page : iterability_list_) {
    MakeIterable(page);
  }
  iterability_list_.clear();
  iterability_in_progress_ = false;
}

class Sweeper::IterabilityTask final : public CancelableTask {
 public:
  IterabilityTask(Isolate* isolate, Sweeper* sweeper,
                  base::Semaphore* pending_iterability_task)
      : CancelableTask(isolate),
        sweeper_(sweeper),
        pending_iterability_task_(pending_iterability_task),
        tracer_(isolate->heap()->tracer()) {}

  virtual ~IterabilityTask() {}

 private:
  void RunInternal() final {
    TRACE_BACKGROUND_GC(tracer_,
                        GCTracer::BackgroundScope::MC_BACKGROUND_SWEEPING);
    for (Page* page : sweeper_->iterability_list_) {
      sweeper_->MakeIterable(page);
    }
    sweeper_->iterability_list_.clear();
    pending_iterability_task_->Signal();
  }

  Sweeper* const sweeper_;
  base::Semaphore* const pending_iterability_task_;
  GCTracer* const tracer_;

  DISALLOW_COPY_AND_ASSIGN(IterabilityTask);
};

void Sweeper::StartIterabilityTasks() {
  if (!iterability_in_progress_) return;

  DCHECK(!iterability_task_started_);
  if (FLAG_concurrent_sweeping && !iterability_list_.empty()) {
    auto task = base::make_unique<IterabilityTask>(
        heap_->isolate(), this, &iterability_task_semaphore_);
    iterability_task_id_ = task->id();
    iterability_task_started_ = true;
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  }
}

void Sweeper::AddPageForIterability(Page* page) {
  DCHECK(sweeping_in_progress_);
  DCHECK(iterability_in_progress_);
  DCHECK(!iterability_task_started_);
  DCHECK(IsValidIterabilitySpace(page->owner()->identity()));
  DCHECK_EQ(Page::kSweepingDone, page->concurrent_sweeping_state().Value());

  iterability_list_.push_back(page);
  page->concurrent_sweeping_state().SetValue(Page::kSweepingPending);
}

void Sweeper::MakeIterable(Page* page) {
  DCHECK(IsValidIterabilitySpace(page->owner()->identity()));
  const FreeSpaceTreatmentMode free_space_mode =
      Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
  RawSweep(page, IGNORE_FREE_LIST, free_space_mode);
}

}  // namespace internal
}  // namespace v8
