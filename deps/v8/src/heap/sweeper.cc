// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/sweeper.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "src/base/atomic-utils.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/code-object-registry.h"
#include "src/heap/free-list-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/marking-state.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/pretenuring-handler-inl.h"
#include "src/heap/pretenuring-handler.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

class Sweeper::ConcurrentSweeper final {
 public:
  explicit ConcurrentSweeper(Sweeper* sweeper)
      : sweeper_(sweeper), local_sweeper_(sweeper_) {}

  bool ConcurrentSweepSpace(AllocationSpace identity, JobDelegate* delegate) {
    DCHECK(IsValidSweepingSpace(identity));
    while (!delegate->ShouldYield()) {
      Page* page = sweeper_->GetSweepingPageSafe(identity);
      if (page == nullptr) return true;
      local_sweeper_.ParallelSweepPage(page, identity,
                                       SweepingMode::kLazyOrConcurrent);
    }
    return false;
  }

  bool ConcurrentSweepForRememberedSet(JobDelegate* delegate) {
    if (!sweeper_->should_iterate_promoted_pages_) {
      local_sweeper_.CleanPromotedPages();
      return true;
    }
    while (!delegate->ShouldYield()) {
      MemoryChunk* chunk = sweeper_->GetPromotedPageForIterationSafe();
      if (chunk == nullptr) return true;
      local_sweeper_.ParallelIteratePromotedPageForRememberedSets(chunk);
    }
    return false;
  }

  void Finalize() { local_sweeper_.Finalize(); }

 private:
  Sweeper* const sweeper_;
  LocalSweeper local_sweeper_;
};

class Sweeper::SweeperJob final : public JobTask {
 public:
  SweeperJob(Isolate* isolate, Sweeper* sweeper)
      : sweeper_(sweeper),
        concurrent_sweepers_(sweeper_->concurrent_sweepers_),
        tracer_(isolate->heap()->tracer()) {}

  ~SweeperJob() override = default;

  SweeperJob(const SweeperJob&) = delete;
  SweeperJob& operator=(const SweeperJob&) = delete;

  void Run(JobDelegate* delegate) final {
    RwxMemoryWriteScope::SetDefaultPermissionsForNewThread();
    DCHECK(sweeper_->current_new_space_collector_.has_value());
    RunImpl(delegate, delegate->IsJoiningThread());
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    const size_t kPagePerTask = 2;
    return std::min<size_t>(
        concurrent_sweepers_.size(),
        worker_count +
            (sweeper_->ConcurrentSweepingPageCount() + kPagePerTask - 1) /
                kPagePerTask);
  }

 private:
  void RunImpl(JobDelegate* delegate, bool is_joining_thread) {
    static_assert(NEW_SPACE == FIRST_SWEEPABLE_SPACE);
    const int offset = delegate->GetTaskId();
    DCHECK_LT(offset, concurrent_sweepers_.size());
    ConcurrentSweeper& concurrent_sweeper = concurrent_sweepers_[offset];
    {
      TRACE_GC_EPOCH(
          tracer_, sweeper_->GetTracingScope(NEW_SPACE, is_joining_thread),
          is_joining_thread ? ThreadKind::kMain : ThreadKind::kBackground);
      // Prioritize sweeping new space pages first. Young allocation are the
      // most prominent, so these pages are most likely to be needed soon.
      if (!concurrent_sweeper.ConcurrentSweepSpace(NEW_SPACE, delegate)) return;
      if (!sweeper_->should_sweep_non_new_spaces_) {
        // If only new space needs to be swept, iterate promoted pages and
        // return.
        concurrent_sweeper.ConcurrentSweepForRememberedSet(delegate);
        return;
      }
    }
    // When non-new space also require sweeping, minor sweeping (i.e. iterating
    // of promoted pages, which will be needed for the next minor GC) and major
    // sweeping (i.e. of non-new spaces) is interleaved within the same sweeping
    // tasks (to balance/mitigate further old allocations needing to sweep on
    // the main thread and minor GCs needing to iterate pages during complete
    // sweep). Each task starts sweeping from a different space (to reduce
    // contention), and moves on to the next space once the current space is
    // done, until all spaces are swept.
    if (offset > 0) {
      if (!SweepNonNewSpaces(concurrent_sweeper, delegate, is_joining_thread,
                             offset, kNumberOfSweepingSpaces))
        return;
    }
    {
      TRACE_GC_EPOCH(
          tracer_, sweeper_->GetTracingScope(NEW_SPACE, is_joining_thread),
          is_joining_thread ? ThreadKind::kMain : ThreadKind::kBackground);
      if (!concurrent_sweeper.ConcurrentSweepForRememberedSet(delegate)) return;
    }
    if (!SweepNonNewSpaces(concurrent_sweeper, delegate, is_joining_thread, 1,
                           offset == 0 ? kNumberOfSweepingSpaces : offset))
      return;
  }

  bool SweepNonNewSpaces(ConcurrentSweeper& concurrent_sweeper,
                         JobDelegate* delegate, bool is_joining_thread,
                         int first_space_index, int last_space_index) {
    DCHECK(sweeper_->should_sweep_non_new_spaces_);
    TRACE_GC_EPOCH(
        tracer_, sweeper_->GetTracingScope(OLD_SPACE, is_joining_thread),
        is_joining_thread ? ThreadKind::kMain : ThreadKind::kBackground);
    for (int i = first_space_index; i < last_space_index; i++) {
      const AllocationSpace space_id =
          static_cast<AllocationSpace>(FIRST_SWEEPABLE_SPACE + i);
      DCHECK_NE(NEW_SPACE, space_id);
      if (!concurrent_sweeper.ConcurrentSweepSpace(space_id, delegate))
        return false;
    }
    return true;
  }

