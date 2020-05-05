// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/safepoint.h"

#include "src/handles/local-handles.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

Safepoint::Safepoint(Heap* heap) : heap_(heap), local_heaps_head_(nullptr) {}

void Safepoint::Start() { StopThreads(); }

void Safepoint::End() { ResumeThreads(); }

void Safepoint::StopThreads() {
  local_heaps_mutex_.Lock();

  barrier_.Arm();

  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->RequestSafepoint();
  }

  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->state_mutex_.Lock();

    while (current->state_ == LocalHeap::ThreadState::Running) {
      current->state_change_.Wait(&current->state_mutex_);
    }
  }
}

void Safepoint::ResumeThreads() {
  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->state_mutex_.Unlock();
  }

  barrier_.Disarm();

  local_heaps_mutex_.Unlock();
}

void Safepoint::EnterFromThread(LocalHeap* local_heap) {
  {
    base::MutexGuard guard(&local_heap->state_mutex_);
    local_heap->state_ = LocalHeap::ThreadState::Safepoint;
    local_heap->state_change_.NotifyAll();
  }

  barrier_.Wait();

  {
    base::MutexGuard guard(&local_heap->state_mutex_);
    local_heap->state_ = LocalHeap::ThreadState::Running;
  }
}

void Safepoint::Barrier::Arm() {
  base::MutexGuard guard(&mutex_);
  CHECK(!armed_);
  armed_ = true;
}

void Safepoint::Barrier::Disarm() {
  base::MutexGuard guard(&mutex_);
  CHECK(armed_);
  armed_ = false;
  cond_.NotifyAll();
}

void Safepoint::Barrier::Wait() {
  base::MutexGuard guard(&mutex_);
  while (armed_) {
    cond_.Wait(&mutex_);
  }
}

SafepointScope::SafepointScope(Heap* heap) : safepoint_(heap->safepoint()) {
  safepoint_->StopThreads();
}

SafepointScope::~SafepointScope() { safepoint_->ResumeThreads(); }

void Safepoint::AddLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  if (local_heaps_head_) local_heaps_head_->prev_ = local_heap;
  local_heap->prev_ = nullptr;
  local_heap->next_ = local_heaps_head_;
  local_heaps_head_ = local_heap;
}

void Safepoint::RemoveLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  if (local_heap->next_) local_heap->next_->prev_ = local_heap->prev_;
  if (local_heap->prev_)
    local_heap->prev_->next_ = local_heap->next_;
  else
    local_heaps_head_ = local_heap->next_;
}

bool Safepoint::ContainsLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  LocalHeap* current = local_heaps_head_;

  while (current) {
    if (current == local_heap) return true;
    current = current->next_;
  }

  return false;
}

bool Safepoint::ContainsAnyLocalHeap() {
  base::MutexGuard guard(&local_heaps_mutex_);
  return local_heaps_head_ != nullptr;
}

void Safepoint::Iterate(RootVisitor* visitor) {
  for (LocalHeap* current = local_heaps_head_; current;
       current = current->next_) {
    current->handles()->Iterate(visitor);
  }
}

}  // namespace internal
}  // namespace v8
