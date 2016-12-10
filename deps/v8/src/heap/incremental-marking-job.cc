// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking-job.h"

#include "src/base/platform/time.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/isolate.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

const double IncrementalMarkingJob::kLongDelayInSeconds = 5;
const double IncrementalMarkingJob::kShortDelayInSeconds = 0.5;

void IncrementalMarkingJob::Start(Heap* heap) {
  DCHECK(!heap->incremental_marking()->IsStopped());
  // We don't need to reset the flags because tasks from the previous job
  // can still be pending. We just want to ensure that tasks are posted
  // if they are not pending.
  // If delayed task is pending and made_progress_since_last_delayed_task_ is
  // true, then the delayed task will clear that flag when it is rescheduled.
  ScheduleIdleTask(heap);
  ScheduleDelayedTask(heap);
}


void IncrementalMarkingJob::NotifyIdleTask() { idle_task_pending_ = false; }


void IncrementalMarkingJob::NotifyDelayedTask() {
  delayed_task_pending_ = false;
}


void IncrementalMarkingJob::NotifyIdleTaskProgress() {
  made_progress_since_last_delayed_task_ = true;
}


void IncrementalMarkingJob::ScheduleIdleTask(Heap* heap) {
  if (!idle_task_pending_) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    if (V8::GetCurrentPlatform()->IdleTasksEnabled(isolate)) {
      idle_task_pending_ = true;
      auto task = new IdleTask(heap->isolate(), this);
      V8::GetCurrentPlatform()->CallIdleOnForegroundThread(isolate, task);
    }
  }
}


void IncrementalMarkingJob::ScheduleDelayedTask(Heap* heap) {
  if (!delayed_task_pending_ && FLAG_memory_reducer) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    delayed_task_pending_ = true;
    made_progress_since_last_delayed_task_ = false;
    auto task = new DelayedTask(heap->isolate(), this);
    double delay =
        heap->HighMemoryPressure() ? kShortDelayInSeconds : kLongDelayInSeconds;
    V8::GetCurrentPlatform()->CallDelayedOnForegroundThread(isolate, task,
                                                            delay);
  }
}


IncrementalMarkingJob::IdleTask::Progress IncrementalMarkingJob::IdleTask::Step(
    Heap* heap, double deadline_in_ms) {
  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (incremental_marking->IsStopped()) {
    return kDone;
  }
  if (incremental_marking->IsSweeping()) {
    incremental_marking->FinalizeSweeping();
    // TODO(hpayer): We can continue here if enough idle time is left.
    return kMoreWork;
  }
  const double remaining_idle_time_in_ms =
      incremental_marking->AdvanceIncrementalMarking(
          deadline_in_ms, IncrementalMarking::IdleStepActions());
  if (remaining_idle_time_in_ms > 0.0) {
    heap->TryFinalizeIdleIncrementalMarking(remaining_idle_time_in_ms);
  }
  return incremental_marking->IsStopped() ? kDone : kMoreWork;
}


void IncrementalMarkingJob::IdleTask::RunInternal(double deadline_in_seconds) {
  double deadline_in_ms =
      deadline_in_seconds *
      static_cast<double>(base::Time::kMillisecondsPerSecond);
  Heap* heap = isolate()->heap();
  double start_ms = heap->MonotonicallyIncreasingTimeInMs();
  job_->NotifyIdleTask();
  job_->NotifyIdleTaskProgress();
  if (Step(heap, deadline_in_ms) == kMoreWork) {
    job_->ScheduleIdleTask(heap);
  }
  if (FLAG_trace_idle_notification) {
    double current_time_ms = heap->MonotonicallyIncreasingTimeInMs();
    double idle_time_in_ms = deadline_in_ms - start_ms;
    double deadline_difference = deadline_in_ms - current_time_ms;
    PrintIsolate(isolate(), "%8.0f ms: ", isolate()->time_millis_since_init());
    PrintF(
        "Idle task: requested idle time %.2f ms, used idle time %.2f "
        "ms, deadline usage %.2f ms\n",
        idle_time_in_ms, idle_time_in_ms - deadline_difference,
        deadline_difference);
  }
}


void IncrementalMarkingJob::DelayedTask::Step(Heap* heap) {
  const int kIncrementalMarkingDelayMs = 50;
  double deadline =
      heap->MonotonicallyIncreasingTimeInMs() + kIncrementalMarkingDelayMs;
  heap->incremental_marking()->AdvanceIncrementalMarking(
      deadline, i::IncrementalMarking::StepActions(
                    i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    i::IncrementalMarking::FORCE_MARKING,
                    i::IncrementalMarking::FORCE_COMPLETION));
  heap->FinalizeIncrementalMarkingIfComplete(
      "Incremental marking task: finalize incremental marking");
}


void IncrementalMarkingJob::DelayedTask::RunInternal() {
  Heap* heap = isolate()->heap();
  job_->NotifyDelayedTask();
  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (!incremental_marking->IsStopped()) {
    if (job_->ShouldForceMarkingStep()) {
      Step(heap);
    }
    // The Step() above could have finished incremental marking.
    if (!incremental_marking->IsStopped()) {
      job_->ScheduleDelayedTask(heap);
    }
  }
}

}  // namespace internal
}  // namespace v8
