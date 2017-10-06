// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_MARKING_
#define V8_HEAP_CONCURRENT_MARKING_

#include "src/allocation.h"
#include "src/cancelable-task.h"
#include "src/heap/worklist.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

class ConcurrentMarking {
 public:
  // When the scope is entered, the concurrent marking tasks
  // are paused and are not looking at the heap objects.
  class PauseScope {
   public:
    explicit PauseScope(ConcurrentMarking* concurrent_marking);
    ~PauseScope();

   private:
    ConcurrentMarking* concurrent_marking_;
  };

  static const int kTasks = 4;
  using MarkingWorklist = Worklist<HeapObject*, 64 /* segment size */>;

  ConcurrentMarking(Heap* heap, MarkingWorklist* shared_,
                    MarkingWorklist* bailout_);

  void ScheduleTasks();
  void EnsureCompleted();
  void RescheduleTasksIfNeeded();

 private:
  struct TaskLock {
    base::Mutex lock;
    char cache_line_padding[64];
  };
  class Task;
  void Run(int task_id, base::Mutex* lock);
  Heap* heap_;
  MarkingWorklist* shared_;
  MarkingWorklist* bailout_;
  TaskLock task_lock_[kTasks + 1];
  base::Mutex pending_lock_;
  base::ConditionVariable pending_condition_;
  int pending_task_count_;
  bool is_pending_[kTasks + 1];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGE_PARALLEL_JOB_
