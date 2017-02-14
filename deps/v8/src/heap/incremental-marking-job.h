// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_JOB_H_
#define V8_HEAP_INCREMENTAL_MARKING_JOB_H_

#include "src/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

// The incremental marking job uses platform tasks to perform incremental
// marking steps. The job posts a foreground task that makes a small (~1ms)
// step and posts another task until the marking is completed.
class IncrementalMarkingJob {
 public:
  class Task : public CancelableTask {
   public:
    explicit Task(Isolate* isolate, IncrementalMarkingJob* job)
        : CancelableTask(isolate), job_(job) {}
    static void Step(Heap* heap);
    // CancelableTask overrides.
    void RunInternal() override;

   private:
    IncrementalMarkingJob* job_;
  };

  IncrementalMarkingJob() : task_pending_(false) {}

  bool TaskPending() { return task_pending_; }

  void Start(Heap* heap);

  void NotifyTask();

  void ScheduleTask(Heap* heap);

 private:
  bool task_pending_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_JOB_H_
