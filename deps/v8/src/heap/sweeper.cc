// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/sweeper.h"

#include <memory>
#include <vector>

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/code-object-registry.h"
#include "src/heap/free-list-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/remembered-set.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

namespace {
static const int kInitialLocalPretenuringFeedbackCapacity = 256;
}  // namespace

class Sweeper::ConcurrentSweeper final {
 public:
  explicit ConcurrentSweeper(Sweeper* sweeper)
      : sweeper_(sweeper),
        local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity) {}

  bool ConcurrentSweepSpace(AllocationSpace identity, JobDelegate* delegate) {
    while (!delegate->ShouldYield()) {
      Page* page = sweeper_->GetSweepingPageSafe(identity);
      if (page == nullptr) return true;
      sweeper_->ParallelSweepPage(page, identity, &local_pretenuring_feedback_,
                                  SweepingMode::kLazyOrConcurrent);
    }
    return false;
  }

  Heap::PretenuringFeedbackMap* local_pretenuring_feedback() {
    return &local_pretenuring_feedback_;
  }

 private:
  Sweeper* const sweeper_;
  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;
};

class Sweeper::SweeperJob final : public JobTask {
 public:
  SweeperJob(Isolate* isolate, Sweeper* sweeper,
             std::vector<ConcurrentSweeper>* concurrent_sweepers)
      : sweeper_(sweeper),
        concurrent_sweepers_(concurrent_sweepers),
        tracer_(isolate->heap()->tracer()) {}

  ~SweeperJob() override = default;

  SweeperJob(const SweeperJob&) = delete;
  SweeperJob& operator=(const SweeperJob&) = delete;

  void Run(JobDelegate* delegate) final {
    RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
    if (delegate->IsJoiningThread()) {
      TRACE_GC(tracer_, GCTracer::Scope::MC_SWEEP);
      RunImpl(delegate);
    } else {
      TRACE_GC_EPOCH(tracer_, GCTracer::Scope::MC_BACKGROUND_SWEEPING,
                     ThreadKind::kBackground);
      RunImpl(delegate);
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    const size_t kPagePerTask = 2;
    return std::min<size_t>(
        concurrent_sweepers_->size(),
        worker_count +
            (sweeper_->ConcurrentSweepingPageCount() + kPagePerTask - 1) /
                kPagePerTask);
  }

 private:
  void RunImpl(JobDelegate* delegate) {
    const int offset = delegate->GetTaskId();
    DCHECK_LT(offset, concurrent_sweepers_->size());
    ConcurrentSweeper& sweeper = (*concurrent_sweepers_)[offset];
    for (int i = 0; i < kNumberOfSweepingSpaces; i++) {
      const AllocationSpace space_id = static_cast<AllocationSpace>(
          FIRST_SWEEPABLE_SPACE + ((i + offset) % kNumberOfSweepingSpaces));
      DCHECK(IsValidSweepingSpace(space_id));
      if (!sweeper.ConcurrentSweepSpace(space_id, delegate)) return;
    }
  }

  Sweeper* const sweeper_;
  std::vector<ConcurrentSweeper>* const concurrent_sweepers_;
  GCTracer* const tracer_;
};

Sweeper::Sweeper(Heap* heap, NonAtomicMarkingState* marking_state)
    : heap_(heap),
      marking_state_(marking_state),
      sweeping_in_progress_(false),
      should_reduce_memory_(false),
      local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity) {}

Sweeper::~Sweeper() {
  DCHECK(concurrent_sweepers_.empty());
  DCHECK(local_pretenuring_feedback_.empty());
}

Sweeper::PauseScope::PauseScope(Sweeper* sweeper) : sweeper_(sweeper) {
  if (!sweeper_->sweeping_in_progress()) return;

  if (sweeper_->job_handle_ && sweeper_->job_handle_->IsValid())
    sweeper_->job_handle_->Cancel();
}

Sweeper::PauseScope::~PauseScope() {
  if (!sweeper_->sweeping_in_progress()) return;

  sweeper_->StartSweeperTasks();
}

