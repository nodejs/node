// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/sweeper.h"

#include "src/execution/vm-state-inl.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/remembered-set.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

Sweeper::Sweeper(Heap* heap, MajorNonAtomicMarkingState* marking_state)
    : heap_(heap),
      marking_state_(marking_state),
      num_tasks_(0),
      pending_sweeper_tasks_semaphore_(0),
      incremental_sweeper_pending_(false),
      sweeping_in_progress_(false),
      num_sweeping_tasks_(0),
      stop_sweeper_tasks_(false),
      iterability_task_semaphore_(0),
      iterability_in_progress_(false),
      iterability_task_started_(false),
      should_reduce_memory_(false) {}

Sweeper::PauseOrCompleteScope::PauseOrCompleteScope(Sweeper* sweeper)
    : sweeper_(sweeper) {
  sweeper_->stop_sweeper_tasks_ = true;
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
  sweeper_->stop_sweeper_tasks_ = false;
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
              std::atomic<intptr_t>* num_sweeping_tasks,
              AllocationSpace space_to_start)
      : CancelableTask(isolate),
        sweeper_(sweeper),
        pending_sweeper_tasks_(pending_sweeper_tasks),
        num_sweeping_tasks_(num_sweeping_tasks),
        space_to_start_(space_to_start),
        tracer_(isolate->heap()->tracer()) {}

  ~SweeperTask() override = default;

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
    (*num_sweeping_tasks_)--;
    pending_sweeper_tasks_->Signal();
  }

  Sweeper* const sweeper_;
  base::Semaphore* const pending_sweeper_tasks_;
  std::atomic<intptr_t>* const num_sweeping_tasks_;
  AllocationSpace space_to_start_;
  GCTracer* const tracer_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};

class Sweeper::IncrementalSweeperTask final : public CancelableTask {
 public:
  IncrementalSweeperTask(Isolate* isolate, Sweeper* sweeper)
      : CancelableTask(isolate), isolate_(isolate), sweeper_(sweeper) {}

  ~IncrementalSweeperTask() override = default;

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
  CHECK(!stop_sweeper_tasks_);
  sweeping_in_progress_ = true;
  iterability_in_progress_ = true;
  should_reduce_memory_ = heap_->ShouldReduceMemory();
  MajorNonAtomicMarkingState* marking_state =
      heap_->mark_compact_collector()->non_atomic_marking_state();
  ForAllSweepingSpaces([this, marking_state](AllocationSpace space) {
    // Sorting is done in order to make compaction more efficient: by sweeping
    // pages with the most free bytes first, we make it more likely that when
    // evacuating a page, already swept pages will have enough free bytes to
    // hold the objects to move (and therefore, we won't need to wait for more
    // pages to be swept in order to move those objects).
    // Since maps don't move, there is no need to sort the pages from MAP_SPACE
    // before sweeping them.
    if (space != MAP_SPACE) {
      int space_index = GetSweepSpaceIndex(space);
      std::sort(
          sweeping_list_[space_index].begin(),
          sweeping_list_[space_index].end(), [marking_state](Page* a, Page* b) {
            return marking_state->live_bytes(a) > marking_state->live_bytes(b);
          });
    }
  });
}

void Sweeper::StartSweeperTasks() {
  DCHECK_EQ(0, num_tasks_);
  DCHECK_EQ(0, num_sweeping_tasks_);
  if (FLAG_concurrent_sweeping && sweeping_in_progress_ &&
      !heap_->delay_sweeper_tasks_for_testing_) {
    ForAllSweepingSpaces([this](AllocationSpace space) {
      DCHECK(IsValidSweepingSpace(space));
      num_sweeping_tasks_++;
      auto task = std::make_unique<SweeperTask>(
          heap_->isolate(), this, &pending_sweeper_tasks_semaphore_,
          &num_sweeping_tasks_, space);
      DCHECK_LT(num_tasks_, kMaxSweeperTasks);
      task_ids_[num_tasks_++] = task->id();
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    });
    ScheduleIncrementalSweepingTask();
  }
}

