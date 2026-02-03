// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/collection-barrier.h"

#include <memory>

#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/parked-scope.h"

namespace v8 {
namespace internal {
namespace {

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
  void RunInternal() override {
    // Set the current isolate such that trusted pointer tables etc are
    // available and the cage base is set correctly for multi-cage mode.
    SetCurrentIsolateScope isolate_scope(heap_->isolate());
    SetCurrentLocalHeapScope local_heap_scope(heap_->isolate());
    heap_->PerformRequestedGC(heap_->isolate()->main_thread_local_heap());
  }

  Heap* heap_;
};

}  // namespace

bool CollectionBarrier::CollectionRequest::Set(RequestedGCKind kind) {
  uint8_t previous_state =
      state_.fetch_or(static_cast<uint8_t>(kind), std::memory_order_relaxed);
  return previous_state >= static_cast<uint8_t>(kind);
}

bool CollectionBarrier::CollectionRequest::Clear(RequestedGCKind kind) {
  uint8_t previous_state = state_.fetch_and(
      ~(static_cast<uint8_t>(kind) | static_cast<uint8_t>(kind) - 1),
      std::memory_order_relaxed);
  return previous_state & static_cast<uint8_t>(kind);
}

void CollectionBarrier::CollectionRequest::ClearAll() {
  state_.store(0, std::memory_order_relaxed);
}

bool CollectionBarrier::CollectionRequest::Has(RequestedGCKind kind) const {
  return state_.load(std::memory_order_relaxed) & static_cast<uint8_t>(kind);
}

bool CollectionBarrier::CollectionRequest::HasAny() const {
  return state_.load(std::memory_order_relaxed) != 0;
}

std::optional<RequestedGCKind> CollectionBarrier::CollectionRequest::Get()
    const {
  auto state = state_.load(std::memory_order_relaxed);
  if (state & static_cast<uint8_t>(RequestedGCKind::kLastResort)) {
    return RequestedGCKind::kLastResort;
  } else if (state & static_cast<uint8_t>(RequestedGCKind::kMajor)) {
    return RequestedGCKind::kMajor;
  }
  return std::nullopt;
}

CollectionBarrier::CollectionBarrier(
    Heap* heap, std::shared_ptr<v8::TaskRunner> foreground_task_runner)
    : heap_(heap), foreground_task_runner_(foreground_task_runner) {}

std::optional<RequestedGCKind> CollectionBarrier::RequestedGC() {
  return collection_request_.Get();
}

bool CollectionBarrier::TryRequestGC(RequestedGCKind kind) {
  base::MutexGuard guard(&mutex_);
  if (shutdown_requested_) return false;
  auto already_requested = collection_request_.Set(kind);

  // The first thread that requests this kind of GC needs to activate the stack
  // guard and post the task.
  if (!already_requested) {
    auto& timer = GetTimerForCollectionRequest(kind);
    DCHECK(!timer.IsStarted());
    timer.Start();

    Isolate* isolate = heap_->isolate();
    ExecutionAccess access(isolate);
    isolate->stack_guard()->RequestGC();

    foreground_task_runner_->PostTask(
        std::make_unique<BackgroundCollectionInterruptTask>(heap_));
  }

  return true;
}

void CollectionBarrier::NotifyShutdownRequested() {
  base::MutexGuard guard(&mutex_);
  if (major_timer_.IsStarted()) major_timer_.Stop();
  if (last_resort_timer_.IsStarted()) last_resort_timer_.Stop();
  shutdown_requested_ = true;
  cv_wakeup_.NotifyAll();
}

void CollectionBarrier::ResumeThreadsAwaitingCollection(RequestedGCKind kind) {
  base::MutexGuard guard(&mutex_);
  DCHECK(!GetTimerForCollectionRequest(kind).IsStarted());
  auto was_requested = collection_request_.Clear(kind);
  if (was_requested) {
    collection_performed_ = true;
    cv_wakeup_.NotifyAll();
  }
}

void CollectionBarrier::CancelCollectionAndResumeThreads() {
  base::MutexGuard guard(&mutex_);
  if (major_timer_.IsStarted()) major_timer_.Stop();
  if (last_resort_timer_.IsStarted()) last_resort_timer_.Stop();
  collection_request_.ClearAll();
  collection_performed_ = false;
  cv_wakeup_.NotifyAll();
}

bool CollectionBarrier::AwaitCollectionBackground(LocalHeap* local_heap,
                                                  RequestedGCKind kind) {
  {
    // Update flag before parking this thread, this guarantees that the flag is
    // set before the next GC.
    base::MutexGuard guard(&mutex_);
    if (shutdown_requested_) return false;

    // Collection was cancelled by the main thread.
    if (!collection_request_.Has(kind)) return false;

    CHECK(GetTimerForCollectionRequest(kind).IsStarted());
  }

  bool collection_performed = false;
  local_heap->ExecuteWhileParked([this, &collection_performed, kind]() {
    base::MutexGuard guard(&mutex_);

    while (collection_request_.Has(kind)) {
      if (shutdown_requested_) {
        collection_performed = false;
        return;
      }
      cv_wakeup_.Wait(&mutex_);
    }

    // Collection may have been cancelled while blocking for it.
    collection_performed = collection_performed_;
  });

  return collection_performed;
}

void CollectionBarrier::StopTimeToCollectionTimer(RequestedGCKind kind) {
  if (collection_request_.Has(kind)) {
    base::MutexGuard guard(&mutex_);
    auto& timer = GetTimerForCollectionRequest(kind);
    // The first thread that requests the GC, starts the timer first and *then*
    // parks itself. Since we are in a safepoint here, the timer is always
    // initialized here already.
    CHECK(timer.IsStarted());
    base::TimeDelta delta = timer.Elapsed();
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                         "V8.GC.TimeToCollectionOnBackground",
                         TRACE_EVENT_SCOPE_THREAD, "duration",
                         delta.InMillisecondsF(), "kind", kind);
    heap_->isolate()
        ->counters()
        ->gc_time_to_collection_on_background()
        ->AddTimedSample(delta);
    timer.Stop();
  }
}

base::ElapsedTimer& CollectionBarrier::GetTimerForCollectionRequest(
    RequestedGCKind kind) {
  if (kind == RequestedGCKind::kLastResort) {
    return last_resort_timer_;
  } else {
    return major_timer_;
  }
}

}  // namespace internal
}  // namespace v8
