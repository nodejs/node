// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenge-job.h"

#include "src/base/platform/time.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


const double ScavengeJob::kMaxAllocationLimitAsFractionOfNewSpace = 0.8;

void ScavengeJob::IdleTask::RunInternal(double deadline_in_seconds) {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");
  Heap* heap = isolate()->heap();
  double deadline_in_ms =
      deadline_in_seconds *
      static_cast<double>(base::Time::kMillisecondsPerSecond);
  double start_ms = heap->MonotonicallyIncreasingTimeInMs();
  double idle_time_in_ms = deadline_in_ms - start_ms;
  double scavenge_speed_in_bytes_per_ms =
      heap->tracer()->ScavengeSpeedInBytesPerMillisecond();
  size_t new_space_size = heap->new_space()->Size();
  size_t new_space_capacity = heap->new_space()->Capacity();

  job_->NotifyIdleTask();

  if (ReachedIdleAllocationLimit(scavenge_speed_in_bytes_per_ms, new_space_size,
                                 new_space_capacity)) {
    if (EnoughIdleTimeForScavenge(
            idle_time_in_ms, scavenge_speed_in_bytes_per_ms, new_space_size)) {
      heap->CollectGarbage(NEW_SPACE, GarbageCollectionReason::kIdleTask);
    } else {
      // Immediately request another idle task that can get larger idle time.
      job_->RescheduleIdleTask(heap);
    }
  }
}

bool ScavengeJob::ReachedIdleAllocationLimit(
    double scavenge_speed_in_bytes_per_ms, size_t new_space_size,
    size_t new_space_capacity) {
  if (scavenge_speed_in_bytes_per_ms == 0) {
    scavenge_speed_in_bytes_per_ms = kInitialScavengeSpeedInBytesPerMs;
  }

  // Set the allocation limit to the number of bytes we can scavenge in an
  // average idle task.
  double allocation_limit = kAverageIdleTimeMs * scavenge_speed_in_bytes_per_ms;

  // Keep the limit smaller than the new space capacity.
  allocation_limit =
      Min<double>(allocation_limit,
                  new_space_capacity * kMaxAllocationLimitAsFractionOfNewSpace);
  // Adjust the limit to take into account bytes that will be allocated until
  // the next check and keep the limit large enough to avoid scavenges in tiny
  // new space.
  allocation_limit =
      Max<double>(allocation_limit - kBytesAllocatedBeforeNextIdleTask,
                  kMinAllocationLimit);

  return allocation_limit <= new_space_size;
}

bool ScavengeJob::EnoughIdleTimeForScavenge(
    double idle_time_in_ms, double scavenge_speed_in_bytes_per_ms,
    size_t new_space_size) {
  if (scavenge_speed_in_bytes_per_ms == 0) {
    scavenge_speed_in_bytes_per_ms = kInitialScavengeSpeedInBytesPerMs;
  }
  return new_space_size <= idle_time_in_ms * scavenge_speed_in_bytes_per_ms;
}


void ScavengeJob::RescheduleIdleTask(Heap* heap) {
  // Make sure that we don't reschedule more than one time.
  // Otherwise, we might spam the scheduler with idle tasks.
  if (!idle_task_rescheduled_) {
    ScheduleIdleTask(heap);
    idle_task_rescheduled_ = true;
  }
}


void ScavengeJob::ScheduleIdleTaskIfNeeded(Heap* heap, int bytes_allocated) {
  bytes_allocated_since_the_last_task_ += bytes_allocated;
  if (bytes_allocated_since_the_last_task_ >=
      static_cast<int>(kBytesAllocatedBeforeNextIdleTask)) {
    ScheduleIdleTask(heap);
    bytes_allocated_since_the_last_task_ = 0;
    idle_task_rescheduled_ = false;
  }
}


void ScavengeJob::ScheduleIdleTask(Heap* heap) {
  if (!idle_task_pending_) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    if (V8::GetCurrentPlatform()->IdleTasksEnabled(isolate)) {
      idle_task_pending_ = true;
      auto task = new IdleTask(heap->isolate(), this);
      V8::GetCurrentPlatform()->CallIdleOnForegroundThread(isolate, task);
    }
  }
}
}  // namespace internal
}  // namespace v8
