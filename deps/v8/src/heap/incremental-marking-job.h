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
  enum class TaskType { kNormal, kDelayed };

  IncrementalMarkingJob() V8_NOEXCEPT = default;

  void Start(Heap* heap);

  void ScheduleTask(Heap* heap, TaskType task_type = TaskType::kNormal);

 private:
  class Task;
  static constexpr double kDelayInSeconds = 10.0 / 1000.0;

  bool IsTaskPending(TaskType task_type) {
    return task_type == TaskType::kNormal ? normal_task_pending_
                                          : delayed_task_pending_;
  }
  void SetTaskPending(TaskType task_type, bool value) {
    if (task_type == TaskType::kNormal) {
      normal_task_pending_ = value;
    } else {
      delayed_task_pending_ = value;
    }
  }

  bool normal_task_pending_ = false;
  bool delayed_task_pending_ = false;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_JOB_H_
