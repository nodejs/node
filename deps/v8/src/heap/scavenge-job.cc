// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenge-job.h"

#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

class ScavengeJob::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, ScavengeJob* job)
      : CancelableTask(isolate), isolate_(isolate), job_(job) {}

  // CancelableTask overrides.
  void RunInternal() override;

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* const isolate_;
  ScavengeJob* const job_;
};

size_t ScavengeJob::YoungGenerationTaskTriggerSize(Heap* heap) {
  static constexpr double kTaskTriggerFactor = 0.8;
  return heap->new_space()->Capacity() * kTaskTriggerFactor;
}

bool ScavengeJob::YoungGenerationSizeTaskTriggerReached(Heap* heap) {
  return heap->new_space()->Size() >= YoungGenerationTaskTriggerSize(heap);
}

void ScavengeJob::ScheduleTaskIfNeeded(Heap* heap) {
  if (!task_pending_ && !heap->IsTearingDown() &&
      YoungGenerationSizeTaskTriggerReached(heap)) {
    v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
    auto taskrunner =
        V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
    taskrunner->PostTask(std::make_unique<Task>(heap->isolate(), this));
    task_pending_ = true;
  }
}

void ScavengeJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  if (ScavengeJob::YoungGenerationSizeTaskTriggerReached(isolate()->heap())) {
    isolate()->heap()->CollectGarbage(NEW_SPACE,
                                      GarbageCollectionReason::kTask);
  }

  job_->set_task_pending(false);
}

}  // namespace internal
}  // namespace v8
