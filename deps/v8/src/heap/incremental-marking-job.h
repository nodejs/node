// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_JOB_H_
#define V8_HEAP_INCREMENTAL_MARKING_JOB_H_

#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

// The incremental marking job uses platform tasks to perform incremental
// marking steps. The job posts a foreground task that makes a small (~1ms)
// step and posts another task until the marking is completed.
class IncrementalMarkingJob final {
 public:
  explicit IncrementalMarkingJob(Heap* heap) V8_NOEXCEPT : heap_(heap) {}

  void ScheduleTask();
  double CurrentTimeToTask() const;

 private:
  class Task;
  static constexpr double kDelayInSeconds = 10.0 / 1000.0;

  Heap* heap_;
  base::Mutex mutex_;
  double scheduled_time_ = 0.0;
  bool is_task_pending_ = false;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_JOB_H_