Page* Sweeper::GetSweptPageSafe(PagedSpace* space) {
  base::MutexGuard guard(&mutex_);
  SweptList& list = swept_list_[GetSweepSpaceIndex(space->identity())];
  if (!list.empty()) {
    auto last_page = list.back();
    list.pop_back();
    return last_page;
  }
  return nullptr;
}

void Sweeper::MergeOldToNewRememberedSetsForSweptPages() {
  base::MutexGuard guard(&mutex_);

  ForAllSweepingSpaces([this](AllocationSpace space) {
    SweptList& swept_list = swept_list_[GetSweepSpaceIndex(space)];
    for (Page* p : swept_list) p->MergeOldToNewRememberedSets();
  });
}

void Sweeper::AbortAndWaitForTasks() {
  if (!FLAG_concurrent_sweeping) return;

  for (int i = 0; i < num_tasks_; i++) {
    if (heap_->isolate()->cancelable_task_manager()->TryAbort(task_ids_[i]) !=
        TryAbortResult::kTaskAborted) {
      pending_sweeper_tasks_semaphore_.Wait();
    } else {
      // Aborted case.
      num_sweeping_tasks_--;
    }
  }
  num_tasks_ = 0;
  DCHECK_EQ(0, num_sweeping_tasks_);
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

bool Sweeper::AreSweeperTasksRunning() { return num_sweeping_tasks_ != 0; }

int Sweeper::RawSweep(
    Page* p, FreeListRebuildingMode free_list_mode,
    FreeSpaceTreatmentMode free_space_mode,
    FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space,
    const base::MutexGuard& page_guard) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(free_list_mode == IGNORE_FREE_LIST || space->identity() == OLD_SPACE ||
         space->identity() == CODE_SPACE || space->identity() == MAP_SPACE);
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());

  CodeObjectRegistry* code_object_registry = p->GetCodeObjectRegistry();

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
  InvalidatedSlotsCleanup old_to_new_cleanup =
      InvalidatedSlotsCleanup::NoCleanup(p);

  // Clean invalidated slots during the final atomic pause. After resuming
  // execution this isn't necessary, invalid old-to-new refs were already
  // removed by mark compact's update pointers phase.
  if (invalidated_slots_in_free_space ==
      FreeSpaceMayContainInvalidatedSlots::kYes)
    old_to_new_cleanup = InvalidatedSlotsCleanup::OldToNew(p);

  intptr_t live_bytes = 0;
  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;

  // Set the allocated_bytes_ counter to area_size and clear the wasted_memory_
  // counter. The free operations below will decrease allocated_bytes_ to actual
  // live bytes and keep track of wasted_memory_.
  p->ResetAllocationStatistics();

  if (code_object_registry) code_object_registry->Clear();

  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(p, marking_state_->bitmap(p))) {
    HeapObject const object = object_and_size.first;
    if (code_object_registry)
      code_object_registry->RegisterAlreadyExistingCodeObject(object.address());
    DCHECK(marking_state_->IsBlack(object));
    Address free_end = object.address();
    if (free_end != free_start) {
      CHECK_GT(free_end, free_start);
      size_t size = static_cast<size_t>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        ZapCode(free_start, size);
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
      if (should_reduce_memory_) p->DiscardUnusedMemory(free_start, size);
      RememberedSetSweeping::RemoveRange(p, free_start, free_end,
                                         SlotSet::KEEP_EMPTY_BUCKETS);
      RememberedSet<OLD_TO_OLD>::RemoveRange(p, free_start, free_end,
                                             SlotSet::KEEP_EMPTY_BUCKETS);
      if (non_empty_typed_slots) {
        free_ranges.insert(std::pair<uint32_t, uint32_t>(
            static_cast<uint32_t>(free_start - p->address()),
            static_cast<uint32_t>(free_end - p->address())));
      }

      old_to_new_cleanup.Free(free_start, free_end);
    }
    Map map = object.synchronized_map();
    int size = object.SizeFromMap(map);
    live_bytes += size;
    free_start = free_end + size;
  }

  if (free_start != p->area_end()) {
    CHECK_GT(p->area_end(), free_start);
    size_t size = static_cast<size_t>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      ZapCode(free_start, size);
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
    if (should_reduce_memory_) p->DiscardUnusedMemory(free_start, size);
    RememberedSetSweeping::RemoveRange(p, free_start, p->area_end(),
                                       SlotSet::KEEP_EMPTY_BUCKETS);
    RememberedSet<OLD_TO_OLD>::RemoveRange(p, free_start, p->area_end(),
                                           SlotSet::KEEP_EMPTY_BUCKETS);
    if (non_empty_typed_slots) {
      free_ranges.insert(std::pair<uint32_t, uint32_t>(
          static_cast<uint32_t>(free_start - p->address()),
          static_cast<uint32_t>(p->area_end() - p->address())));
    }

    old_to_new_cleanup.Free(free_start, p->area_end());
  }

  // Clear invalid typed slots after collection all free ranges.
  if (!free_ranges.empty()) {
    TypedSlotSet* old_to_new = p->typed_slot_set<OLD_TO_NEW>();
    if (old_to_new != nullptr) {
      old_to_new->ClearInvalidSlots(free_ranges);
    }
    TypedSlotSet* old_to_old = p->typed_slot_set<OLD_TO_OLD>();
    if (old_to_old != nullptr) {
      old_to_old->ClearInvalidSlots(free_ranges);
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
  p->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kDone);
  if (code_object_registry) code_object_registry->Finalize();
  if (free_list_mode == IGNORE_FREE_LIST) return 0;

  return static_cast<int>(
      p->free_list()->GuaranteedAllocatable(max_freed_bytes));
}