  Sweeper* const sweeper_;
  std::vector<ConcurrentSweeper>& concurrent_sweepers_;
  GCTracer* const tracer_;
};

namespace {
void AssertMainThreadOrSharedMainThread(Heap* heap) {
  DCHECK(heap->IsMainThread() || (heap->IsSharedMainThread() &&
                                  !heap->isolate()->is_shared_space_isolate()));
}
}  // namespace

void Sweeper::LocalSweeper::Finalize() {
  AssertMainThreadOrSharedMainThread(sweeper_->heap_);
  for (auto it : old_to_new_remembered_sets_) {
    MemoryChunk* chunk = it.first;
    RememberedSet<OLD_TO_NEW>::MergeAndDelete(chunk, it.second);
  }
  old_to_new_remembered_sets_.clear();
}

void Sweeper::LocalSweeper::ContributeAndWaitForPromotedPagesIteration() {
  if (!sweeper_->sweeping_in_progress()) return;
  if (!sweeper_->IsIteratingPromotedPages()) return;
  ParallelIteratePromotedPagesForRememberedSets();
  base::MutexGuard guard(
      &sweeper_->promoted_pages_iteration_notification_mutex_);
  // Check again that iteration is not yet finished.
  if (!sweeper_->IsIteratingPromotedPages()) return;
  sweeper_->promoted_pages_iteration_notification_variable_.Wait(
      &sweeper_->promoted_pages_iteration_notification_mutex_);
}

int Sweeper::LocalSweeper::ParallelSweepSpace(AllocationSpace identity,
                                              SweepingMode sweeping_mode,
                                              int required_freed_bytes,
                                              int max_pages) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = sweeper_->GetSweepingPageSafe(identity)) != nullptr) {
    int freed = ParallelSweepPage(page, identity, sweeping_mode);
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

int Sweeper::LocalSweeper::ParallelSweepPage(Page* page,
                                             AllocationSpace identity,
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
    max_freed =
        sweeper_->RawSweep(page, free_space_treatment_mode, sweeping_mode);
    DCHECK(page->SweepingDone());
  }

  sweeper_->AddSweptPage(page, identity);

  return max_freed;
}

void Sweeper::LocalSweeper::ParallelIteratePromotedPagesForRememberedSets() {
  if (sweeper_->should_iterate_promoted_pages_) {
    MemoryChunk* chunk = nullptr;
    while ((chunk = sweeper_->GetPromotedPageForIterationSafe()) != nullptr) {
      ParallelIteratePromotedPageForRememberedSets(chunk);
    }
  } else {
    CleanPromotedPages();
  }
}

void Sweeper::LocalSweeper::ParallelIteratePromotedPageForRememberedSets(
    MemoryChunk* chunk) {
  DCHECK_NOT_NULL(chunk);
  base::MutexGuard guard(chunk->mutex());
  DCHECK(!chunk->SweepingDone());
  DCHECK_EQ(Page::ConcurrentSweepingState::kPending,
            chunk->concurrent_sweeping_state());
  chunk->set_concurrent_sweeping_state(
      Page::ConcurrentSweepingState::kInProgress);
  DCHECK(sweeper_->should_iterate_promoted_pages_);
  sweeper_->RawIteratePromotedPageForRememberedSets(
      chunk, &old_to_new_remembered_sets_);
  DCHECK(chunk->SweepingDone());
  sweeper_->IncrementAndNotifyPromotedPagesIterationFinishedIfNeeded();
}

void Sweeper::LocalSweeper::CleanPromotedPages() {
  DCHECK(!sweeper_->should_iterate_promoted_pages_);
  std::vector<MemoryChunk*> promoted_pages =
      sweeper_->GetAllPromotedPagesForIterationSafe();
  if (promoted_pages.empty()) return;
  for (MemoryChunk* chunk : promoted_pages) {
    sweeper_->marking_state_->ClearLiveness(chunk);
    chunk->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kDone);
  }
  DCHECK_EQ(0u, sweeper_->iterated_promoted_pages_count_);
  sweeper_->iterated_promoted_pages_count_ = promoted_pages.size();
  sweeper_->NotifyPromotedPagesIterationFinished();
}

Sweeper::Sweeper(Heap* heap)
    : heap_(heap),
      marking_state_(heap_->non_atomic_marking_state()),
      sweeping_in_progress_(false),
      should_reduce_memory_(false),
      main_thread_local_sweeper_(this) {}

