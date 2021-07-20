// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/collection-barrier.h"

#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"

namespace v8 {
namespace internal {

bool CollectionBarrier::CollectionRequested() {
  return main_thread_state_relaxed() == LocalHeap::kCollectionRequested;
}

LocalHeap::ThreadState CollectionBarrier::main_thread_state_relaxed() {
  LocalHeap* main_thread_local_heap = heap_->main_thread_local_heap();
  return main_thread_local_heap->state_relaxed();
}

void CollectionBarrier::NotifyShutdownRequested() {
  base::MutexGuard guard(&mutex_);
  if (timer_.IsStarted()) timer_.Stop();
  shutdown_requested_ = true;
  cv_wakeup_.NotifyAll();
}

void CollectionBarrier::ResumeThreadsAwaitingCollection() {
  base::MutexGuard guard(&mutex_);
  cv_wakeup_.NotifyAll();
}

class BackgroundCollectionInterruptTask : public CancelableTask {
 public:
  explicit BackgroundCollectionInterruptTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  ~BackgroundCollectionInterruptTask() override = default;
  BackgroundCollectionInterruptTask(const BackgroundCollectionInterruptTask&) =
      delete;
  BackgroundCollectionInterruptTask& operator=(
      const BackgroundCollectionInterruptTask&) = delete;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { heap_->CheckCollectionRequested(); }

  Heap* heap_;
};

bool CollectionBarrier::AwaitCollectionBackground(LocalHeap* local_heap) {
  ParkedScope scope(local_heap);
  base::MutexGuard guard(&mutex_);

  while (CollectionRequested()) {
    if (shutdown_requested_) return false;
    cv_wakeup_.Wait(&mutex_);
  }

  return true;
}

void CollectionBarrier::StopTimeToCollectionTimer() {
  LocalHeap::ThreadState main_thread_state = main_thread_state_relaxed();
  CHECK(main_thread_state == LocalHeap::kRunning ||
        main_thread_state == LocalHeap::kCollectionRequested);

  if (main_thread_state == LocalHeap::kCollectionRequested) {
    base::MutexGuard guard(&mutex_);
    // The first background thread that requests the GC, starts the timer first
    // and only then parks itself. Since we are in a safepoint here, the timer
    // is therefore always initialized here already.
    CHECK(timer_.IsStarted());
    base::TimeDelta delta = timer_.Elapsed();
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                         "V8.GC.TimeToCollectionOnBackground",
                         TRACE_EVENT_SCOPE_THREAD, "duration",
                         delta.InMillisecondsF());
    heap_->isolate()
        ->counters()
        ->gc_time_to_collection_on_background()
        ->AddTimedSample(delta);
    timer_.Stop();
  }
}

void CollectionBarrier::ActivateStackGuardAndPostTask() {
  Isolate* isolate = heap_->isolate();
  ExecutionAccess access(isolate);
  isolate->stack_guard()->RequestGC();

  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
      reinterpret_cast<v8::Isolate*>(isolate));
  taskrunner->PostTask(
      std::make_unique<BackgroundCollectionInterruptTask>(heap_));

  base::MutexGuard guard(&mutex_);
  CHECK(!timer_.IsStarted());
  timer_.Start();
}

}  // namespace internal
}  // namespace v8