Sweeper::FilterSweepingPagesScope::FilterSweepingPagesScope(
    Sweeper* sweeper, const PauseScope& pause_scope)
    : sweeper_(sweeper),
      sweeping_in_progress_(sweeper_->sweeping_in_progress()) {
  // The PauseScope here only serves as a witness that concurrent sweeping has
  // been paused.
  USE(pause_scope);

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

void Sweeper::TearDown() {
  if (job_handle_ && job_handle_->IsValid()) job_handle_->Cancel();
}

void Sweeper::StartSweeping() {
  DCHECK(local_pretenuring_feedback_.empty());
  sweeping_in_progress_ = true;
  should_reduce_memory_ = heap_->ShouldReduceMemory();
  ForAllSweepingSpaces([this](AllocationSpace space) {
    // Sorting is done in order to make compaction more efficient: by sweeping
    // pages with the most free bytes first, we make it more likely that when
    // evacuating a page, already swept pages will have enough free bytes to
    // hold the objects to move (and therefore, we won't need to wait for more
    // pages to be swept in order to move those objects).
    // We sort in descending order of live bytes, i.e., ascending order of free
    // bytes, because GetSweepingPageSafe returns pages in reverse order.
    int space_index = GetSweepSpaceIndex(space);
    std::sort(
        sweeping_list_[space_index].begin(), sweeping_list_[space_index].end(),
        [marking_state = marking_state_](Page* a, Page* b) {
          return marking_state->live_bytes(a) > marking_state->live_bytes(b);
        });
  });
}

int Sweeper::NumberOfConcurrentSweepers() const {
  DCHECK(v8_flags.concurrent_sweeping);
  return std::min(Sweeper::kMaxSweeperTasks,
                  V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1);
}

void Sweeper::StartSweeperTasks() {
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  if (v8_flags.concurrent_sweeping && sweeping_in_progress_ &&
      !heap_->delay_sweeper_tasks_for_testing_) {
    if (concurrent_sweepers_.empty()) {
      for (int i = 0; i < NumberOfConcurrentSweepers(); ++i) {
        concurrent_sweepers_.emplace_back(this);
      }
    }
    DCHECK_EQ(NumberOfConcurrentSweepers(), concurrent_sweepers_.size());
    job_handle_ = V8::GetCurrentPlatform()->PostJob(
        TaskPriority::kUserVisible,
        std::make_unique<SweeperJob>(heap_->isolate(), this,
                                     &concurrent_sweepers_));
  }
}

Page* Sweeper::GetSweptPageSafe(PagedSpaceBase* space) {
  base::MutexGuard guard(&mutex_);
  SweptList& list = swept_list_[GetSweepSpaceIndex(space->identity())];
  if (!list.empty()) {
    auto last_page = list.back();
    list.pop_back();
    return last_page;
  }
  return nullptr;
}

void Sweeper::EnsureCompleted(SweepingMode sweeping_mode) {
  if (!sweeping_in_progress_) return;

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  ForAllSweepingSpaces([this, sweeping_mode](AllocationSpace space) {
    ParallelSweepSpace(space, sweeping_mode, 0);
  });

  if (job_handle_ && job_handle_->IsValid()) job_handle_->Join();

  ForAllSweepingSpaces([this](AllocationSpace space) {
    CHECK(sweeping_list_[GetSweepSpaceIndex(space)].empty());
  });

  heap_->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  for (ConcurrentSweeper& concurrent_sweeper : concurrent_sweepers_) {
    heap_->MergeAllocationSitePretenuringFeedback(
        *concurrent_sweeper.local_pretenuring_feedback());
  }
  local_pretenuring_feedback_.clear();
  concurrent_sweepers_.clear();

  sweeping_in_progress_ = false;
}

void Sweeper::DrainSweepingWorklistForSpace(AllocationSpace space) {
  if (!sweeping_in_progress_) return;
  ParallelSweepSpace(space, SweepingMode::kLazyOrConcurrent, 0);
}

void Sweeper::SupportConcurrentSweeping() {
  ForAllSweepingSpaces([this](AllocationSpace space) {
    const int kMaxPagesToSweepPerSpace = 1;
    ParallelSweepSpace(space, SweepingMode::kLazyOrConcurrent, 0,
                       kMaxPagesToSweepPerSpace);
  });
}

bool Sweeper::AreSweeperTasksRunning() {
  return job_handle_ && job_handle_->IsValid() && job_handle_->IsActive();
}

V8_INLINE size_t Sweeper::FreeAndProcessFreedMemory(
    Address free_start, Address free_end, Page* page, Space* space,
    FreeSpaceTreatmentMode free_space_treatment_mode) {
  CHECK_GT(free_end, free_start);
  size_t freed_bytes = 0;
  size_t size = static_cast<size_t>(free_end - free_start);
  if (free_space_treatment_mode == FreeSpaceTreatmentMode::kZapFreeSpace) {
    ZapCode(free_start, size);
  }
  page->heap()->CreateFillerObjectAtSweeper(free_start, static_cast<int>(size));
  freed_bytes =
      reinterpret_cast<PagedSpace*>(space)->UnaccountedFree(free_start, size);
  if (should_reduce_memory_) page->DiscardUnusedMemory(free_start, size);

  return freed_bytes;
}

V8_INLINE void Sweeper::CleanupRememberedSetEntriesForFreedMemory(
    Address free_start, Address free_end, Page* page, bool record_free_ranges,
    TypedSlotSet::FreeRangesMap* free_ranges_map, SweepingMode sweeping_mode,
    InvalidatedSlotsCleanup* invalidated_old_to_new_cleanup,
    InvalidatedSlotsCleanup* invalidated_old_to_shared_cleanup) {
  DCHECK_LE(free_start, free_end);
  if (sweeping_mode == SweepingMode::kEagerDuringGC) {
    // New space and in consequence the old-to-new remembered set is always
    // empty after a full GC, so we do not need to remove from it after the full
    // GC. However, we wouldn't even be allowed to do that, since the main
    // thread then owns the old-to-new remembered set. Removing from it from a
    // sweeper thread would race with the main thread.
    RememberedSet<OLD_TO_NEW>::RemoveRange(page, free_start, free_end,
                                           SlotSet::KEEP_EMPTY_BUCKETS);

    // While we only add old-to-old slots on live objects, we can still end up
    // with old-to-old slots in free memory with e.g. right-trimming of objects.
    RememberedSet<OLD_TO_OLD>::RemoveRange(page, free_start, free_end,
                                           SlotSet::KEEP_EMPTY_BUCKETS);
  } else {
    DCHECK_NULL(page->slot_set<OLD_TO_OLD>());
  }

  // Old-to-shared isn't reset after a full GC, so needs to be cleaned both
  // during and after a full GC.
  RememberedSet<OLD_TO_SHARED>::RemoveRange(page, free_start, free_end,
                                            SlotSet::KEEP_EMPTY_BUCKETS);

  if (record_free_ranges) {
    free_ranges_map->insert(std::pair<uint32_t, uint32_t>(
        static_cast<uint32_t>(free_start - page->address()),
        static_cast<uint32_t>(free_end - page->address())));
  }

  invalidated_old_to_new_cleanup->Free(free_start, free_end);
  invalidated_old_to_shared_cleanup->Free(free_start, free_end);
}

void Sweeper::CleanupTypedSlotsInFreeMemory(
    Page* page, const TypedSlotSet::FreeRangesMap& free_ranges_map,
    SweepingMode sweeping_mode) {
  if (sweeping_mode == SweepingMode::kEagerDuringGC) {
    page->ClearTypedSlotsInFreeMemory<OLD_TO_NEW>(free_ranges_map);

    // Typed old-to-old slot sets are only ever recorded in live code objects.
    // Also code objects are never right-trimmed, so there cannot be any slots
    // in a free range.
    page->AssertNoTypedSlotsInFreeMemory<OLD_TO_OLD>(free_ranges_map);

    page->ClearTypedSlotsInFreeMemory<OLD_TO_SHARED>(free_ranges_map);
    return;
  }

  DCHECK_EQ(sweeping_mode, SweepingMode::kLazyOrConcurrent);

  // After a full GC there are no old-to-new typed slots. The main thread
  // could create new slots but not in a free range.
  page->AssertNoTypedSlotsInFreeMemory<OLD_TO_NEW>(free_ranges_map);
  DCHECK_NULL(page->typed_slot_set<OLD_TO_OLD>());
  page->ClearTypedSlotsInFreeMemory<OLD_TO_SHARED>(free_ranges_map);
}

void Sweeper::ClearMarkBitsAndHandleLivenessStatistics(Page* page,
                                                       size_t live_bytes) {
  marking_state_->bitmap(page)->Clear();
  // Keep the old live bytes counter of the page until RefillFreeList, where
  // the space size is refined.
  // The allocated_bytes() counter is precisely the total size of objects.
  DCHECK_EQ(live_bytes, page->allocated_bytes());
}

int Sweeper::RawSweep(
    Page* p, FreeSpaceTreatmentMode free_space_treatment_mode,
    SweepingMode sweeping_mode, const base::MutexGuard& page_guard,
    Heap::PretenuringFeedbackMap* local_pretenuring_feedback) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE ||
         space->identity() == MAP_SPACE ||
         (space->identity() == NEW_SPACE && v8_flags.minor_mc));
  DCHECK_IMPLIES(space->identity() == NEW_SPACE,
                 sweeping_mode == SweepingMode::kEagerDuringGC);
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());

  // Phase 1: Prepare the page for sweeping.
  base::Optional<CodePageMemoryModificationScope> write_scope;
  if (space->identity() == CODE_SPACE) write_scope.emplace(p);

  // Set the allocated_bytes_ counter to area_size and clear the wasted_memory_
  // counter. The free operations below will decrease allocated_bytes_ to actual
  // live bytes and keep track of wasted_memory_.
  p->ResetAllocationStatistics();

  CodeObjectRegistry* code_object_registry = p->GetCodeObjectRegistry();
  std::vector<Address> code_objects;

  base::Optional<ActiveSystemPages> active_system_pages_after_sweeping;
  if (should_reduce_memory_) {
    // Only decrement counter when we discard unused system pages.
    active_system_pages_after_sweeping = ActiveSystemPages();
    active_system_pages_after_sweeping->Init(
        MemoryChunkLayout::kMemoryChunkHeaderSize,
        MemoryAllocator::GetCommitPageSizeBits(), Page::kPageSize);
  }

  // Phase 2: Free the non-live memory and clean-up the regular remembered set
  // entires.

  // Liveness and freeing statistics.
  size_t live_bytes = 0;
  size_t max_freed_bytes = 0;

  bool record_free_ranges = p->typed_slot_set<OLD_TO_NEW>() != nullptr ||
                            p->typed_slot_set<OLD_TO_OLD>() != nullptr ||
                            p->typed_slot_set<OLD_TO_SHARED>() != nullptr ||
                            DEBUG_BOOL;

  // Clean invalidated slots in free memory during the final atomic pause. After
  // resuming execution this isn't necessary, invalid slots were already removed
  // by mark compact's update pointers phase. So there are no invalid slots left
  // in free memory.
  InvalidatedSlotsCleanup invalidated_old_to_new_cleanup =
      InvalidatedSlotsCleanup::NoCleanup(p);
  InvalidatedSlotsCleanup invalidated_old_to_shared_cleanup =
      InvalidatedSlotsCleanup::NoCleanup(p);
  if (sweeping_mode == SweepingMode::kEagerDuringGC) {
    invalidated_old_to_new_cleanup = InvalidatedSlotsCleanup::OldToNew(p);
    invalidated_old_to_shared_cleanup = InvalidatedSlotsCleanup::OldToShared(p);
  }

  // The free ranges map is used for filtering typed slots.
  TypedSlotSet::FreeRangesMap free_ranges_map;

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  p->object_start_bitmap()->Clear();
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB

  // Iterate over the page using the live objects and free the memory before
  // the given live object.
  Address free_start = p->area_start();
  PtrComprCageBase cage_base(heap_->isolate());
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(p, marking_state_->bitmap(p))) {
    HeapObject const object = object_and_size.first;
    if (code_object_registry) code_objects.push_back(object.address());
    DCHECK(marking_state_->IsBlack(object));
    Address free_end = object.address();
    if (free_end != free_start) {
      max_freed_bytes =
          std::max(max_freed_bytes,
                   FreeAndProcessFreedMemory(free_start, free_end, p, space,
                                             free_space_treatment_mode));
      CleanupRememberedSetEntriesForFreedMemory(
          free_start, free_end, p, record_free_ranges, &free_ranges_map,
          sweeping_mode, &invalidated_old_to_new_cleanup,
          &invalidated_old_to_shared_cleanup);
    }
    Map map = object.map(cage_base, kAcquireLoad);
    DCHECK(MarkCompactCollector::IsMapOrForwarded(map));
    int size = object.SizeFromMap(map);
    live_bytes += size;
    free_start = free_end + size;

    if (p->InYoungGeneration()) {
      heap_->UpdateAllocationSite(map, object, local_pretenuring_feedback);
    }

    if (active_system_pages_after_sweeping) {
      active_system_pages_after_sweeping->Add(
          free_end - p->address(), free_start - p->address(),
          MemoryAllocator::GetCommitPageSizeBits());
    }

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
    p->object_start_bitmap()->SetBit(object.address());
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  }

  // If there is free memory after the last live object also free that.
  Address free_end = p->area_end();
  if (free_end != free_start) {
    max_freed_bytes =
        std::max(max_freed_bytes,
                 FreeAndProcessFreedMemory(free_start, free_end, p, space,
                                           free_space_treatment_mode));
    CleanupRememberedSetEntriesForFreedMemory(
        free_start, free_end, p, record_free_ranges, &free_ranges_map,
        sweeping_mode, &invalidated_old_to_new_cleanup,
        &invalidated_old_to_shared_cleanup);
  }

  // Phase 3: Post process the page.
  CleanupTypedSlotsInFreeMemory(p, free_ranges_map, sweeping_mode);
  ClearMarkBitsAndHandleLivenessStatistics(p, live_bytes);

  if (active_system_pages_after_sweeping) {
    // Decrement accounted memory for discarded memory.
    PagedSpaceBase* paged_space = static_cast<PagedSpaceBase*>(p->owner());
    paged_space->ReduceActiveSystemPages(p,
                                         *active_system_pages_after_sweeping);
  }

  if (code_object_registry)
    code_object_registry->ReinitializeFrom(std::move(code_objects));
  p->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kDone);

  return static_cast<int>(
      p->owner()->free_list()->GuaranteedAllocatable(max_freed_bytes));
}

