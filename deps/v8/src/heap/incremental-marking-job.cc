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
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

void IncrementalMarkingJob::Start(Heap* heap) {
  DCHECK(!heap->incremental_marking()->IsStopped());
  ScheduleTask(heap);
}

void IncrementalMarkingJob::NotifyTask() { task_pending_ = false; }

void IncrementalMarkingJob::ScheduleTask(Heap* heap) {
  if (!task_pending_) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    task_pending_ = true;
    auto task = new Task(heap->isolate(), this);
    V8::GetCurrentPlatform()->CallOnForegroundThread(isolate, task);
  }
}

void IncrementalMarkingJob::Task::Step(Heap* heap) {
  const int kIncrementalMarkingDelayMs = 1;
  double deadline =
      heap->MonotonicallyIncreasingTimeInMs() + kIncrementalMarkingDelayMs;
  heap->incremental_marking()->AdvanceIncrementalMarking(
      deadline, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
      i::IncrementalMarking::FORCE_COMPLETION, i::StepOrigin::kTask);
  heap->FinalizeIncrementalMarkingIfComplete(
      GarbageCollectionReason::kFinalizeMarkingViaTask);
}

void IncrementalMarkingJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  RuntimeCallTimerScope runtime_timer(
      isolate(), &RuntimeCallStats::GC_IncrementalMarkingJob);

  Heap* heap = isolate()->heap();
  job_->NotifyTask();
  IncrementalMarking* incremental_marking = heap->incremental_marking();
  if (incremental_marking->IsStopped()) {
    if (heap->IncrementalMarkingLimitReached() !=
        Heap::IncrementalMarkingLimit::kNoLimit) {
      heap->StartIncrementalMarking(Heap::kNoGCFlags,
                                    GarbageCollectionReason::kIdleTask,
                                    kNoGCCallbackFlags);
    }
  }
  if (!incremental_marking->IsStopped()) {
    Step(heap);
    if (!incremental_marking->IsStopped()) {
      job_->ScheduleTask(heap);
    }
  }
}

}  // namespace internal
}  // namespace v8