void Sweeper::SweepSpaceFromTask(AllocationSpace identity) {
  Page* page = nullptr;
  while (!stop_sweeper_tasks_ &&
         ((page = GetSweepingPageSafe(identity)) != nullptr)) {
    // Typed slot sets are only recorded on code pages. Code pages
    // are not swept concurrently to the application to ensure W^X.
    DCHECK(!page->typed_slot_set<OLD_TO_NEW>() &&
           !page->typed_slot_set<OLD_TO_OLD>());
    ParallelSweepPage(page, identity);
  }
}

bool Sweeper::SweepSpaceIncrementallyFromTask(AllocationSpace identity) {
  if (Page* page = GetSweepingPageSafe(identity)) {
    ParallelSweepPage(page, identity);
  }
  return sweeping_list_[GetSweepSpaceIndex(identity)].empty();
}

int Sweeper::ParallelSweepSpace(
    AllocationSpace identity, int required_freed_bytes, int max_pages,
    FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = GetSweepingPageSafe(identity)) != nullptr) {
    int freed =
        ParallelSweepPage(page, identity, invalidated_slots_in_free_space);
    ++pages_freed;
    if (page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // Free list of a never-allocate page will be dropped later on.
      continue;
    }
    DCHECK_GE(freed, 0);
    max_freed = Max(max_freed, freed);
    if ((required_freed_bytes) > 0 && (max_freed >= required_freed_bytes))
      return max_freed;
    if ((max_pages > 0) && (pages_freed >= max_pages)) return max_freed;
  }
  return max_freed;
}

int Sweeper::ParallelSweepPage(
    Page* page, AllocationSpace identity,
    FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space) {
  DCHECK(IsValidSweepingSpace(identity));

  // The Scavenger may add already swept pages back.
  if (page->SweepingDone()) return 0;

  int max_freed = 0;
  {
    base::MutexGuard guard(page->mutex());
    DCHECK(!page->SweepingDone());
    // If the page is a code page, the CodePageMemoryModificationScope changes
    // the page protection mode from rx -> rw while sweeping.
    CodePageMemoryModificationScope code_page_scope(page);

    DCHECK_EQ(Page::ConcurrentSweepingState::kPending,
              page->concurrent_sweeping_state());
    page->set_concurrent_sweeping_state(
        Page::ConcurrentSweepingState::kInProgress);
    const FreeSpaceTreatmentMode free_space_mode =
        Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
    max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode,
                         invalidated_slots_in_free_space, guard);
    DCHECK(page->SweepingDone());
  }

  {
    base::MutexGuard guard(&mutex_);
    swept_list_[GetSweepSpaceIndex(identity)].push_back(page);
  }
  return max_freed;
}

