// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGE_JOB_H_
#define V8_HEAP_SCAVENGE_JOB_H_

#include "src/cancelable-task.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

// This class posts idle tasks and performs scavenges in the idle tasks.
class V8_EXPORT_PRIVATE ScavengeJob {
 public:
  class IdleTask : public CancelableIdleTask {
   public:
    explicit IdleTask(Isolate* isolate, ScavengeJob* job)
        : CancelableIdleTask(isolate), isolate_(isolate), job_(job) {}
    // CancelableIdleTask overrides.
    void RunInternal(double deadline_in_seconds) override;

    Isolate* isolate() { return isolate_; }

   private:
    Isolate* isolate_;
    ScavengeJob* job_;
  };

  ScavengeJob()
      : idle_task_pending_(false),
        idle_task_rescheduled_(false),
        bytes_allocated_since_the_last_task_(0) {}

  // Posts an idle task if the cumulative bytes allocated since the last
  // idle task exceed kBytesAllocatedBeforeNextIdleTask.
  void ScheduleIdleTaskIfNeeded(Heap* heap, int bytes_allocated);

  // Posts an idle task ignoring the bytes allocated, but makes sure
  // that the new idle task cannot reschedule again.
  // This prevents infinite rescheduling.
  void RescheduleIdleTask(Heap* heap);

  bool IdleTaskPending() { return idle_task_pending_; }
  void NotifyIdleTask() { idle_task_pending_ = false; }
  bool IdleTaskRescheduled() { return idle_task_rescheduled_; }

  static bool ReachedIdleAllocationLimit(double scavenge_speed_in_bytes_per_ms,
                                         size_t new_space_size,
                                         size_t new_space_capacity);

  static bool EnoughIdleTimeForScavenge(double idle_time_ms,
                                        double scavenge_speed_in_bytes_per_ms,
                                        size_t new_space_size);

  // If we haven't recorded any scavenger events yet, we use a conservative
  // lower bound for the scavenger speed.
  static const int kInitialScavengeSpeedInBytesPerMs = 256 * KB;
  // Estimate of the average idle time that an idle task gets.
  static const int kAverageIdleTimeMs = 5;
  // The number of bytes to be allocated in new space before the next idle
  // task is posted.
  static const size_t kBytesAllocatedBeforeNextIdleTask = 1024 * KB;
  // The minimum size of allocated new space objects to trigger a scavenge.
  static const size_t kMinAllocationLimit = 512 * KB;
  // The allocation limit cannot exceed this fraction of the new space capacity.
  static const double kMaxAllocationLimitAsFractionOfNewSpace;

 private:
  void ScheduleIdleTask(Heap* heap);
  bool idle_task_pending_;
  bool idle_task_rescheduled_;
  int bytes_allocated_since_the_last_task_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGE_JOB_H_