Sweeper::~Sweeper() {
  DCHECK(concurrent_sweepers_.empty());
  DCHECK(main_thread_local_sweeper_.IsEmpty());
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

void Sweeper::SnapshotPageSets() {
  DCHECK(heap_->IsMainThread());
  // No mutex needed for the main thread.
  std::tie(snapshot_normal_pages_set_, snapshot_large_pages_set_) =
      heap_->memory_allocator()->SnapshotPageSetsUnsafe();
  if (heap_->isolate()->has_shared_space()) {
    Heap* shared_heap = heap_->isolate()->shared_space_isolate()->heap();
    if (shared_heap == heap_) {
      // Current heap is the shared heap, thus all relevant pages have already
      // been snapshotted and no lock is required.
      snapshot_shared_normal_pages_set_ = snapshot_normal_pages_set_;
      snapshot_shared_large_pages_set_ = snapshot_large_pages_set_;
    } else {
      std::tie(snapshot_shared_normal_pages_set_,
               snapshot_shared_large_pages_set_) =
          shared_heap->memory_allocator()->SnapshotPageSetsSafe();
    }
  }
}

void Sweeper::StartSweeping(GarbageCollector collector) {
  DCHECK(main_thread_local_sweeper_.IsEmpty());
  sweeping_in_progress_ = true;
  if (collector == GarbageCollector::MARK_COMPACTOR)
    should_sweep_non_new_spaces_ = true;
  current_new_space_collector_ = collector;
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

namespace {
bool ShouldUpdateRememberedSets(Heap* heap) {
  DCHECK_EQ(0, heap->new_lo_space()->Size());
  if (heap->new_space()->Size() > 0) {
    // Keep track of OLD_TO_NEW slots
    return true;
  }
  if (heap->isolate()->has_shared_space()) {
    // Keep track of OLD_TO_SHARED slots
    return true;
  }
  if (heap->ShouldZapGarbage()) {
    return true;
  }
  return false;
}
}  // namespace

void Sweeper::StartSweeperTasks() {
  DCHECK(current_new_space_collector_.has_value());
  DCHECK(!job_handle_ || !job_handle_->IsValid());
  if (promoted_pages_for_iteration_count_ > 0) {
    DCHECK(v8_flags.minor_mc);
    DCHECK_EQ(GarbageCollector::MINOR_MARK_COMPACTOR,
              current_new_space_collector_);

    DCHECK(snapshot_normal_pages_set_.empty());
    DCHECK(snapshot_large_pages_set_.empty());

    DCHECK(!promoted_page_iteration_in_progress_);
    should_iterate_promoted_pages_ = ShouldUpdateRememberedSets(heap_);
    if (should_iterate_promoted_pages_) {
      SnapshotPageSets();
    }
    promoted_page_iteration_in_progress_.store(true, std::memory_order_release);
  }
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
        std::make_unique<SweeperJob>(heap_->isolate(), this));
  }
}

Page* Sweeper::GetSweptPageSafe(PagedSpaceBase* space) {
  base::MutexGuard guard(&mutex_);
  SweptList& list = swept_list_[GetSweepSpaceIndex(space->identity())];
  Page* page = nullptr;
  if (!list.empty()) {
    page = list.back();
    list.pop_back();
  }
  if (list.empty()) {
    has_swept_pages_[GetSweepSpaceIndex(space->identity())].store(
        false, std::memory_order_release);
  }
  return page;
}

Sweeper::SweptList Sweeper::GetAllSweptPagesSafe(PagedSpaceBase* space) {
  base::MutexGuard guard(&mutex_);
  SweptList list;
  list.swap(swept_list_[GetSweepSpaceIndex(space->identity())]);
  has_swept_pages_[GetSweepSpaceIndex(space->identity())].store(
      false, std::memory_order_release);
  return list;
}

void Sweeper::FinalizeLocalSweepers() {
  DCHECK_EQ(promoted_pages_for_iteration_count_,
            iterated_promoted_pages_count_);
  main_thread_local_sweeper_.Finalize();

  for (ConcurrentSweeper& concurrent_sweeper : concurrent_sweepers_) {
    concurrent_sweeper.Finalize();
  }
}

void Sweeper::EnsureCompleted() {
  AssertMainThreadOrSharedMainThread(heap_);

  if (!sweeping_in_progress_) return;

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  if (should_sweep_non_new_spaces_) {
    TRACE_GC_EPOCH(heap_->tracer(), GCTracer::Scope::MC_COMPLETE_SWEEPING,
                   ThreadKind::kMain);
    ForAllSweepingSpaces([this](AllocationSpace space) {
      if (space == NEW_SPACE) return;
      main_thread_local_sweeper_.ParallelSweepSpace(
          space, SweepingMode::kLazyOrConcurrent, 0);
    });
  }
  TRACE_GC_EPOCH(heap_->tracer(), GetTracingScopeForCompleteYoungSweep(),
                 ThreadKind::kMain);
  main_thread_local_sweeper_.ParallelSweepSpace(
      NEW_SPACE, SweepingMode::kLazyOrConcurrent, 0);
  // Array buffer sweeper may have grabbed a page for iteration to contribute.
  // Wait until it has finished iterating.
  main_thread_local_sweeper_.ContributeAndWaitForPromotedPagesIteration();

  if (job_handle_ && job_handle_->IsValid()) job_handle_->Join();

  ForAllSweepingSpaces([this](AllocationSpace space) {
    CHECK(sweeping_list_[GetSweepSpaceIndex(space)].empty());
    DCHECK(IsSweepingDoneForSpace(space));
  });

  FinalizeLocalSweepers();
  DCHECK(main_thread_local_sweeper_.IsEmpty());

  concurrent_sweepers_.clear();

  current_new_space_collector_.reset();
  should_sweep_non_new_spaces_ = false;
  {
    base::MutexGuard guard(&promoted_pages_iteration_notification_mutex_);
    DCHECK_EQ(promoted_pages_for_iteration_count_,
              iterated_promoted_pages_count_);
    base::AsAtomicPtr(&promoted_pages_for_iteration_count_)
        ->store(0, std::memory_order_relaxed);
    iterated_promoted_pages_count_ = 0;
  }
  snapshot_large_pages_set_.clear();
  snapshot_normal_pages_set_.clear();
  sweeping_in_progress_ = false;
}

