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
// marking steps. The job posts an idle and a delayed task with a large delay.
// The delayed task performs steps only if the idle task is not making progress.
// We expect this to be a rare event since incremental marking should finish
// quickly with the help of the mutator and the idle task.
// The delayed task guarantees that we eventually finish incremental marking
// even if the mutator becomes idle and the platform stops running idle tasks,
// which can happen for background tabs in Chrome.
class IncrementalMarkingJob {
 public:
  class IdleTask : public CancelableIdleTask {
   public:
    explicit IdleTask(Isolate* isolate, IncrementalMarkingJob* job)
        : CancelableIdleTask(isolate), job_(job) {}
    enum Progress { kDone, kMoreWork };
    static Progress Step(Heap* heap, double deadline_in_ms);
    // CancelableIdleTask overrides.
    void RunInternal(double deadline_in_seconds) override;

   private:
    IncrementalMarkingJob* job_;
  };

  class DelayedTask : public CancelableTask {
   public:
    explicit DelayedTask(Isolate* isolate, IncrementalMarkingJob* job)
        : CancelableTask(isolate), job_(job) {}
    static void Step(Heap* heap);
    // CancelableTask overrides.
    void RunInternal() override;

   private:
    IncrementalMarkingJob* job_;
  };

  // Delay of the delayed task.
  static const int kDelayInSeconds = 5;

  IncrementalMarkingJob()
      : idle_task_pending_(false),
        delayed_task_pending_(false),
        made_progress_since_last_delayed_task_(false) {}

  bool ShouldForceMarkingStep() {
    return !made_progress_since_last_delayed_task_;
  }

  bool IdleTaskPending() { return idle_task_pending_; }

  void Start(Heap* heap);

  void NotifyIdleTask();
  void NotifyDelayedTask();
  void NotifyIdleTaskProgress();
  void ScheduleIdleTask(Heap* heap);
  void ScheduleDelayedTask(Heap* heap);

 private:
  bool idle_task_pending_;
  bool delayed_task_pending_;
  bool made_progress_since_last_delayed_task_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_JOB_H_
