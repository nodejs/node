// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SWEEPER_H_
#define V8_HEAP_SWEEPER_H_

#include <deque>
#include <vector>

#include "src/base/platform/semaphore.h"
#include "src/cancelable-task.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class MajorNonAtomicMarkingState;
class Page;
class PagedSpace;

enum FreeSpaceTreatmentMode { IGNORE_FREE_SPACE, ZAP_FREE_SPACE };

class Sweeper {
 public:
  typedef std::deque<Page*> SweepingList;
  typedef std::vector<Page*> SweptList;

  // Pauses the sweeper tasks or completes sweeping.
  class PauseOrCompleteScope final {
   public:
    explicit PauseOrCompleteScope(Sweeper* sweeper);
    ~PauseOrCompleteScope();

   private:
    Sweeper* const sweeper_;
  };

  // Temporary filters old space sweeping lists. Requires the concurrent
  // sweeper to be paused. Allows for pages to be added to the sweeper while
  // in this scope. Note that the original list of sweeping pages is restored
  // after exiting this scope.
  class FilterSweepingPagesScope final {
   public:
    explicit FilterSweepingPagesScope(
        Sweeper* sweeper, const PauseOrCompleteScope& pause_or_complete_scope);
    ~FilterSweepingPagesScope();

    template <typename Callback>
    void FilterOldSpaceSweepingPages(Callback callback) {
      if (!sweeping_in_progress_) return;

      SweepingList* sweeper_list = &sweeper_->sweeping_list_[OLD_SPACE];
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
    const PauseOrCompleteScope& pause_or_complete_scope_;
    bool sweeping_in_progress_;
  };

  enum FreeListRebuildingMode { REBUILD_FREE_LIST, IGNORE_FREE_LIST };
  enum ClearOldToNewSlotsMode {
    DO_NOT_CLEAR,
    CLEAR_REGULAR_SLOTS,
    CLEAR_TYPED_SLOTS
  };
  enum AddPageMode { REGULAR, READD_TEMPORARY_REMOVED_PAGE };

  Sweeper(Heap* heap, MajorNonAtomicMarkingState* marking_state)
      : heap_(heap),
        marking_state_(marking_state),
        num_tasks_(0),
        pending_sweeper_tasks_semaphore_(0),
        incremental_sweeper_pending_(false),
        sweeping_in_progress_(false),
        num_sweeping_tasks_(0),
        stop_sweeper_tasks_(false) {}

  bool sweeping_in_progress() const { return sweeping_in_progress_; }

  void AddPage(AllocationSpace space, Page* page, AddPageMode mode);

  int ParallelSweepSpace(AllocationSpace identity, int required_freed_bytes,
                         int max_pages = 0);
  int ParallelSweepPage(Page* page, AllocationSpace identity);

  void ScheduleIncrementalSweepingTask();

  int RawSweep(Page* p, FreeListRebuildingMode free_list_mode,
               FreeSpaceTreatmentMode free_space_mode);

  // After calling this function sweeping is considered to be in progress
  // and the main thread can sweep lazily, but the background sweeper tasks
  // are not running yet.
  void StartSweeping();
  void StartSweeperTasks();
  void EnsureCompleted();
  void EnsureNewSpaceCompleted();
  bool AreSweeperTasksRunning();
  void SweepOrWaitUntilSweepingCompleted(Page* page);

  Page* GetSweptPageSafe(PagedSpace* space);

 private:
  class IncrementalSweeperTask;
  class SweeperTask;

  static const int kAllocationSpaces = LAST_PAGED_SPACE + 1;
  static const int kMaxSweeperTasks = kAllocationSpaces;

  template <typename Callback>
  void ForAllSweepingSpaces(Callback callback) {
    for (int i = 0; i < kAllocationSpaces; i++) {
      callback(static_cast<AllocationSpace>(i));
    }
  }

  // Can only be called on the main thread when no tasks are running.
  bool IsDoneSweeping() const {
    for (int i = 0; i < kAllocationSpaces; i++) {
      if (!sweeping_list_[i].empty()) return false;
    }
    return true;
  }

  void SweepSpaceFromTask(AllocationSpace identity);

  // Sweeps incrementally one page from the given space. Returns true if
  // there are no more pages to sweep in the given space.
  bool SweepSpaceIncrementallyFromTask(AllocationSpace identity);

  void AbortAndWaitForTasks();

  Page* GetSweepingPageSafe(AllocationSpace space);

  void PrepareToBeSweptPage(AllocationSpace space, Page* page);

  Heap* const heap_;
  MajorNonAtomicMarkingState* marking_state_;
  int num_tasks_;
  CancelableTaskManager::Id task_ids_[kMaxSweeperTasks];
  base::Semaphore pending_sweeper_tasks_semaphore_;
  base::Mutex mutex_;
  SweptList swept_list_[kAllocationSpaces];
  SweepingList sweeping_list_[kAllocationSpaces];
  bool incremental_sweeper_pending_;
  bool sweeping_in_progress_;
  // Counter is actively maintained by the concurrent tasks to avoid querying
  // the semaphore for maintaining a task counter on the main thread.
  base::AtomicNumber<intptr_t> num_sweeping_tasks_;
  // Used by PauseOrCompleteScope to signal early bailout to tasks.
  base::AtomicValue<bool> stop_sweeper_tasks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SWEEPER_H_
