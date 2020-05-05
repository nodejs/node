// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"
#include "src/handles/local-handles.h"
#include "src/heap/heap.h"
#include "src/heap/safepoint.h"

namespace v8 {
namespace internal {

LocalHeap::LocalHeap(Heap* heap)
    : heap_(heap),
      state_(ThreadState::Running),
      safepoint_requested_(false),
      prev_(nullptr),
      next_(nullptr),
      handles_(new LocalHandles) {
  heap_->safepoint()->AddLocalHeap(this);
}

LocalHeap::~LocalHeap() {
  // Park thread since removing the local heap could block.
  EnsureParkedBeforeDestruction();

  heap_->safepoint()->RemoveLocalHeap(this);
}

void LocalHeap::Park() {
  base::MutexGuard guard(&state_mutex_);
  CHECK(state_ == ThreadState::Running);
  state_ = ThreadState::Parked;
  state_change_.NotifyAll();
}

void LocalHeap::Unpark() {
  base::MutexGuard guard(&state_mutex_);
  CHECK(state_ == ThreadState::Parked);
  state_ = ThreadState::Running;
}

void LocalHeap::EnsureParkedBeforeDestruction() {
  base::MutexGuard guard(&state_mutex_);
  state_ = ThreadState::Parked;
  state_change_.NotifyAll();
}

void LocalHeap::RequestSafepoint() {
  safepoint_requested_.store(true, std::memory_order_relaxed);
}

bool LocalHeap::IsSafepointRequested() {
  return safepoint_requested_.load(std::memory_order_relaxed);
}

void LocalHeap::Safepoint() {
  if (IsSafepointRequested()) {
    ClearSafepointRequested();
    EnterSafepoint();
  }
}

void LocalHeap::ClearSafepointRequested() {
  safepoint_requested_.store(false, std::memory_order_relaxed);
}

void LocalHeap::EnterSafepoint() { heap_->safepoint()->EnterFromThread(this); }

}  // namespace internal
}  // namespace v8
