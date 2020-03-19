// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGE_JOB_H_
#define V8_HEAP_SCAVENGE_JOB_H_

#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

// The scavenge job uses platform tasks to perform a young generation
// Scavenge garbage collection. The job posts a foreground task.
class ScavengeJob {
 public:
  ScavengeJob() V8_NOEXCEPT = default;

  void ScheduleTaskIfNeeded(Heap* heap);

  static size_t YoungGenerationTaskTriggerSize(Heap* heap);

 private:
  class Task;

  static bool YoungGenerationSizeTaskTriggerReached(Heap* heap);

  void set_task_pending(bool value) { task_pending_ = value; }

  bool task_pending_ = false;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGE_JOB_H_
