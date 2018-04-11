// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_MARKING_H_
#define V8_HEAP_CONCURRENT_MARKING_H_

#include "include/v8-platform.h"
#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/cancelable-task.h"
#include "src/heap/spaces.h"
#include "src/heap/worklist.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class MajorNonAtomicMarkingState;
struct WeakObjects;

using LiveBytesMap =
    std::unordered_map<MemoryChunk*, intptr_t, MemoryChunk::Hasher>;

class ConcurrentMarking {
 public:
  // When the scope is entered, the concurrent marking tasks
  // are preempted and are not looking at the heap objects, concurrent marking
  // is resumed when the scope is exited.
  class PauseScope {
   public:
    explicit PauseScope(ConcurrentMarking* concurrent_marking);
    ~PauseScope();

   private:
    ConcurrentMarking* const concurrent_marking_;
    const bool resume_on_exit_;
  };

  enum class StopRequest {
    // Preempt ongoing tasks ASAP (and cancel unstarted tasks).
    PREEMPT_TASKS,
    // Wait for ongoing tasks to complete (and cancels unstarted tasks).
    COMPLETE_ONGOING_TASKS,
    // Wait for all scheduled tasks to complete (only use this in tests that
    // control the full stack -- otherwise tasks cancelled by the platform can
    // make this call hang).
    COMPLETE_TASKS_FOR_TESTING,
  };

  // TODO(gab): The only thing that prevents this being above 7 is
  // Worklist::kMaxNumTasks being maxed at 8 (concurrent marking doesn't use
  // task 0, reserved for the main thread).
  static constexpr int kMaxTasks = 7;
  using MarkingWorklist = Worklist<HeapObject*, 64 /* segment size */>;

  ConcurrentMarking(Heap* heap, MarkingWorklist* shared,
                    MarkingWorklist* bailout, MarkingWorklist* on_hold,
                    WeakObjects* weak_objects);

  // Schedules asynchronous tasks to perform concurrent marking. Objects in the
  // heap should not be moved while these are active (can be stopped safely via
  // Stop() or PauseScope).
  void ScheduleTasks();

  // Stops concurrent marking per |stop_request|'s semantics. Returns true
  // if concurrent marking was in progress, false otherwise.
  bool Stop(StopRequest stop_request);

  void RescheduleTasksIfNeeded();
  // Flushes the local live bytes into the given marking state.
  void FlushLiveBytes(MajorNonAtomicMarkingState* marking_state);
  // This function is called for a new space page that was cleared after
  // scavenge and is going to be re-used.
  void ClearLiveness(MemoryChunk* chunk);

  int TaskCount() { return task_count_; }

  size_t TotalMarkedBytes();

 private:
  struct TaskState {
    // The main thread sets this flag to true when it wants the concurrent
    // marker to give up the worker thread.
    base::AtomicValue<bool> preemption_request;

    LiveBytesMap live_bytes;
    size_t marked_bytes = 0;
    char cache_line_padding[64];
  };
  class Task;
  void Run(int task_id, TaskState* task_state);
  Heap* const heap_;
  MarkingWorklist* const shared_;
  MarkingWorklist* const bailout_;
  MarkingWorklist* const on_hold_;
  WeakObjects* const weak_objects_;
  TaskState task_state_[kMaxTasks + 1];
  base::AtomicNumber<size_t> total_marked_bytes_{0};
  base::Mutex pending_lock_;
  base::ConditionVariable pending_condition_;
  int pending_task_count_ = 0;
  bool is_pending_[kMaxTasks + 1] = {};
  CancelableTaskManager::Id cancelable_id_[kMaxTasks + 1] = {};
  int task_count_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_MARKING_H_
