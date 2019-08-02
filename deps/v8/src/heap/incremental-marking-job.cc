// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking-job.h"

#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

class IncrementalMarkingJob::Task : public CancelableTask {
 public:
  static StepResult Step(Heap* heap,
                         EmbedderHeapTracer::EmbedderStackState stack_state);

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
  if (!IsTaskPending(task_type) && !heap->IsTearingDown()) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    SetTaskPending(task_type, true);
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
    if (task_type == TaskType::kNormal) {
      if (taskrunner->NonNestableTasksEnabled()) {
        taskrunner->PostNonNestableTask(base::make_unique<Task>(
            heap->isolate(), this,
            EmbedderHeapTracer::EmbedderStackState::kEmpty, task_type));
      } else {
        taskrunner->PostTask(base::make_unique<Task>(
            heap->isolate(), this,
            EmbedderHeapTracer::EmbedderStackState::kUnknown, task_type));
      }
    } else {
      if (taskrunner->NonNestableDelayedTasksEnabled()) {
        taskrunner->PostNonNestableDelayedTask(
            base::make_unique<Task>(
                heap->isolate(), this,
                EmbedderHeapTracer::EmbedderStackState::kEmpty, task_type),
            kDelayInSeconds);
      } else {
        taskrunner->PostDelayedTask(
            base::make_unique<Task>(
                heap->isolate(), this,
                EmbedderHeapTracer::EmbedderStackState::kUnknown, task_type),
            kDelayInSeconds);
      }
    }
  }
}

StepResult IncrementalMarkingJob::Task::Step(
    Heap* heap, EmbedderHeapTracer::EmbedderStackState stack_state) {
  const int kIncrementalMarkingDelayMs = 1;
  double deadline =
      heap->MonotonicallyIncreasingTimeInMs() + kIncrementalMarkingDelayMs;
  StepResult result = heap->incremental_marking()->AdvanceWithDeadline(
      deadline, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
      i::StepOrigin::kTask);
  {
    EmbedderStackStateScope scope(heap->local_embedder_heap_tracer(),
                                  stack_state);
    heap->FinalizeIncrementalMarkingIfComplete(
        GarbageCollectionReason::kFinalizeMarkingViaTask);
  }
  return result;
}

void IncrementalMarkingJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  Heap* heap = isolate()->heap();
  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (incremental_marking->IsStopped()) {
    if (heap->IncrementalMarkingLimitReached() !=
        Heap::IncrementalMarkingLimit::kNoLimit) {
      heap->StartIncrementalMarking(heap->GCFlagsForIncrementalMarking(),
                                    GarbageCollectionReason::kIdleTask,
                                    kGCCallbackScheduleIdleGarbageCollection);
    }
  }

  // Clear this flag after StartIncrementalMarking call to avoid
  // scheduling a new task when startining incremental marking.
  job_->SetTaskPending(task_type_, false);

  if (!incremental_marking->IsStopped()) {
    StepResult step_result = Step(heap, stack_state_);
    if (!incremental_marking->IsStopped()) {
      job_->ScheduleTask(heap, step_result == StepResult::kNoImmediateWork
                                   ? TaskType::kDelayed
                                   : TaskType::kNormal);
    }
  }
}

}  // namespace internal
}  // namespace v8