void Sweeper::PauseAndEnsureNewSpaceCompleted() {
  AssertMainThreadOrSharedMainThread(heap_);

  if (!sweeping_in_progress_) return;

  main_thread_local_sweeper_.ParallelSweepSpace(
      NEW_SPACE, SweepingMode::kLazyOrConcurrent, 0);
  // Array buffer sweeper may have grabbed a page for iteration to contribute.
  // Wait until it has finished iterating.
  main_thread_local_sweeper_.ContributeAndWaitForPromotedPagesIteration();

  if (job_handle_ && job_handle_->IsValid()) job_handle_->Cancel();

  CHECK(sweeping_list_[GetSweepSpaceIndex(NEW_SPACE)].empty());
  DCHECK(IsSweepingDoneForSpace(NEW_SPACE));
  CHECK(sweeping_list_for_promoted_page_iteration_.empty());

  FinalizeLocalSweepers();
  DCHECK(main_thread_local_sweeper_.IsEmpty());

  current_new_space_collector_.reset();
  DCHECK_EQ(promoted_pages_for_iteration_count_,
            iterated_promoted_pages_count_);
  base::AsAtomicPtr(&promoted_pages_for_iteration_count_)
      ->store(0, std::memory_order_relaxed);
  iterated_promoted_pages_count_ = 0;
  snapshot_large_pages_set_.clear();
  snapshot_normal_pages_set_.clear();
}

void Sweeper::DrainSweepingWorklistForSpace(AllocationSpace space) {
  if (!sweeping_in_progress_) return;
  main_thread_local_sweeper_.ParallelSweepSpace(
      space, SweepingMode::kLazyOrConcurrent, 0);
}

bool Sweeper::AreSweeperTasksRunning() {
  return job_handle_ && job_handle_->IsValid() && job_handle_->IsActive();
}

namespace {
// Atomically zap the specified area.
V8_INLINE void AtomicZapBlock(Address addr, size_t size_in_bytes) {
  static_assert(sizeof(Tagged_t) == kTaggedSize);
  static constexpr Tagged_t kZapTagged = static_cast<Tagged_t>(kZapValue);
  DCHECK(IsAligned(addr, kTaggedSize));
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  const size_t size_in_tagged = size_in_bytes / kTaggedSize;
  Tagged_t* current_addr = reinterpret_cast<Tagged_t*>(addr);
  for (size_t i = 0; i < size_in_tagged; ++i) {
    base::AsAtomicPtr(current_addr++)
        ->store(kZapTagged, std::memory_order_relaxed);
  }
}
}  // namespace

V8_INLINE size_t Sweeper::FreeAndProcessFreedMemory(
    Address free_start, Address free_end, Page* page, Space* space,
    FreeSpaceTreatmentMode free_space_treatment_mode) {
  CHECK_GT(free_end, free_start);
  size_t freed_bytes = 0;
  size_t size = static_cast<size_t>(free_end - free_start);
  if (free_space_treatment_mode == FreeSpaceTreatmentMode::kZapFreeSpace) {
    AtomicZapBlock(free_start, size);
  }
  page->heap()->CreateFillerObjectAtSweeper(free_start, static_cast<int>(size));
  freed_bytes = reinterpret_cast<PagedSpaceBase*>(space)->UnaccountedFree(
      free_start, size);
  if (should_reduce_memory_) page->DiscardUnusedMemory(free_start, size);

  return freed_bytes;
}

V8_INLINE void Sweeper::CleanupRememberedSetEntriesForFreedMemory(
    Address free_start, Address free_end, Page* page, bool record_free_ranges,
    TypedSlotSet::FreeRangesMap* free_ranges_map, SweepingMode sweeping_mode,
    InvalidatedSlotsCleanup* invalidated_old_to_new_cleanup,
    InvalidatedSlotsCleanup* invalidated_old_to_old_cleanup,
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
  invalidated_old_to_old_cleanup->Free(free_start, free_end);
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

int Sweeper::RawSweep(Page* p, FreeSpaceTreatmentMode free_space_treatment_mode,
                      SweepingMode sweeping_mode) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE ||
         space->identity() == SHARED_SPACE ||
         (space->identity() == NEW_SPACE && v8_flags.minor_mc));
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
  InvalidatedSlotsCleanup invalidated_old_to_old_cleanup =
      InvalidatedSlotsCleanup::NoCleanup(p);
  InvalidatedSlotsCleanup invalidated_old_to_shared_cleanup =
      InvalidatedSlotsCleanup::NoCleanup(p);
  if (sweeping_mode == SweepingMode::kEagerDuringGC) {
    invalidated_old_to_new_cleanup = InvalidatedSlotsCleanup::OldToNew(p);
    invalidated_old_to_old_cleanup = InvalidatedSlotsCleanup::OldToOld(p);
    invalidated_old_to_shared_cleanup = InvalidatedSlotsCleanup::OldToShared(p);
  }

  // The free ranges map is used for filtering typed slots.
  TypedSlotSet::FreeRangesMap free_ranges_map;

  // Iterate over the page using the live objects and free the memory before
  // the given live object.
  Address free_start = p->area_start();
  PtrComprCageBase cage_base(heap_->isolate());
  for (auto object_and_size :
       LiveObjectRange<kBlackObjects>(p, marking_state_->bitmap(p))) {
    HeapObject const object = object_and_size.first;
    if (code_object_registry) code_objects.push_back(object.address());
    DCHECK(marking_state_->IsMarked(object));
    Address free_end = object.address();
    if (free_end != free_start) {
      max_freed_bytes =
          std::max(max_freed_bytes,
                   FreeAndProcessFreedMemory(free_start, free_end, p, space,
                                             free_space_treatment_mode));
      CleanupRememberedSetEntriesForFreedMemory(
          free_start, free_end, p, record_free_ranges, &free_ranges_map,
          sweeping_mode, &invalidated_old_to_new_cleanup,
          &invalidated_old_to_old_cleanup, &invalidated_old_to_shared_cleanup);
    }
    Map map = object.map(cage_base, kAcquireLoad);
    DCHECK(MarkCompactCollector::IsMapOrForwarded(map));
    int size = ALIGN_TO_ALLOCATION_ALIGNMENT(object.SizeFromMap(map));
    live_bytes += size;
    free_start = free_end + size;

    if (active_system_pages_after_sweeping) {
      active_system_pages_after_sweeping->Add(
          free_end - p->address(), free_start - p->address(),
          MemoryAllocator::GetCommitPageSizeBits());
    }
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
        &invalidated_old_to_old_cleanup, &invalidated_old_to_shared_cleanup);
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

  if (code_object_registry) {
    code_object_registry->ReinitializeFrom(std::move(code_objects));
  }
  p->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kDone);

  return static_cast<int>(
      p->owner()->free_list()->GuaranteedAllocatable(max_freed_bytes));
}

