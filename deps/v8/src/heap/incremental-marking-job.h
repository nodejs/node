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
  IncrementalMarkingJob() = default;

  bool TaskPending() const { return task_pending_; }

  void Start(Heap* heap);

  void ScheduleTask(Heap* heap);

 private:
  class Task;

  bool task_pending_ = false;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_JOB_H_
