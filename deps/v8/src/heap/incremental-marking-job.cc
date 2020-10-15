// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking-job.h"

#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

class IncrementalMarkingJob::Task : public CancelableTask {
 public:
  static StepResult Step(Heap* heap);

  Task(Isolate* isolate, IncrementalMarkingJob* job,
       EmbedderHeapTracer::EmbedderStackState stack_state, TaskType task_type)
      : CancelableTask(isolate),
        isolate_(isolate),
        job_(job),
        stack_state_(stack_state),
        task_type_(task_type) {}

  // CancelableTask overrides.
  void RunInternal() override;

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* const isolate_;
  IncrementalMarkingJob* const job_;
  const EmbedderHeapTracer::EmbedderStackState stack_state_;
  const TaskType task_type_;
};

void IncrementalMarkingJob::Start(Heap* heap) {
  DCHECK(!heap->incremental_marking()->IsStopped());
  ScheduleTask(heap);
}

void IncrementalMarkingJob::ScheduleTask(Heap* heap, TaskType task_type) {
  base::MutexGuard guard(&mutex_);

  if (!IsTaskPending(task_type) && !heap->IsTearingDown() &&
      FLAG_incremental_marking_task) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    SetTaskPending(task_type, true);
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
    const EmbedderHeapTracer::EmbedderStackState stack_state =
        taskrunner->NonNestableTasksEnabled()
            ? EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers
            : EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers;
    auto task =
        std::make_unique<Task>(heap->isolate(), this, stack_state, task_type);
    if (task_type == TaskType::kNormal) {
      scheduled_time_ = heap->MonotonicallyIncreasingTimeInMs();
      if (taskrunner->NonNestableTasksEnabled()) {
        taskrunner->PostNonNestableTask(std::move(task));
      } else {
        taskrunner->PostTask(std::move(task));
      }
    } else {
      if (taskrunner->NonNestableDelayedTasksEnabled()) {
        taskrunner->PostNonNestableDelayedTask(std::move(task),
                                               kDelayInSeconds);
      } else {
        taskrunner->PostDelayedTask(std::move(task), kDelayInSeconds);
      }
    }
  }
}

StepResult IncrementalMarkingJob::Task::Step(Heap* heap) {
  const int kIncrementalMarkingDelayMs = 1;
  double deadline =
      heap->MonotonicallyIncreasingTimeInMs() + kIncrementalMarkingDelayMs;
  StepResult result = heap->incremental_marking()->AdvanceWithDeadline(
      deadline, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
      i::StepOrigin::kTask);
  heap->FinalizeIncrementalMarkingIfComplete(
      GarbageCollectionReason::kFinalizeMarkingViaTask);
  return result;
}

void IncrementalMarkingJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  Heap* heap = isolate()->heap();
  EmbedderStackStateScope scope(heap->local_embedder_heap_tracer(),
                                stack_state_);
  if (task_type_ == TaskType::kNormal) {
    heap->tracer()->RecordTimeToIncrementalMarkingTask(
        heap->MonotonicallyIncreasingTimeInMs() - job_->scheduled_time_);
    job_->scheduled_time_ = 0.0;
  }
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
    job_->SetTaskPending(task_type_, false);
  }

  if (!incremental_marking->IsStopped()) {
    // All objects are initialized at that point.
    heap->new_space()->MarkLabStartInitialized();
    heap->new_lo_space()->ResetPendingObject();
    StepResult step_result = Step(heap);
    if (!incremental_marking->IsStopped()) {
      const TaskType task_type =
          incremental_marking->finalize_marking_completed() ||
                  step_result != StepResult::kNoImmediateWork
              ? TaskType::kNormal
              : TaskType::kDelayed;
      job_->ScheduleTask(heap, task_type);
    }
  }
}

double IncrementalMarkingJob::CurrentTimeToTask(Heap* heap) const {
  if (scheduled_time_ == 0.0) return 0.0;

  return heap->MonotonicallyIncreasingTimeInMs() - scheduled_time_;
}

}  // namespace internal
}  // namespace v8
