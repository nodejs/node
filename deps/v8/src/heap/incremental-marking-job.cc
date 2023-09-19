// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking-job.h"

#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

class IncrementalMarkingJob::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, IncrementalMarkingJob* job, StackState stack_state)
      : CancelableTask(isolate),
        isolate_(isolate),
        job_(job),
        stack_state_(stack_state) {}

  // CancelableTask overrides.
  void RunInternal() override;

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* const isolate_;
  IncrementalMarkingJob* const job_;
  const StackState stack_state_;
};

void IncrementalMarkingJob::ScheduleTask() {
  base::MutexGuard guard(&mutex_);

  if (is_task_pending_ || heap_->IsTearingDown() ||
      !v8_flags.incremental_marking_task) {
    return;
  }

  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap_->isolate());
  is_task_pending_ = true;
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);

  const auto stack_state = taskrunner->NonNestableTasksEnabled()
                               ? StackState::kNoHeapPointers
                               : StackState::kMayContainHeapPointers;

  auto task = std::make_unique<Task>(heap_->isolate(), this, stack_state);

  scheduled_time_ = heap_->MonotonicallyIncreasingTimeInMs();

  if (taskrunner->NonNestableTasksEnabled()) {
    taskrunner->PostNonNestableTask(std::move(task));
  } else {
    taskrunner->PostTask(std::move(task));
  }
}

void IncrementalMarkingJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  Heap* heap = isolate()->heap();
  EmbedderStackStateScope scope(
      heap, EmbedderStackStateScope::kImplicitThroughTask, stack_state_);

  heap->tracer()->RecordTimeToIncrementalMarkingTask(
      heap->MonotonicallyIncreasingTimeInMs() - job_->scheduled_time_);
  job_->scheduled_time_ = 0.0;

  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (incremental_marking->IsStopped()) {
    if (heap->IncrementalMarkingLimitReached() !=
        Heap::IncrementalMarkingLimit::kNoLimit) {
      heap->StartIncrementalMarking(heap->GCFlagsForIncrementalMarking(),
                                    GarbageCollectionReason::kTask,
                                    kGCCallbackScheduleIdleGarbageCollection);
    }
  }

  // Clear this flag after StartIncrementalMarking call to avoid
  // scheduling a new task when starting incremental marking.
  {
    base::MutexGuard guard(&job_->mutex_);
    job_->is_task_pending_ = false;
  }

  if (incremental_marking->IsMajorMarking()) {
    // All objects are initialized at that point.
    heap->new_space()->MarkLabStartInitialized();
    heap->new_lo_space()->ResetPendingObject();

    heap->incremental_marking()->AdvanceAndFinalizeIfComplete();

    if (incremental_marking->IsMajorMarking()) {
      // TODO(v8:12775): It is quite suprising that we schedule the task
      // immediately here. This was introduced since delayed task were
      // unreliable at some point. Investigate whether this is still the case
      // and whether this could be improved.
      job_->ScheduleTask();
    }
  }
}

double IncrementalMarkingJob::CurrentTimeToTask() const {
  if (scheduled_time_ == 0.0) return 0.0;

  return heap_->MonotonicallyIncreasingTimeInMs() - scheduled_time_;
}

}  // namespace internal
}  // namespace v8