size_t Sweeper::ConcurrentSweepingPageCount() {
  base::MutexGuard guard(&mutex_);
  return sweeping_list_[GetSweepSpaceIndex(OLD_SPACE)].size() +
         sweeping_list_[GetSweepSpaceIndex(MAP_SPACE)].size() +
         (v8_flags.minor_mc
              ? sweeping_list_[GetSweepSpaceIndex(NEW_SPACE)].size()
              : 0);
}

int Sweeper::ParallelSweepSpace(AllocationSpace identity,
                                SweepingMode sweeping_mode,
                                int required_freed_bytes, int max_pages) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = GetSweepingPageSafe(identity)) != nullptr) {
    int freed = ParallelSweepPage(page, identity, &local_pretenuring_feedback_,
                                  sweeping_mode);
    ++pages_freed;
    if (page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // Free list of a never-allocate page will be dropped later on.
      continue;
    }
    DCHECK_GE(freed, 0);
    max_freed = std::max(max_freed, freed);
    if ((required_freed_bytes) > 0 && (max_freed >= required_freed_bytes))
      return max_freed;
    if ((max_pages > 0) && (pages_freed >= max_pages)) return max_freed;
  }
  return max_freed;
}

int Sweeper::ParallelSweepPage(
    Page* page, AllocationSpace identity,
    Heap::PretenuringFeedbackMap* local_pretenuring_feedback,
    SweepingMode sweeping_mode) {
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
    const FreeSpaceTreatmentMode free_space_treatment_mode =
        Heap::ShouldZapGarbage() ? FreeSpaceTreatmentMode::kZapFreeSpace
                                 : FreeSpaceTreatmentMode::kIgnoreFreeSpace;
    max_freed = RawSweep(page, free_space_treatment_mode, sweeping_mode, guard,
                         local_pretenuring_feedback);
    DCHECK(page->SweepingDone());
  }

  {
    base::MutexGuard guard(&mutex_);
    swept_list_[GetSweepSpaceIndex(identity)].push_back(page);
    cv_page_swept_.NotifyAll();
  }
  return max_freed;
}

