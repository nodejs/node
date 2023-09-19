// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SWEEPER_H_
#define V8_HEAP_SWEEPER_H_

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/optional.h"
#include "src/base/platform/condition-variable.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/pretenuring-handler.h"
#include "src/heap/slot-set.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class InvalidatedSlotsCleanup;
class MemoryChunk;
class NonAtomicMarkingState;
class Page;
class LargePage;
class PagedSpaceBase;
class Space;

enum class FreeSpaceTreatmentMode { kIgnoreFreeSpace, kZapFreeSpace };

class Sweeper {
 public:
  using SweepingList = std::vector<Page*>;
  using SweptList = std::vector<Page*>;
  using CachedOldToNewRememberedSets =
      std::unordered_map<MemoryChunk*, SlotSet*>;

  enum FreeListRebuildingMode { REBUILD_FREE_LIST, IGNORE_FREE_LIST };
  enum AddPageMode { REGULAR, READD_TEMPORARY_REMOVED_PAGE };
  enum class SweepingMode { kEagerDuringGC, kLazyOrConcurrent };

  // Pauses the sweeper tasks.
  class V8_NODISCARD PauseScope final {
   public:
    explicit PauseScope(Sweeper* sweeper);
    ~PauseScope();

   private:
    Sweeper* const sweeper_;
  };

  // Temporary filters old space sweeping lists. Requires the concurrent
  // sweeper to be paused. Allows for pages to be added to the sweeper while
  // in this scope. Note that the original list of sweeping pages is restored
  // after exiting this scope.
  class V8_NODISCARD FilterSweepingPagesScope final {
   public:
    FilterSweepingPagesScope(Sweeper* sweeper, const PauseScope& pause_scope);
    ~FilterSweepingPagesScope();

    template <typename Callback>
    void FilterOldSpaceSweepingPages(Callback callback) {
      if (!sweeping_in_progress_) return;

      SweepingList* sweeper_list =
          &sweeper_->sweeping_list_[GetSweepSpaceIndex(OLD_SPACE)];
      // Iteration here is from most free space to least free space.
      for (auto it = old_space_sweeping_list_.begin();
           it != old_space_sweeping_list_.end(); it++) {
        if (callback(*it)) {
          sweeper_list->push_back(*it);
        }
      }
    }

   private:
    Sweeper* const sweeper_;
    SweepingList old_space_sweeping_list_;
    bool sweeping_in_progress_;
  };

  // LocalSweeper holds local data structures required for sweeping and is used
  // to initiate sweeping and promoted page iteration on multiple threads. Each
  // thread should holds its own LocalSweeper. Once sweeping is done, all
  // LocalSweepers should be finalized on the main thread.
  //
  // LocalSweeper is not thread-safe and should not be concurrently by several
  // threads. The exceptions to this rule are allocations during parallel
  // evacuation and from concurrent allocators. In practice the data structures
  // in LocalSweeper are only actively used for new space sweeping. Since
  // parallel evacuators and concurrent allocators never try to allocate in new
  // space, they will never contribute to new space sweeping and thus can use
  // the main thread's local sweeper without risk of data races.
  class LocalSweeper final {
   public:
    explicit LocalSweeper(Sweeper* sweeper) : sweeper_(sweeper) {
      DCHECK_NOT_NULL(sweeper_);
    }
    ~LocalSweeper() { DCHECK(IsEmpty()); }

    int ParallelSweepSpace(AllocationSpace identity, SweepingMode sweeping_mode,
                           int required_freed_bytes, int max_pages = 0);
    void ContributeAndWaitForPromotedPagesIteration();
    void Finalize();

    bool IsEmpty() const { return old_to_new_remembered_sets_.empty(); }

   private:
    int ParallelSweepPage(Page* page, AllocationSpace identity,
                          SweepingMode sweeping_mode);

    void ParallelIteratePromotedPagesForRememberedSets();
    void ParallelIteratePromotedPageForRememberedSets(MemoryChunk* chunk);
    void CleanPromotedPages();

    Sweeper* const sweeper_;
    CachedOldToNewRememberedSets old_to_new_remembered_sets_;

