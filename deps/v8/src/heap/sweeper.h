// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SWEEPER_H_
#define V8_HEAP_SWEEPER_H_

#include <deque>
#include <vector>

#include "src/base/platform/semaphore.h"
#include "src/common/globals.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class MajorNonAtomicMarkingState;
class Page;
class PagedSpace;

enum FreeSpaceTreatmentMode { IGNORE_FREE_SPACE, ZAP_FREE_SPACE };

class Sweeper {
 public:
  using IterabilityList = std::vector<Page*>;
  using SweepingList = std::vector<Page*>;
  using SweptList = std::vector<Page*>;

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
    const PauseOrCompleteScope& pause_or_complete_scope_;
    bool sweeping_in_progress_;
  };

  enum FreeListRebuildingMode { REBUILD_FREE_LIST, IGNORE_FREE_LIST };
  enum AddPageMode { REGULAR, READD_TEMPORARY_REMOVED_PAGE };
  enum class FreeSpaceMayContainInvalidatedSlots { kYes, kNo };

  Sweeper(Heap* heap, MajorNonAtomicMarkingState* marking_state);

  bool sweeping_in_progress() const { return sweeping_in_progress_; }

  void AddPage(AllocationSpace space, Page* page, AddPageMode mode);

  int ParallelSweepSpace(
      AllocationSpace identity, int required_freed_bytes, int max_pages = 0,
      FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space =
          FreeSpaceMayContainInvalidatedSlots::kNo);
  int ParallelSweepPage(
      Page* page, AllocationSpace identity,
      FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space =
          FreeSpaceMayContainInvalidatedSlots::kNo);

  void ScheduleIncrementalSweepingTask();

  int RawSweep(
      Page* p, FreeListRebuildingMode free_list_mode,
      FreeSpaceTreatmentMode free_space_mode,
      FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space,
      const base::MutexGuard& page_guard);

  // After calling this function sweeping is considered to be in progress
  // and the main thread can sweep lazily, but the background sweeper tasks
  // are not running yet.
  void StartSweeping();
  V8_EXPORT_PRIVATE void StartSweeperTasks();
  void EnsureCompleted();
  bool AreSweeperTasksRunning();

  Page* GetSweptPageSafe(PagedSpace* space);

  void AddPageForIterability(Page* page);
  void StartIterabilityTasks();
  void EnsureIterabilityCompleted();
  void MergeOldToNewRememberedSetsForSweptPages();

 private:
  class IncrementalSweeperTask;
  class IterabilityTask;
  class SweeperTask;

  static const int kNumberOfSweepingSpaces =
      LAST_GROWABLE_PAGED_SPACE - FIRST_GROWABLE_PAGED_SPACE + 1;
  static const int kMaxSweeperTasks = 3;

  template <typename Callback>
  void ForAllSweepingSpaces(Callback callback) const {
    callback(OLD_SPACE);
    callback(CODE_SPACE);
    callback(MAP_SPACE);
  }

  // Can only be called on the main thread when no tasks are running.
  bool IsDoneSweeping() const {
    bool is_done = true;
    ForAllSweepingSpaces([this, &is_done](AllocationSpace space) {
      if (!sweeping_list_[GetSweepSpaceIndex(space)].empty()) is_done = false;
    });
    return is_done;
  }

  void SweepSpaceFromTask(AllocationSpace identity);

  // Sweeps incrementally one page from the given space. Returns true if
  // there are no more pages to sweep in the given space.
  bool SweepSpaceIncrementallyFromTask(AllocationSpace identity);

  void AbortAndWaitForTasks();

  Page* GetSweepingPageSafe(AllocationSpace space);

  void PrepareToBeSweptPage(AllocationSpace space, Page* page);

  void MakeIterable(Page* page);

  bool IsValidIterabilitySpace(AllocationSpace space) {
    return space == NEW_SPACE || space == RO_SPACE;
  }

  static bool IsValidSweepingSpace(AllocationSpace space) {
    return space >= FIRST_GROWABLE_PAGED_SPACE &&
           space <= LAST_GROWABLE_PAGED_SPACE;
  }

  static int GetSweepSpaceIndex(AllocationSpace space) {
    DCHECK(IsValidSweepingSpace(space));
    return space - FIRST_GROWABLE_PAGED_SPACE;
  }

  Heap* const heap_;
  MajorNonAtomicMarkingState* marking_state_;
  int num_tasks_;
  CancelableTaskManager::Id task_ids_[kNumberOfSweepingSpaces];
  base::Semaphore pending_sweeper_tasks_semaphore_;
  base::Mutex mutex_;
  SweptList swept_list_[kNumberOfSweepingSpaces];
  SweepingList sweeping_list_[kNumberOfSweepingSpaces];
  bool incremental_sweeper_pending_;
  bool sweeping_in_progress_;
  // Counter is actively maintained by the concurrent tasks to avoid querying
  // the semaphore for maintaining a task counter on the main thread.
  std::atomic<intptr_t> num_sweeping_tasks_;
  // Used by PauseOrCompleteScope to signal early bailout to tasks.
  std::atomic<bool> stop_sweeper_tasks_;

  // Pages that are only made iterable but have their free lists ignored.
  IterabilityList iterability_list_;
  CancelableTaskManager::Id iterability_task_id_;
  base::Semaphore iterability_task_semaphore_;
  bool iterability_in_progress_;
  bool iterability_task_started_;
  bool should_reduce_memory_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SWEEPER_H_
