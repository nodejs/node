// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/safepoint.h"

#include "src/base/logging.h"
#include "src/handles/local-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

GlobalSafepoint::GlobalSafepoint(Heap* heap)
    : heap_(heap), local_heaps_head_(nullptr), active_safepoint_scopes_(0) {}

void GlobalSafepoint::EnterSafepointScope() {
  if (!FLAG_local_heaps) return;

  if (++active_safepoint_scopes_ > 1) return;

  TimedHistogramScope timer(heap_->isolate()->counters()->stop_the_world());
  TRACE_GC(heap_->tracer(), GCTracer::Scope::STOP_THE_WORLD);

  local_heaps_mutex_.Lock();

  barrier_.Arm();
  DCHECK_NULL(LocalHeap::Current());

  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    if (current->is_main_thread()) {
      continue;
    }
    current->RequestSafepoint();
  }

  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    if (current->is_main_thread()) {
      continue;
    }
    DCHECK(!current->is_main_thread());
    current->state_mutex_.Lock();

    while (current->state_ == LocalHeap::ThreadState::Running) {
      current->state_change_.Wait(&current->state_mutex_);
    }
  }
}

void GlobalSafepoint::LeaveSafepointScope() {
  if (!FLAG_local_heaps) return;

  DCHECK_GT(active_safepoint_scopes_, 0);
  if (--active_safepoint_scopes_ > 0) return;

  DCHECK_NULL(LocalHeap::Current());

  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    if (current->is_main_thread()) {
      continue;
    }
    current->state_mutex_.Unlock();
  }

  barrier_.Disarm();

  local_heaps_mutex_.Unlock();
}

void GlobalSafepoint::EnterFromThread(LocalHeap* local_heap) {
  {
    base::MutexGuard guard(&local_heap->state_mutex_);
    DCHECK_EQ(local_heap->state_, LocalHeap::ThreadState::Running);
    local_heap->state_ = LocalHeap::ThreadState::Safepoint;
    local_heap->state_change_.NotifyAll();
  }

  barrier_.Wait();

  {
    base::MutexGuard guard(&local_heap->state_mutex_);
    local_heap->state_ = LocalHeap::ThreadState::Running;
  }
}

void GlobalSafepoint::Barrier::Arm() {
  base::MutexGuard guard(&mutex_);
  CHECK(!armed_);
  armed_ = true;
}

void GlobalSafepoint::Barrier::Disarm() {
  base::MutexGuard guard(&mutex_);
  CHECK(armed_);
  armed_ = false;
  cond_.NotifyAll();
}

void GlobalSafepoint::Barrier::Wait() {
  base::MutexGuard guard(&mutex_);
  while (armed_) {
    cond_.Wait(&mutex_);
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