namespace {

class PromotedPageRecordMigratedSlotVisitor
    : public ObjectVisitorWithCageBases {
 public:
  PromotedPageRecordMigratedSlotVisitor(
      Heap* heap,
      Sweeper::CachedOldToNewRememberedSets*
          snapshot_old_to_new_remembered_sets,
      const MemoryAllocator::NormalPagesSet& snapshot_normal_pages_set,
      const MemoryAllocator::LargePagesSet& snapshot_large_pages_set,
      const MemoryAllocator::NormalPagesSet& snapshot_shared_normal_pages_set,
      const MemoryAllocator::LargePagesSet& snapshot_shared_large_pages_set)
      : ObjectVisitorWithCageBases(heap->isolate()),
        heap_(heap),
        snapshot_old_to_new_remembered_sets_(
            snapshot_old_to_new_remembered_sets),
        snapshot_normal_pages_set_(snapshot_normal_pages_set),
        snapshot_large_pages_set_(snapshot_large_pages_set),
        snapshot_shared_normal_pages_set_(snapshot_shared_normal_pages_set),
        snapshot_shared_large_pages_set_(snapshot_shared_large_pages_set) {}

  inline void VisitPointer(HeapObject host, ObjectSlot p) final {
    DCHECK(!HasWeakHeapObjectTag(p.Relaxed_Load(cage_base())));
    RecordMigratedSlot(host,
                       MaybeObject::FromObject(p.Relaxed_Load(cage_base())),
                       p.address());
  }

  inline void VisitMapPointer(HeapObject host) final {
    VisitPointer(host, host.map_slot());
  }

  inline void VisitPointer(HeapObject host, MaybeObjectSlot p) final {
    DCHECK(!MapWord::IsPacked(p.Relaxed_Load(cage_base()).ptr()));
    RecordMigratedSlot(host, p.Relaxed_Load(cage_base()), p.address());
  }

  inline void VisitPointers(HeapObject host, ObjectSlot start,
                            ObjectSlot end) final {
    while (start < end) {
      VisitPointer(host, start);
      ++start;
    }
  }

  inline void VisitPointers(HeapObject host, MaybeObjectSlot start,
                            MaybeObjectSlot end) final {
    while (start < end) {
      VisitPointer(host, start);
      ++start;
    }
  }

  inline void VisitCodePointer(Code host, CodeObjectSlot slot) final {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    // This code is similar to the implementation of VisitPointer() modulo
    // new kind of slot.
    DCHECK(!HasWeakHeapObjectTag(slot.Relaxed_Load(code_cage_base())));
    Object code = slot.Relaxed_Load(code_cage_base());
    RecordMigratedSlot(host, MaybeObject::FromObject(code), slot.address());
  }

  inline void VisitEphemeron(HeapObject host, int index, ObjectSlot key,
                             ObjectSlot value) override {
    DCHECK(host.IsEphemeronHashTable());
    DCHECK(!Heap::InYoungGeneration(host));

    VisitPointer(host, value);
    VisitPointer(host, key);
  }

  void VisitCodeTarget(RelocInfo* rinfo) final { UNREACHABLE(); }
  void VisitEmbeddedPointer(RelocInfo* rinfo) final { UNREACHABLE(); }

  // Entries that are skipped for recording.
  inline void VisitExternalReference(RelocInfo* rinfo) final {}
  inline void VisitInternalReference(RelocInfo* rinfo) final {}
  inline void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                                   ExternalPointerTag tag) final {}

 protected:
  inline void RecordMigratedSlot(HeapObject host, MaybeObject value,
                                 Address slot) {
    DCHECK(!host.InWritableSharedSpace());
    DCHECK(!Heap::InYoungGeneration(host));
    DCHECK(!MemoryChunk::FromHeapObject(host)->SweepingDone());
    if (value->IsStrongOrWeak()) {
      RecordOldToNewMigratedSlot(host, value, slot);
      RecordOldToSharedMigratedSlot(host, value, slot);
    }
  }

