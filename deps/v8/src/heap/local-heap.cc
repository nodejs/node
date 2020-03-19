// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

LocalHeap::LocalHeap(Heap* heap)
    : heap_(heap),
      state_(ThreadState::Running),
      prev_(nullptr),
      next_(nullptr) {
  heap_->AddLocalHeap(this);
}

LocalHeap::~LocalHeap() { heap_->RemoveLocalHeap(this); }

void LocalHeap::Park() {
  base::MutexGuard guard(&state_mutex_);
  CHECK(state_ == ThreadState::Running);
  state_ = ThreadState::Parked;
}

void LocalHeap::Unpark() {
  base::MutexGuard guard(&state_mutex_);
  CHECK(state_ == ThreadState::Parked);
  state_ = ThreadState::Running;
}

void LocalHeap::RequestSafepoint() {
  safepoint_requested_.store(true, std::memory_order_relaxed);
}

bool LocalHeap::IsSafepointRequested() {
  return safepoint_requested_.load(std::memory_order_relaxed);
}

void LocalHeap::Safepoint() {
  if (IsSafepointRequested()) {
    EnterSafepoint();
  }
}

void LocalHeap::ClearSafepointRequested() {
  safepoint_requested_.store(false, std::memory_order_relaxed);
}

void LocalHeap::EnterSafepoint() { UNIMPLEMENTED(); }

}  // namespace internal
}  // namespace v8
