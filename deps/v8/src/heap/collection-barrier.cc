// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/collection-barrier.h"

#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

void CollectionBarrier::ResumeThreadsAwaitingCollection() {
  base::MutexGuard guard(&mutex_);
  ClearCollectionRequested();
  cond_.NotifyAll();
}

void CollectionBarrier::ShutdownRequested() {
  base::MutexGuard guard(&mutex_);
  if (timer_.IsStarted()) timer_.Stop();
  state_.store(RequestState::kShutdown);
  cond_.NotifyAll();
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

void CollectionBarrier::AwaitCollectionBackground() {
  bool first;

  {
    base::MutexGuard guard(&mutex_);
    first = FirstCollectionRequest();
    if (first) timer_.Start();
  }

  if (first) {
    // This is the first background thread requesting collection, ask the main
    // thread for GC.
    ActivateStackGuardAndPostTask();
  }

  BlockUntilCollected();
}

void CollectionBarrier::StopTimeToCollectionTimer() {
  base::MutexGuard guard(&mutex_);
  RequestState old_state = state_.exchange(RequestState::kCollectionStarted,
                                           std::memory_order_relaxed);
  if (old_state == RequestState::kCollectionRequested) {
    DCHECK(timer_.IsStarted());
    base::TimeDelta delta = timer_.Elapsed();
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                         "V8.TimeToCollection", TRACE_EVENT_SCOPE_THREAD,
                         "duration", delta.InMillisecondsF());
    heap_->isolate()->counters()->time_to_collection()->AddTimedSample(delta);
    timer_.Stop();
  } else {
    DCHECK_EQ(old_state, RequestState::kDefault);
    DCHECK(!timer_.IsStarted());
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
}

void CollectionBarrier::BlockUntilCollected() {
  TRACE_GC1(heap_->tracer(), GCTracer::Scope::BACKGROUND_COLLECTION,
            ThreadKind::kBackground);
  base::MutexGuard guard(&mutex_);

  while (CollectionRequested()) {
    cond_.Wait(&mutex_);
  }
}

}  // namespace internal
}  // namespace v8