void Sweeper::ScheduleIncrementalSweepingTask() {
  if (!incremental_sweeper_pending_) {
    incremental_sweeper_pending_ = true;
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap_->isolate());
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
    taskrunner->PostTask(
        std::make_unique<IncrementalSweeperTask>(heap_->isolate(), this));
  }
}

void Sweeper::AddPage(AllocationSpace space, Page* page,
                      Sweeper::AddPageMode mode) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  DCHECK(!FLAG_concurrent_sweeping || !AreSweeperTasksRunning());
  if (mode == Sweeper::REGULAR) {
    PrepareToBeSweptPage(space, page);
  } else {
    // Page has been temporarily removed from the sweeper. Accounting already
    // happened when the page was initially added, so it is skipped here.
    DCHECK_EQ(Sweeper::READD_TEMPORARY_REMOVED_PAGE, mode);
  }
  DCHECK_EQ(Page::ConcurrentSweepingState::kPending,
            page->concurrent_sweeping_state());
  sweeping_list_[GetSweepSpaceIndex(space)].push_back(page);
}

void Sweeper::PrepareToBeSweptPage(AllocationSpace space, Page* page) {
#ifdef DEBUG
  DCHECK_GE(page->area_size(),
            static_cast<size_t>(marking_state_->live_bytes(page)));
  DCHECK_EQ(Page::ConcurrentSweepingState::kDone,
            page->concurrent_sweeping_state());
  page->ForAllFreeListCategories([page](FreeListCategory* category) {
    DCHECK(!category->is_linked(page->owner()->free_list()));
  });
#endif  // DEBUG
  page->MoveOldToNewRememberedSetForSweeping();
  page->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kPending);
  heap_->paged_space(space)->IncreaseAllocatedBytes(
      marking_state_->live_bytes(page), page);
}

Page* Sweeper::GetSweepingPageSafe(AllocationSpace space) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  int space_index = GetSweepSpaceIndex(space);
  Page* page = nullptr;
  if (!sweeping_list_[space_index].empty()) {
    page = sweeping_list_[space_index].back();
    sweeping_list_[space_index].pop_back();
  }
  return page;
}

void Sweeper::EnsureIterabilityCompleted() {
  if (!iterability_in_progress_) return;

  if (FLAG_concurrent_sweeping && iterability_task_started_) {
    if (heap_->isolate()->cancelable_task_manager()->TryAbort(
            iterability_task_id_) != TryAbortResult::kTaskAborted) {
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

  ~IterabilityTask() override = default;

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
    auto task = std::make_unique<IterabilityTask>(heap_->isolate(), this,
                                                  &iterability_task_semaphore_);
    iterability_task_id_ = task->id();
    iterability_task_started_ = true;
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  }
}

void Sweeper::AddPageForIterability(Page* page) {
  DCHECK(sweeping_in_progress_);
  DCHECK(iterability_in_progress_);
  DCHECK(!iterability_task_started_);
  DCHECK(IsValidIterabilitySpace(page->owner_identity()));
  DCHECK_EQ(Page::ConcurrentSweepingState::kDone,
            page->concurrent_sweeping_state());

  iterability_list_.push_back(page);
  page->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kPending);
}

void Sweeper::MakeIterable(Page* page) {
  base::MutexGuard guard(page->mutex());
  DCHECK(IsValidIterabilitySpace(page->owner_identity()));
  const FreeSpaceTreatmentMode free_space_mode =
      Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
  RawSweep(page, IGNORE_FREE_LIST, free_space_mode,
           FreeSpaceMayContainInvalidatedSlots::kNo, guard);
}

}  // namespace internal
}  // namespace v8
