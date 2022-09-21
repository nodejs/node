// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SWEEPER_H_
#define V8_HEAP_SWEEPER_H_

#include <map>
#include <vector>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/semaphore.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/heap.h"
#include "src/heap/slot-set.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class InvalidatedSlotsCleanup;
class NonAtomicMarkingState;
class Page;
class PagedSpaceBase;
class Space;

enum class FreeSpaceTreatmentMode { kIgnoreFreeSpace, kZapFreeSpace };

class Sweeper {
 public:
  using SweepingList = std::vector<Page*>;
  using SweptList = std::vector<Page*>;

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

  enum FreeListRebuildingMode { REBUILD_FREE_LIST, IGNORE_FREE_LIST };
  enum AddPageMode { REGULAR, READD_TEMPORARY_REMOVED_PAGE };
  enum class SweepingMode { kEagerDuringGC, kLazyOrConcurrent };

  Sweeper(Heap* heap, NonAtomicMarkingState* marking_state);
  ~Sweeper();

  bool sweeping_in_progress() const { return sweeping_in_progress_; }

  void TearDown();

  void AddPage(AllocationSpace space, Page* page, AddPageMode mode);

  int ParallelSweepSpace(AllocationSpace identity, SweepingMode sweeping_mode,
                         int required_freed_bytes, int max_pages = 0);
  int ParallelSweepPage(
      Page* page, AllocationSpace identity,
      Heap::PretenuringFeedbackMap* local_pretenuring_feedback,
      SweepingMode sweeping_mode);

  void EnsurePageIsSwept(Page* page);

  int RawSweep(Page* p, FreeSpaceTreatmentMode free_space_treatment_mode,
               SweepingMode sweeping_mode, const base::MutexGuard& page_guard,
               Heap::PretenuringFeedbackMap* local_pretenuring_feedback);

  // After calling this function sweeping is considered to be in progress
  // and the main thread can sweep lazily, but the background sweeper tasks
  // are not running yet.
  void StartSweeping();
  V8_EXPORT_PRIVATE void StartSweeperTasks();
  void EnsureCompleted(
      SweepingMode sweeping_mode = SweepingMode::kLazyOrConcurrent);
  void DrainSweepingWorklistForSpace(AllocationSpace space);
  bool AreSweeperTasksRunning();

  // Support concurrent sweepers from main thread
  void SupportConcurrentSweeping();

  Page* GetSweptPageSafe(PagedSpaceBase* space);

  NonAtomicMarkingState* marking_state() const { return marking_state_; }

 private:
  class ConcurrentSweeper;
  class SweeperJob;

  static const int kNumberOfSweepingSpaces =
      LAST_SWEEPABLE_SPACE - FIRST_SWEEPABLE_SPACE + 1;
  static constexpr int kMaxSweeperTasks = 3;

  template <typename Callback>
  void ForAllSweepingSpaces(Callback callback) const {
    if (v8_flags.minor_mc) {
      callback(NEW_SPACE);
    }
    callback(OLD_SPACE);
    callback(CODE_SPACE);
    callback(MAP_SPACE);
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
      if (!sweeping_list_[GetSweepSpaceIndex(space)].empty()) is_done = false;
    });
    return is_done;
  }

  size_t ConcurrentSweepingPageCount();

  Page* GetSweepingPageSafe(AllocationSpace space);
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

  Heap* const heap_;
  NonAtomicMarkingState* marking_state_;
  std::unique_ptr<JobHandle> job_handle_;
  base::Mutex mutex_;
  base::ConditionVariable cv_page_swept_;
  SweptList swept_list_[kNumberOfSweepingSpaces];
  SweepingList sweeping_list_[kNumberOfSweepingSpaces];
  std::vector<ConcurrentSweeper> concurrent_sweepers_;
  // Main thread can finalize sweeping, while background threads allocation slow
  // path checks this flag to see whether it could support concurrent sweeping.
  std::atomic<bool> sweeping_in_progress_;
  bool should_reduce_memory_;
  Heap::PretenuringFeedbackMap local_pretenuring_feedback_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SWEEPER_H_