  inline void RecordOldToNewMigratedSlot(HeapObject host, MaybeObject value,
                                         Address slot) {
    const MemoryChunk* value_chunk =
        MemoryAllocator::LookupChunkContainingAddress(
            snapshot_normal_pages_set_, snapshot_large_pages_set_, value.ptr());
    if (!value_chunk) return;
#ifdef THREAD_SANITIZER
    value_chunk->SynchronizedHeapLoad();
#endif  // THREAD_SANITIZER
    if (!value_chunk->InYoungGeneration()) return;
    MemoryChunk* host_chunk = MemoryChunk::FromHeapObject(host);
    if (snapshot_old_to_new_remembered_sets_->find(host_chunk) ==
        snapshot_old_to_new_remembered_sets_->end()) {
      snapshot_old_to_new_remembered_sets_->emplace(
          host_chunk, SlotSet::Allocate(host_chunk->buckets()));
    }
    RememberedSetOperations::Insert<AccessMode::NON_ATOMIC>(
        (*snapshot_old_to_new_remembered_sets_)[host_chunk], host_chunk, slot);
  }

  inline void RecordOldToSharedMigratedSlot(HeapObject host, MaybeObject value,
                                            Address slot) {
    const MemoryChunk* value_chunk =
        MemoryAllocator::LookupChunkContainingAddress(
            snapshot_shared_normal_pages_set_, snapshot_shared_large_pages_set_,
            value.ptr());
    if (!value_chunk) return;
#ifdef THREAD_SANITIZER
    value_chunk->SynchronizedHeapLoad();
#endif  // THREAD_SANITIZER
    if (!value_chunk->InWritableSharedSpace()) return;
    RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::ATOMIC>(
        MemoryChunk::FromHeapObject(host), slot);
  }

  Heap* const heap_;
  Sweeper::CachedOldToNewRememberedSets* snapshot_old_to_new_remembered_sets_;
  const MemoryAllocator::NormalPagesSet& snapshot_normal_pages_set_;
  const MemoryAllocator::LargePagesSet& snapshot_large_pages_set_;
  const MemoryAllocator::NormalPagesSet& snapshot_shared_normal_pages_set_;
  const MemoryAllocator::LargePagesSet& snapshot_shared_large_pages_set_;
};

inline void HandlePromotedObject(
    HeapObject object, NonAtomicMarkingState* marking_state,
    PtrComprCageBase cage_base,
    PromotedPageRecordMigratedSlotVisitor* record_visitor) {
  DCHECK(marking_state->IsMarked(object));
  DCHECK(!IsCodeSpaceObject(object));
  object.IterateFast(cage_base, record_visitor);
  if (object.IsJSArrayBuffer()) {
    JSArrayBuffer::cast(object).YoungMarkExtensionPromoted();
  }
}

inline void HandleFreeSpace(Address free_start, Address free_end, Heap* heap) {
  if (!heap->ShouldZapGarbage()) return;
  if (free_end == free_start) return;
  size_t size = static_cast<size_t>(free_end - free_start);
  DCHECK(
      heap->non_atomic_marking_state()
          ->bitmap(Page::FromAddress(free_start))
          ->AllBitsClearInRange(
              Page::FromAddress(free_start)->AddressToMarkbitIndex(free_start),
              Page::FromAddress(free_start)->AddressToMarkbitIndex(free_end)));
  AtomicZapBlock(free_start, size);
  heap->CreateFillerObjectAtSweeper(free_start, static_cast<int>(size));
}

}  // namespace

void Sweeper::RawIteratePromotedPageForRememberedSets(
    MemoryChunk* chunk,
    CachedOldToNewRememberedSets* old_to_new_remembered_sets) {
  DCHECK(v8_flags.minor_mc);
  DCHECK(chunk->owner_identity() == OLD_SPACE ||
         chunk->owner_identity() == LO_SPACE);
  DCHECK(!chunk->SweepingDone());
  DCHECK(!chunk->IsEvacuationCandidate());

  // Iterate over the page using the live objects and free the memory before
  // the given live object.
  PtrComprCageBase cage_base(heap_->isolate());
  PromotedPageRecordMigratedSlotVisitor record_visitor(
      heap_, old_to_new_remembered_sets, snapshot_normal_pages_set_,
      snapshot_large_pages_set_, snapshot_shared_normal_pages_set_,
      snapshot_shared_large_pages_set_);
  DCHECK(!heap_->incremental_marking()->IsMarking());
  if (chunk->IsLargePage()) {
    HandlePromotedObject(static_cast<LargePage*>(chunk)->GetObject(),
                         marking_state_, cage_base, &record_visitor);
  } else {
    PtrComprCageBase cage_base(chunk->heap()->isolate());
    Address free_start = chunk->area_start();
    for (auto object_and_size :
         LiveObjectRange<kBlackObjects>(chunk, marking_state_->bitmap(chunk))) {
      HeapObject object = object_and_size.first;
      HandlePromotedObject(object, marking_state_, cage_base, &record_visitor);
      Address free_end = object.address();
      HandleFreeSpace(free_start, free_end, heap_);
      Map map = object.map(cage_base, kAcquireLoad);
      int size = object.SizeFromMap(map);
      free_start = free_end + size;
    }
    HandleFreeSpace(free_start, chunk->area_end(), heap_);
  }
  marking_state_->ClearLiveness(chunk);
  chunk->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kDone);
}

bool Sweeper::IsIteratingPromotedPages() const {
  return promoted_page_iteration_in_progress_.load(std::memory_order_acquire);
}

void Sweeper::ContributeAndWaitForPromotedPagesIteration() {
  main_thread_local_sweeper_.ContributeAndWaitForPromotedPagesIteration();
}

void Sweeper::IncrementAndNotifyPromotedPagesIterationFinishedIfNeeded() {
  if (++iterated_promoted_pages_count_ < promoted_pages_for_iteration_count_)
    return;
  NotifyPromotedPagesIterationFinished();
}