    friend class Sweeper;
  };

  explicit Sweeper(Heap* heap);
  ~Sweeper();

  bool sweeping_in_progress() const { return sweeping_in_progress_; }

  void TearDown();

  void AddPage(AllocationSpace space, Page* page, AddPageMode mode,
               AccessMode mutex_mode = AccessMode::NON_ATOMIC);
  void AddNewSpacePage(Page* page,
                       AccessMode mutex_mode = AccessMode::NON_ATOMIC);
  void AddPromotedPageForIteration(MemoryChunk* chunk);

  int ParallelSweepSpace(AllocationSpace identity, SweepingMode sweeping_mode,
                         int required_freed_bytes, int max_pages = 0);

  void EnsurePageIsSwept(Page* page);

  // After calling this function sweeping is considered to be in progress
  // and the main thread can sweep lazily, but the background sweeper tasks
  // are not running yet.
  void StartSweeping(GarbageCollector collector);
  V8_EXPORT_PRIVATE void StartSweeperTasks();
  void EnsureCompleted();
  void PauseAndEnsureNewSpaceCompleted();
  void DrainSweepingWorklistForSpace(AllocationSpace space);
  bool AreSweeperTasksRunning();

  Page* GetSweptPageSafe(PagedSpaceBase* space);
  SweptList GetAllSweptPagesSafe(PagedSpaceBase* space);

  bool IsSweepingDoneForSpace(AllocationSpace space) const;

  GCTracer::Scope::ScopeId GetTracingScope(AllocationSpace space,
                                           bool is_joining_thread);
  GCTracer::Scope::ScopeId GetTracingScopeForCompleteYoungSweep();

  bool IsIteratingPromotedPages() const;
  void ContributeAndWaitForPromotedPagesIteration();

  bool ShouldRefillFreelistForSpace(AllocationSpace space) const;

  void SweepEmptyNewSpacePage(Page* page);

 private:
  NonAtomicMarkingState* marking_state() const { return marking_state_; }

  int RawSweep(Page* p, FreeSpaceTreatmentMode free_space_treatment_mode,
               SweepingMode sweeping_mode);

  void RawIteratePromotedPageForRememberedSets(
      MemoryChunk* chunk,
      CachedOldToNewRememberedSets* old_to_new_remembered_sets);

  void AddPageImpl(AllocationSpace space, Page* page, AddPageMode mode,
                   AccessMode mutex_mode);

  void FinalizeLocalSweepers();

  class ConcurrentSweeper;
  class SweeperJob;

  static const int kNumberOfSweepingSpaces =
      LAST_SWEEPABLE_SPACE - FIRST_SWEEPABLE_SPACE + 1;
  static constexpr int kMaxSweeperTasks = kNumberOfSweepingSpaces;

  template <typename Callback>
  void ForAllSweepingSpaces(Callback callback) const {
    if (v8_flags.minor_mc) {
      callback(NEW_SPACE);
    }
    callback(OLD_SPACE);
    callback(CODE_SPACE);
    callback(SHARED_SPACE);
  }

  // Helper function for RawSweep. Depending on the FreeListRebuildingMode and
  // FreeSpaceTreatmentMode this function may add the free memory to a free
  // list, make the memory iterable, clear it, and return the free memory to
  // the operating system.
  size_t FreeAndProcessFreedMemory(
      Address free_start, Address free_end, Page* page, Space* space,
      FreeSpaceTreatmentMode free_space_treatment_mode);

  // Helper function for RawSweep. Handle remembered set entries in the freed
  // memory which require clearing.
  void CleanupRememberedSetEntriesForFreedMemory(
      Address free_start, Address free_end, Page* page, bool record_free_ranges,
      TypedSlotSet::FreeRangesMap* free_ranges_map, SweepingMode sweeping_mode,
      InvalidatedSlotsCleanup* invalidated_old_to_new_cleanup,
      InvalidatedSlotsCleanup* invalidated_old_to_old_cleanup,
      InvalidatedSlotsCleanup* invalidated_old_to_shared_cleanup);

