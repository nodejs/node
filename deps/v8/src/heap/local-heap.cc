// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"

#include <memory>

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/handles/local-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/safepoint.h"

namespace v8 {
namespace internal {

namespace {
thread_local LocalHeap* current_local_heap = nullptr;
}  // namespace

LocalHeap* LocalHeap::Current() { return current_local_heap; }

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
  DCHECK_NULL(current_local_heap);
  current_local_heap = this;
}

LocalHeap::~LocalHeap() {
  // Give up LAB before parking thread
  old_space_allocator_.FreeLinearAllocationArea();

  // Park thread since removing the local heap could block.
  EnsureParkedBeforeDestruction();

  heap_->safepoint()->RemoveLocalHeap(this);

  DCHECK_EQ(current_local_heap, this);
  current_local_heap = nullptr;
}

void LocalHeap::EnsurePersistentHandles() {
  if (!persistent_handles_) {
    persistent_handles_.reset(
        heap_->isolate()->NewPersistentHandles().release());
    persistent_handles_->Attach(this);
  }
}

std::unique_ptr<PersistentHandles> LocalHeap::DetachPersistentHandles() {
  if (persistent_handles_) persistent_handles_->Detach();
  return std::move(persistent_handles_);
}

#ifdef DEBUG
bool LocalHeap::ContainsPersistentHandle(Address* location) {
  return persistent_handles_ ? persistent_handles_->Contains(location) : false;
}

bool LocalHeap::ContainsLocalHandle(Address* location) {
  return handles_ ? handles_->Contains(location) : false;
}

bool LocalHeap::IsHandleDereferenceAllowed() {
  DCHECK_EQ(LocalHeap::Current(), this);
  return state_ == ThreadState::Running;
}
#endif

bool LocalHeap::IsParked() {
  DCHECK_EQ(LocalHeap::Current(), this);
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

void LocalHeap::ClearSafepointRequested() {
  safepoint_requested_.store(false, std::memory_order_relaxed);
}

void LocalHeap::EnterSafepoint() {
  DCHECK_EQ(LocalHeap::Current(), this);
  if (state_ == ThreadState::Running) heap_->safepoint()->EnterFromThread(this);
}

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

Address LocalHeap::PerformCollectionAndAllocateAgain(
    int object_size, AllocationType type, AllocationOrigin origin,
    AllocationAlignment alignment) {
  allocation_failed_ = true;
  static const int kMaxNumberOfRetries = 3;

  for (int i = 0; i < kMaxNumberOfRetries; i++) {
    {
      ParkedScope scope(this);
      heap_->RequestAndWaitForCollection();
    }

    AllocationResult result = AllocateRaw(object_size, type, origin, alignment);
    if (!result.IsRetry()) {
      allocation_failed_ = false;
      return result.ToObjectChecked().address();
    }
  }

  heap_->FatalProcessOutOfMemory("LocalHeap: allocation failed");
}

}  // namespace internal
}  // namespace v8
