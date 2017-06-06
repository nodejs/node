// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/store-buffer.h"

#include <algorithm>

#include "src/counters.h"
#include "src/heap/incremental-marking.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

StoreBuffer::StoreBuffer(Heap* heap)
    : heap_(heap),
      top_(nullptr),
      current_(0),
      mode_(NOT_IN_GC),
      virtual_memory_(nullptr) {
  for (int i = 0; i < kStoreBuffers; i++) {
    start_[i] = nullptr;
    limit_[i] = nullptr;
    lazy_top_[i] = nullptr;
  }
  task_running_ = false;
  insertion_callback = &InsertDuringRuntime;
  deletion_callback = &DeleteDuringRuntime;
}

void StoreBuffer::SetUp() {
  // Allocate 3x the buffer size, so that we can start the new store buffer
  // aligned to 2x the size.  This lets us use a bit test to detect the end of
  // the area.
  virtual_memory_ = new base::VirtualMemory(kStoreBufferSize * 3);
  uintptr_t start_as_int =
      reinterpret_cast<uintptr_t>(virtual_memory_->address());
  start_[0] =
      reinterpret_cast<Address*>(RoundUp(start_as_int, kStoreBufferSize));
  limit_[0] = start_[0] + (kStoreBufferSize / kPointerSize);
  start_[1] = limit_[0];
  limit_[1] = start_[1] + (kStoreBufferSize / kPointerSize);

  Address* vm_limit = reinterpret_cast<Address*>(
      reinterpret_cast<char*>(virtual_memory_->address()) +
      virtual_memory_->size());

  USE(vm_limit);
  for (int i = 0; i < kStoreBuffers; i++) {
    DCHECK(reinterpret_cast<Address>(start_[i]) >= virtual_memory_->address());
    DCHECK(reinterpret_cast<Address>(limit_[i]) >= virtual_memory_->address());
    DCHECK(start_[i] <= vm_limit);
    DCHECK(limit_[i] <= vm_limit);
    DCHECK((reinterpret_cast<uintptr_t>(limit_[i]) & kStoreBufferMask) == 0);
  }

  if (!virtual_memory_->Commit(reinterpret_cast<Address>(start_[0]),
                               kStoreBufferSize * kStoreBuffers,
                               false)) {  // Not executable.
    V8::FatalProcessOutOfMemory("StoreBuffer::SetUp");
  }
  current_ = 0;
  top_ = start_[current_];
}


void StoreBuffer::TearDown() {
  delete virtual_memory_;
  top_ = nullptr;
  for (int i = 0; i < kStoreBuffers; i++) {
    start_[i] = nullptr;
    limit_[i] = nullptr;
    lazy_top_[i] = nullptr;
  }
}


void StoreBuffer::StoreBufferOverflow(Isolate* isolate) {
  isolate->heap()->store_buffer()->FlipStoreBuffers();
  isolate->counters()->store_buffer_overflows()->Increment();
}

void StoreBuffer::FlipStoreBuffers() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  lazy_top_[current_] = top_;
  current_ = other;
  top_ = start_[current_];

  if (!task_running_ && FLAG_concurrent_store_buffer) {
    task_running_ = true;
    Task* task = new Task(heap_->isolate(), this);
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        task, v8::Platform::kShortRunningTask);
  }
}

void StoreBuffer::MoveEntriesToRememberedSet(int index) {
  if (!lazy_top_[index]) return;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kStoreBuffers);
  for (Address* current = start_[index]; current < lazy_top_[index];
       current++) {
    Address addr = *current;
    Page* page = Page::FromAnyPointerAddress(heap_, addr);
    if (IsDeletionAddress(addr)) {
      current++;
      Address end = *current;
      DCHECK(!IsDeletionAddress(end));
      addr = UnmarkDeletionAddress(addr);
      if (end) {
        RememberedSet<OLD_TO_NEW>::RemoveRange(page, addr, end,
                                               SlotSet::PREFREE_EMPTY_BUCKETS);
      } else {
        RememberedSet<OLD_TO_NEW>::Remove(page, addr);
      }
    } else {
      DCHECK(!IsDeletionAddress(addr));
      RememberedSet<OLD_TO_NEW>::Insert(page, addr);
    }
  }
  lazy_top_[index] = nullptr;
}

void StoreBuffer::MoveAllEntriesToRememberedSet() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  lazy_top_[current_] = top_;
  MoveEntriesToRememberedSet(current_);
  top_ = start_[current_];
}

void StoreBuffer::ConcurrentlyProcessStoreBuffer() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  int other = (current_ + 1) % kStoreBuffers;
  MoveEntriesToRememberedSet(other);
  task_running_ = false;
}

}  // namespace internal
}  // namespace v8
