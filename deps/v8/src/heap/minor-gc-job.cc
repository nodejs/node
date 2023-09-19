// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/minor-gc-job.h"

#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

class MinorGCJob::Task : public CancelableTask {
 public:
  Task(Isolate* isolate, MinorGCJob* job)
      : CancelableTask(isolate), isolate_(isolate), job_(job) {}

  // CancelableTask overrides.
  void RunInternal() override;

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* const isolate_;
  MinorGCJob* const job_;
};

size_t MinorGCJob::YoungGenerationTaskTriggerSize(Heap* heap) {
  return heap->new_space()->TotalCapacity() * v8_flags.minor_gc_task_trigger /
         100;
}

bool MinorGCJob::YoungGenerationSizeTaskTriggerReached(Heap* heap) {
  return heap->new_space()->Size() >= YoungGenerationTaskTriggerSize(heap);
}

void MinorGCJob::ScheduleTaskIfNeeded(Heap* heap) {
  if (!v8_flags.minor_gc_task) return;
  if (task_pending_) return;
  if (heap->IsTearingDown()) return;
  if (!YoungGenerationSizeTaskTriggerReached(heap)) return;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap->isolate());
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
  if (taskrunner->NonNestableTasksEnabled()) {
    taskrunner->PostNonNestableTask(
        std::make_unique<Task>(heap->isolate(), this));
    task_pending_ = true;
  }
}

void MinorGCJob::Task::RunInternal() {
  VMState<GC> state(isolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate(), "v8", "V8.Task");

  job_->task_pending_ = false;

  if (v8_flags.minor_mc &&
      isolate()->heap()->incremental_marking()->IsMajorMarking()) {
    // Don't trigger a MinorMC cycle while major incremental marking is active.
    return;
  }
  if (!MinorGCJob::YoungGenerationSizeTaskTriggerReached(isolate()->heap()))
    return;

  isolate()->heap()->CollectGarbage(NEW_SPACE, GarbageCollectionReason::kTask);
}

}  // namespace internal
}  // namespace v8
