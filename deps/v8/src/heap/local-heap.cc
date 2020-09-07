// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"

#include <memory>

#include "src/base/platform/mutex.h"
#include "src/handles/local-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/safepoint.h"

namespace v8 {
namespace internal {

LocalHeap::LocalHeap(Heap* heap,
                     std::unique_ptr<PersistentHandles> persistent_handles)
    : heap_(heap),
      state_(ThreadState::Running),
      safepoint_requested_(false),
      allocation_failed_(false),
      prev_(nullptr),
      next_(nullptr),
      handles_(new LocalHandles),
      persistent_handles_(std::move(persistent_handles)),
      old_space_allocator_(this, heap->old_space()) {
  heap_->safepoint()->AddLocalHeap(this);
  if (persistent_handles_) {
    persistent_handles_->Attach(this);
  }
}

LocalHeap::~LocalHeap() {
  // Give up LAB before parking thread
  old_space_allocator_.FreeLinearAllocationArea();

  // Park thread since removing the local heap could block.
  EnsureParkedBeforeDestruction();

  heap_->safepoint()->RemoveLocalHeap(this);
}

Handle<Object> LocalHeap::NewPersistentHandle(Address value) {
  if (!persistent_handles_) {
    persistent_handles_.reset(
        heap_->isolate()->NewPersistentHandles().release());
  }
  return persistent_handles_->NewHandle(value);
}

std::unique_ptr<PersistentHandles> LocalHeap::DetachPersistentHandles() {
  if (persistent_handles_) persistent_handles_->Detach();
  return std::move(persistent_handles_);
}

bool LocalHeap::IsParked() {
  base::MutexGuard guard(&state_mutex_);
  return state_ == ThreadState::Parked;
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

void LocalHeap::FreeLinearAllocationArea() {
  old_space_allocator_.FreeLinearAllocationArea();
}

void LocalHeap::MakeLinearAllocationAreaIterable() {
  old_space_allocator_.MakeLinearAllocationAreaIterable();
}

void LocalHeap::MarkLinearAllocationAreaBlack() {
  old_space_allocator_.MarkLinearAllocationAreaBlack();
}

void LocalHeap::UnmarkLinearAllocationArea() {
  old_space_allocator_.UnmarkLinearAllocationArea();
}

}  // namespace internal
}  // namespace v8
