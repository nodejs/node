// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/safepoint.h"

#include <atomic>

#include "src/base/logging.h"
#include "src/handles/handles.h"
#include "src/handles/local-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

GlobalSafepoint::GlobalSafepoint(Heap* heap)
    : heap_(heap), local_heaps_head_(nullptr), active_safepoint_scopes_(0) {}

void GlobalSafepoint::EnterSafepointScope() {
  if (++active_safepoint_scopes_ > 1) return;

  TimedHistogramScope timer(
      heap_->isolate()->counters()->gc_time_to_safepoint());
  TRACE_GC(heap_->tracer(), GCTracer::Scope::TIME_TO_SAFEPOINT);

  local_heaps_mutex_.Lock();

  barrier_.Arm();
  DCHECK_NULL(LocalHeap::Current());

  int running = 0;

  for (LocalHeap* local_heap = local_heaps_head_; local_heap;
       local_heap = local_heap->next_) {
    if (local_heap->is_main_thread()) {
      continue;
    }
    DCHECK(!local_heap->is_main_thread());

    LocalHeap::ThreadState expected = local_heap->state_relaxed();

    while (true) {
      CHECK(expected == LocalHeap::kParked || expected == LocalHeap::kRunning);
      LocalHeap::ThreadState new_state =
          expected == LocalHeap::kParked ? LocalHeap::kParkedSafepointRequested
                                         : LocalHeap::kSafepointRequested;

      if (local_heap->state_.compare_exchange_strong(expected, new_state)) {
        if (expected == LocalHeap::kRunning) {
          running++;
        } else {
          CHECK_EQ(expected, LocalHeap::kParked);
        }
        break;
      }
    }
  }

  barrier_.WaitUntilRunningThreadsInSafepoint(running);
}

void GlobalSafepoint::LeaveSafepointScope() {
  DCHECK_GT(active_safepoint_scopes_, 0);
  if (--active_safepoint_scopes_ > 0) return;

  DCHECK_NULL(LocalHeap::Current());

  for (LocalHeap* local_heap = local_heaps_head_; local_heap;
       local_heap = local_heap->next_) {
    if (local_heap->is_main_thread()) {
      continue;
    }

    // We transition both ParkedSafepointRequested and Safepoint states to
    // Parked. While this is probably intuitive for ParkedSafepointRequested,
    // this might be surprising for Safepoint though. SafepointSlowPath() will
    // later unpark that thread again. Going through Parked means that a
    // background thread doesn't need to be waked up before the main thread can
    // start the next safepoint.

    LocalHeap::ThreadState old_state =
        local_heap->state_.exchange(LocalHeap::kParked);
    CHECK(old_state == LocalHeap::kParkedSafepointRequested ||
          old_state == LocalHeap::kSafepoint);
  }

  barrier_.Disarm();

  local_heaps_mutex_.Unlock();
}

void GlobalSafepoint::WaitInSafepoint() { barrier_.WaitInSafepoint(); }

void GlobalSafepoint::WaitInUnpark() { barrier_.WaitInUnpark(); }

void GlobalSafepoint::NotifyPark() { barrier_.NotifyPark(); }

void GlobalSafepoint::Barrier::Arm() {
  base::MutexGuard guard(&mutex_);
  DCHECK(!IsArmed());
  armed_ = true;
  stopped_ = 0;
}

void GlobalSafepoint::Barrier::Disarm() {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsArmed());
  armed_ = false;
  stopped_ = 0;
  cv_resume_.NotifyAll();
}

void GlobalSafepoint::Barrier::WaitUntilRunningThreadsInSafepoint(int running) {
  base::MutexGuard guard(&mutex_);
  DCHECK(IsArmed());
  while (stopped_ < running) {
    cv_stopped_.Wait(&mutex_);
  }
  DCHECK_EQ(stopped_, running);
}

void GlobalSafepoint::Barrier::NotifyPark() {
  base::MutexGuard guard(&mutex_);
  CHECK(IsArmed());
  stopped_++;
  cv_stopped_.NotifyOne();
}

void GlobalSafepoint::Barrier::WaitInSafepoint() {
  base::MutexGuard guard(&mutex_);
  CHECK(IsArmed());
  stopped_++;
  cv_stopped_.NotifyOne();

  while (IsArmed()) {
    cv_resume_.Wait(&mutex_);
  }
}

void GlobalSafepoint::Barrier::WaitInUnpark() {
  base::MutexGuard guard(&mutex_);

  while (IsArmed()) {
    cv_resume_.Wait(&mutex_);
  }
}

SafepointScope::SafepointScope(Heap* heap) : safepoint_(heap->safepoint()) {
  safepoint_->EnterSafepointScope();
}

SafepointScope::~SafepointScope() { safepoint_->LeaveSafepointScope(); }

bool GlobalSafepoint::ContainsLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  LocalHeap* current = local_heaps_head_;

  while (current) {
    if (current == local_heap) return true;
    current = current->next_;
  }

  return false;
}

bool GlobalSafepoint::ContainsAnyLocalHeap() {
  base::MutexGuard guard(&local_heaps_mutex_);
  return local_heaps_head_ != nullptr;
}

void GlobalSafepoint::Iterate(RootVisitor* visitor) {
  DCHECK(IsActive());
  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->handles()->Iterate(visitor);
  }
}

}  // namespace internal
}  // namespace v8