void Sweeper::NotifyPromotedPagesIterationFinished() {
  DCHECK_EQ(iterated_promoted_pages_count_,
            promoted_pages_for_iteration_count_);
  base::MutexGuard guard(&promoted_pages_iteration_notification_mutex_);
  promoted_page_iteration_in_progress_.store(false, std::memory_order_release);
  promoted_pages_iteration_notification_variable_.NotifyAll();
}

size_t Sweeper::ConcurrentSweepingPageCount() {
  DCHECK(sweeping_in_progress());
  base::MutexGuard guard(&mutex_);
  base::MutexGuard promoted_pages_guard(&promoted_pages_iteration_mutex_);
  size_t promoted_pages_count =
      sweeping_list_for_promoted_page_iteration_.size();
  size_t count = should_iterate_promoted_pages_
                     ? promoted_pages_count
                     : std::min(static_cast<size_t>(1), promoted_pages_count);
  for (int i = 0; i < kNumberOfSweepingSpaces; i++) {
    count += sweeping_list_[i].size();
  }
  return count;
}

int Sweeper::ParallelSweepSpace(AllocationSpace identity,
                                SweepingMode sweeping_mode,
                                int required_freed_bytes, int max_pages) {
  DCHECK_IMPLIES(identity == NEW_SPACE, heap_->IsMainThread());
  return main_thread_local_sweeper_.ParallelSweepSpace(
      identity, sweeping_mode, required_freed_bytes, max_pages);
}