  // Helper function for RawSweep. Clears invalid typed slots in the given free
  // ranges.
  void CleanupTypedSlotsInFreeMemory(
      Page* page, const TypedSlotSet::FreeRangesMap& free_ranges_map,
      SweepingMode sweeping_mode);

  // Helper function for RawSweep. Clears the mark bits and ensures consistency
  // of live bytes.
  void ClearMarkBitsAndHandleLivenessStatistics(Page* page, size_t live_bytes);

  // Can only be called on the main thread when no tasks are running.
  bool IsDoneSweeping() const {
    bool is_done = true;
    ForAllSweepingSpaces([this, &is_done](AllocationSpace space) {
      DCHECK_EQ(IsSweepingDoneForSpace(space),
                sweeping_list_[GetSweepSpaceIndex(space)].empty());
      if (!sweeping_list_[GetSweepSpaceIndex(space)].empty()) is_done = false;
    });
    return is_done;
  }

  size_t ConcurrentSweepingPageCount();

  Page* GetSweepingPageSafe(AllocationSpace space);
  MemoryChunk* GetPromotedPageForIterationSafe();
  std::vector<MemoryChunk*> GetAllPromotedPagesForIterationSafe();
  bool TryRemoveSweepingPageSafe(AllocationSpace space, Page* page);

  void PrepareToBeSweptPage(AllocationSpace space, Page* page);

  static bool IsValidSweepingSpace(AllocationSpace space) {
    return space >= FIRST_SWEEPABLE_SPACE && space <= LAST_SWEEPABLE_SPACE;
  }

  static int GetSweepSpaceIndex(AllocationSpace space) {
    DCHECK(IsValidSweepingSpace(space));
    return space - FIRST_SWEEPABLE_SPACE;
  }

  int NumberOfConcurrentSweepers() const;

  void IncrementAndNotifyPromotedPagesIterationFinishedIfNeeded();
  void NotifyPromotedPagesIterationFinished();

  void SnapshotPageSets();

  void AddSweptPage(Page* page, AllocationSpace identity);

  Heap* const heap_;
  NonAtomicMarkingState* const marking_state_;
  std::unique_ptr<JobHandle> job_handle_;
  base::Mutex mutex_;
  base::Mutex promoted_pages_iteration_mutex_;
  base::ConditionVariable cv_page_swept_;
  SweptList swept_list_[kNumberOfSweepingSpaces];
  SweepingList sweeping_list_[kNumberOfSweepingSpaces];
  std::atomic<bool> has_sweeping_work_[kNumberOfSweepingSpaces]{false};
  std::atomic<bool> has_swept_pages_[kNumberOfSweepingSpaces]{false};
  std::vector<MemoryChunk*> sweeping_list_for_promoted_page_iteration_;
  std::vector<ConcurrentSweeper> concurrent_sweepers_;
  // Main thread can finalize sweeping, while background threads allocation slow
  // path checks this flag to see whether it could support concurrent sweeping.
  std::atomic<bool> sweeping_in_progress_;
  bool should_reduce_memory_;
  bool should_sweep_non_new_spaces_ = false;
  base::Optional<GarbageCollector> current_new_space_collector_;
  LocalSweeper main_thread_local_sweeper_;

  // The following fields are used for maintaining an order between iterating
  // promoted pages and sweeping array buffer extensions.
  size_t promoted_pages_for_iteration_count_ = 0;
  std::atomic<size_t> iterated_promoted_pages_count_{0};
  base::Mutex promoted_pages_iteration_notification_mutex_;
  base::ConditionVariable promoted_pages_iteration_notification_variable_;
  MemoryAllocator::NormalPagesSet snapshot_normal_pages_set_;
  MemoryAllocator::LargePagesSet snapshot_large_pages_set_;
  MemoryAllocator::NormalPagesSet snapshot_shared_normal_pages_set_;
  MemoryAllocator::LargePagesSet snapshot_shared_large_pages_set_;
  std::atomic<bool> promoted_page_iteration_in_progress_{false};
  bool should_iterate_promoted_pages_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SWEEPER_H_