void Sweeper::EnsurePageIsSwept(Page* page) {
  if (!sweeping_in_progress() || page->SweepingDone()) return;
  AllocationSpace space = page->owner_identity();

  if (IsValidSweepingSpace(space)) {
    if (TryRemoveSweepingPageSafe(space, page)) {
      // Page was successfully removed and can now be swept.
      ParallelSweepPage(page, space, &local_pretenuring_feedback_,
                        SweepingMode::kLazyOrConcurrent);
    } else {
      // Some sweeper task already took ownership of that page, wait until
      // sweeping is finished.
      base::MutexGuard guard(&mutex_);
      while (!page->SweepingDone()) {
        cv_page_swept_.Wait(&mutex_);
      }
    }
  } else {
    DCHECK(page->InNewSpace());
  }

  CHECK(page->SweepingDone());
}

bool Sweeper::TryRemoveSweepingPageSafe(AllocationSpace space, Page* page) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  int space_index = GetSweepSpaceIndex(space);
  SweepingList& sweeping_list = sweeping_list_[space_index];
  SweepingList::iterator position =
      std::find(sweeping_list.begin(), sweeping_list.end(), page);
  if (position == sweeping_list.end()) return false;
  sweeping_list.erase(position);
  return true;
}

void Sweeper::AddPage(AllocationSpace space, Page* page,
                      Sweeper::AddPageMode mode) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsValidSweepingSpace(space));
  DCHECK(!v8_flags.concurrent_sweeping || !job_handle_ ||
         !job_handle_->IsValid());
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
  page->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kPending);
  PagedSpaceBase* paged_space;
  if (space == NEW_SPACE) {
    DCHECK(v8_flags.minor_mc);
    paged_space = heap_->paged_new_space()->paged_space();
  } else {
    paged_space = heap_->paged_space(space);
  }
  paged_space->IncreaseAllocatedBytes(marking_state_->live_bytes(page), page);
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

}  // namespace internal
}  // namespace v8