void Sweeper::EnsurePageIsSwept(Page* page) {
  AssertMainThreadOrSharedMainThread(heap_);
  if (!sweeping_in_progress() || page->SweepingDone()) return;
  AllocationSpace space = page->owner_identity();

  if (IsValidSweepingSpace(space)) {
    if (TryRemoveSweepingPageSafe(space, page)) {
      // Page was successfully removed and can now be swept.
      main_thread_local_sweeper_.ParallelSweepPage(
          page, space, SweepingMode::kLazyOrConcurrent);
    } else {
      // Some sweeper task already took ownership of that page, wait until
      // sweeping is finished.
      base::MutexGuard guard(&mutex_);
      while (!page->SweepingDone()) {
        cv_page_swept_.Wait(&mutex_);
      }
    }
  } else {
    DCHECK(page->InNewSpace() && !v8_flags.minor_mc);
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
  if (sweeping_list.empty()) {
    has_sweeping_work_[GetSweepSpaceIndex(space)].store(
        false, std::memory_order_release);
  }
  return true;
}

void Sweeper::AddPage(AllocationSpace space, Page* page,
                      Sweeper::AddPageMode mode, AccessMode mutex_mode) {
  DCHECK_NE(NEW_SPACE, space);
  AddPageImpl(space, page, mode, mutex_mode);
}

void Sweeper::AddNewSpacePage(Page* page, AccessMode mutex_mode) {
  DCHECK_EQ(NEW_SPACE, page->owner_identity());
  size_t live_bytes = marking_state_->live_bytes(page);
  heap_->IncrementNewSpaceSurvivingObjectSize(live_bytes);
  heap_->IncrementYoungSurvivorsCounter(live_bytes);
  page->ClearWasUsedForAllocation();
  AddPageImpl(NEW_SPACE, page, AddPageMode::REGULAR, mutex_mode);
}

void Sweeper::AddPromotedPageForIteration(MemoryChunk* chunk) {
  DCHECK(heap_->IsMainThread());
  DCHECK(chunk->owner_identity() == OLD_SPACE ||
         chunk->owner_identity() == LO_SPACE);
  DCHECK_IMPLIES(v8_flags.concurrent_sweeping,
                 !job_handle_ || !job_handle_->IsValid());
  size_t live_bytes = marking_state_->live_bytes(chunk);
  DCHECK_GE(chunk->area_size(), live_bytes);
  heap_->IncrementPromotedObjectsSize(live_bytes);
  heap_->IncrementYoungSurvivorsCounter(live_bytes);
#if DEBUG
  if (!chunk->IsLargePage()) {
    static_cast<Page*>(chunk)->ForAllFreeListCategories(
        [chunk](FreeListCategory* category) {
          DCHECK(!category->is_linked(chunk->owner()->free_list()));
        });
  }
#endif  // DEBUG
  DCHECK_EQ(Page::ConcurrentSweepingState::kDone,
            chunk->concurrent_sweeping_state());
  chunk->set_concurrent_sweeping_state(Page::ConcurrentSweepingState::kPending);
  // This method is called only from the main thread while sweeping tasks have
  // not yet started, thus a mutex is not needed.
  sweeping_list_for_promoted_page_iteration_.push_back(chunk);
  promoted_pages_for_iteration_count_++;
}

void Sweeper::AddPageImpl(AllocationSpace space, Page* page,
                          Sweeper::AddPageMode mode, AccessMode mutex_mode) {
  base::Optional<base::MutexGuard> guard;
  if (mutex_mode == AccessMode::ATOMIC) {
    guard.emplace(&mutex_);
  } else {
    // This assert only checks that the non_atomic version is only used on the
    // main thread. It would not catch cases where main thread add a page
    // non-atomically while concurrent jobs are adding pages atomically.
    AssertMainThreadOrSharedMainThread(heap_);
  }
  DCHECK(IsValidSweepingSpace(space));
  DCHECK_IMPLIES(v8_flags.concurrent_sweeping,
                 !job_handle_ || !job_handle_->IsValid());
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
  has_sweeping_work_[GetSweepSpaceIndex(space)].store(
      true, std::memory_order_release);
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
  SweepingList& sweeping_list = sweeping_list_[space_index];
  if (!sweeping_list.empty()) {
    page = sweeping_list.back();
    sweeping_list.pop_back();
  }
  if (sweeping_list.empty()) {
    has_sweeping_work_[GetSweepSpaceIndex(space)].store(
        false, std::memory_order_release);
  }
  return page;
}

MemoryChunk* Sweeper::GetPromotedPageForIterationSafe() {
  base::MutexGuard guard(&promoted_pages_iteration_mutex_);
  MemoryChunk* chunk = nullptr;
  if (!sweeping_list_for_promoted_page_iteration_.empty()) {
    chunk = sweeping_list_for_promoted_page_iteration_.back();
    sweeping_list_for_promoted_page_iteration_.pop_back();
  }
  return chunk;
}

std::vector<MemoryChunk*> Sweeper::GetAllPromotedPagesForIterationSafe() {
  base::MutexGuard guard(&promoted_pages_iteration_mutex_);
  std::vector<MemoryChunk*> pages;
  pages.swap(sweeping_list_for_promoted_page_iteration_);
  DCHECK(sweeping_list_for_promoted_page_iteration_.empty());
  return pages;
}

GCTracer::Scope::ScopeId Sweeper::GetTracingScope(AllocationSpace space,
                                                  bool is_joining_thread) {
  if (space == NEW_SPACE &&
      current_new_space_collector_ == GarbageCollector::MINOR_MARK_COMPACTOR) {
    return is_joining_thread ? GCTracer::Scope::MINOR_MC_SWEEP
                             : GCTracer::Scope::MINOR_MC_BACKGROUND_SWEEPING;
  }
  return is_joining_thread ? GCTracer::Scope::MC_SWEEP
                           : GCTracer::Scope::MC_BACKGROUND_SWEEPING;
}

GCTracer::Scope::ScopeId Sweeper::GetTracingScopeForCompleteYoungSweep() {
  return current_new_space_collector_ == GarbageCollector::MINOR_MARK_COMPACTOR
             ? GCTracer::Scope::MINOR_MC_COMPLETE_SWEEPING
             : GCTracer::Scope::MC_COMPLETE_SWEEPING;
}

bool Sweeper::IsSweepingDoneForSpace(AllocationSpace space) const {
  return !has_sweeping_work_[GetSweepSpaceIndex(space)].load(
      std::memory_order_acquire);
}

void Sweeper::AddSweptPage(Page* page, AllocationSpace identity) {
  base::MutexGuard guard(&mutex_);
  swept_list_[GetSweepSpaceIndex(identity)].push_back(page);
  has_swept_pages_[GetSweepSpaceIndex(identity)].store(
      true, std::memory_order_release);
  cv_page_swept_.NotifyAll();
}

bool Sweeper::ShouldRefillFreelistForSpace(AllocationSpace space) const {
  DCHECK_IMPLIES(space == NEW_SPACE, v8_flags.minor_mc);
  return has_swept_pages_[GetSweepSpaceIndex(space)].load(
      std::memory_order_acquire);
}

void Sweeper::SweepEmptyNewSpacePage(Page* page) {
  DCHECK(v8_flags.minor_mc);
  DCHECK_EQ(NEW_SPACE, page->owner_identity());
  DCHECK_EQ(0, marking_state_->live_bytes(page));
  DCHECK(marking_state_->bitmap(page)->IsClean());
  DCHECK(heap_->IsMainThread() ||
         (heap_->IsSharedMainThread() &&
          !heap_->isolate()->is_shared_space_isolate()));
  DCHECK(heap_->tracer()->IsInAtomicPause());
  DCHECK_EQ(Page::ConcurrentSweepingState::kDone,
            page->concurrent_sweeping_state());

  PagedSpaceBase* paged_space =
      PagedNewSpace::From(heap_->new_space())->paged_space();

  Address start = page->area_start();
  size_t size = page->area_size();

  if (Heap::ShouldZapGarbage()) {
    static constexpr Tagged_t kZapTagged = static_cast<Tagged_t>(kZapValue);
    const size_t size_in_tagged = size / kTaggedSize;
    Tagged_t* current_addr = reinterpret_cast<Tagged_t*>(start);
    for (size_t i = 0; i < size_in_tagged; ++i) {
      base::AsAtomicPtr(current_addr++)
          ->store(kZapTagged, std::memory_order_relaxed);
    }
  }

  page->ClearWasUsedForAllocation();
  page->ResetAllocationStatistics();
  heap_->CreateFillerObjectAtSweeper(start, static_cast<int>(size));
  paged_space->UnaccountedFree(start, size);
  paged_space->IncreaseAllocatedBytes(0, page);
  paged_space->RelinkFreeListCategories(page);

  if (heap_->ShouldReduceMemory()) {
    page->DiscardUnusedMemory(start, size);
    // Only decrement counter when we discard unused system pages.
    ActiveSystemPages active_system_pages_after_sweeping;
    active_system_pages_after_sweeping.Init(
        MemoryChunkLayout::kMemoryChunkHeaderSize,
        MemoryAllocator::GetCommitPageSizeBits(), Page::kPageSize);
    // Decrement accounted memory for discarded memory.
    paged_space->ReduceActiveSystemPages(page,
                                         active_system_pages_after_sweeping);
  }
}

}  // namespace internal
}  // namespace v8
