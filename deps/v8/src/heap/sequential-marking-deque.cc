// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/sequential-marking-deque.h"

#include "src/allocation.h"
#include "src/base/bits.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

void SequentialMarkingDeque::SetUp() {
  backing_store_ = new base::VirtualMemory(kMaxSize);
  backing_store_committed_size_ = 0;
  if (backing_store_ == nullptr) {
    V8::FatalProcessOutOfMemory("SequentialMarkingDeque::SetUp");
  }
}

void SequentialMarkingDeque::TearDown() { delete backing_store_; }

void SequentialMarkingDeque::StartUsing() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (in_use_) {
    // This can happen in mark-compact GC if the incremental marker already
    // started using the marking deque.
    return;
  }
  in_use_ = true;
  EnsureCommitted();
  array_ = reinterpret_cast<HeapObject**>(backing_store_->address());
  size_t size = FLAG_force_marking_deque_overflows
                    ? 64 * kPointerSize
                    : backing_store_committed_size_;
  DCHECK(
      base::bits::IsPowerOfTwo32(static_cast<uint32_t>(size / kPointerSize)));
  mask_ = static_cast<int>((size / kPointerSize) - 1);
  top_ = bottom_ = 0;
  overflowed_ = false;
}

void SequentialMarkingDeque::StopUsing() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (!in_use_) return;
  DCHECK(IsEmpty());
  DCHECK(!overflowed_);
  top_ = bottom_ = mask_ = 0;
  in_use_ = false;
  if (FLAG_concurrent_sweeping) {
    StartUncommitTask();
  } else {
    Uncommit();
  }
}

void SequentialMarkingDeque::Clear() {
  DCHECK(in_use_);
  top_ = bottom_ = 0;
  overflowed_ = false;
}

void SequentialMarkingDeque::Uncommit() {
  DCHECK(!in_use_);
  bool success = backing_store_->Uncommit(backing_store_->address(),
                                          backing_store_committed_size_);
  backing_store_committed_size_ = 0;
  CHECK(success);
}

void SequentialMarkingDeque::EnsureCommitted() {
  DCHECK(in_use_);
  if (backing_store_committed_size_ > 0) return;

  for (size_t size = kMaxSize; size >= kMinSize; size /= 2) {
    if (backing_store_->Commit(backing_store_->address(), size, false)) {
      backing_store_committed_size_ = size;
      break;
    }
  }
  if (backing_store_committed_size_ == 0) {
    V8::FatalProcessOutOfMemory("SequentialMarkingDeque::EnsureCommitted");
  }
}

void SequentialMarkingDeque::StartUncommitTask() {
  if (!uncommit_task_pending_) {
    uncommit_task_pending_ = true;
    UncommitTask* task = new UncommitTask(heap_->isolate(), this);
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        task, v8::Platform::kShortRunningTask);
  }
}

}  // namespace internal
}  // namespace v8
