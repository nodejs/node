// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/microtask-queue.h"

#include <stddef.h>
#include <algorithm>

#include "src/base/logging.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects/microtask.h"
#include "src/roots-inl.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

const size_t MicrotaskQueue::kRingBufferOffset =
    offsetof(MicrotaskQueue, ring_buffer_);
const size_t MicrotaskQueue::kCapacityOffset =
    offsetof(MicrotaskQueue, capacity_);
const size_t MicrotaskQueue::kSizeOffset = offsetof(MicrotaskQueue, size_);
const size_t MicrotaskQueue::kStartOffset = offsetof(MicrotaskQueue, start_);

const intptr_t MicrotaskQueue::kMinimumCapacity = 8;

// static
void MicrotaskQueue::SetUpDefaultMicrotaskQueue(Isolate* isolate) {
  DCHECK_NULL(isolate->default_microtask_queue());

  MicrotaskQueue* microtask_queue = new MicrotaskQueue;
  microtask_queue->next_ = microtask_queue;
  microtask_queue->prev_ = microtask_queue;
  isolate->set_default_microtask_queue(microtask_queue);
}

// static
std::unique_ptr<MicrotaskQueue> MicrotaskQueue::New(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate->default_microtask_queue());

  std::unique_ptr<MicrotaskQueue> microtask_queue(new MicrotaskQueue);

  // Insert the new instance to the next of last MicrotaskQueue instance.
  MicrotaskQueue* last = isolate->default_microtask_queue()->prev_;
  microtask_queue->next_ = last->next_;
  microtask_queue->prev_ = last;
  last->next_->prev_ = microtask_queue.get();
  last->next_ = microtask_queue.get();

  return microtask_queue;
}

MicrotaskQueue::MicrotaskQueue() = default;

MicrotaskQueue::~MicrotaskQueue() {
  if (next_ != this) {
    DCHECK_NE(prev_, this);
    next_->prev_ = prev_;
    prev_->next_ = next_;
  }
  delete[] ring_buffer_;
}

// static
Object* MicrotaskQueue::CallEnqueueMicrotask(Isolate* isolate,
                                             intptr_t microtask_queue_pointer,
                                             Microtask* microtask) {
  reinterpret_cast<MicrotaskQueue*>(microtask_queue_pointer)
      ->EnqueueMicrotask(microtask);
  return ReadOnlyRoots(isolate).undefined_value();
}

void MicrotaskQueue::EnqueueMicrotask(Microtask* microtask) {
  if (size_ == capacity_) {
    // Keep the capacity of |ring_buffer_| power of 2, so that the JIT
    // implementation can calculate the modulo easily.
    intptr_t new_capacity = std::max(kMinimumCapacity, capacity_ << 1);
    ResizeBuffer(new_capacity);
  }

  DCHECK_LT(size_, capacity_);
  ring_buffer_[(start_ + size_) % capacity_] = microtask;
  ++size_;
}

int MicrotaskQueue::RunMicrotasks(Isolate* isolate) {
  HandleScope scope(isolate);
  MaybeHandle<Object> maybe_exception;

  // TODO(tzik): Execution::RunMicrotasks() runs default_microtask_queue.
  // Give it as a parameter to support non-default MicrotaskQueue.
  DCHECK_EQ(this, isolate->default_microtask_queue());
  MaybeHandle<Object> maybe_result = Execution::RunMicrotasks(
      isolate, Execution::MessageHandling::kReport, &maybe_exception);

  // If execution is terminating, clean up and propagate that to the caller.
  if (maybe_result.is_null() && maybe_exception.is_null()) {
    delete[] ring_buffer_;
    ring_buffer_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    start_ = 0;
    return -1;
  }

  // TODO(tzik): Return the number of microtasks run in this round.
  return 0;
}

void MicrotaskQueue::IterateMicrotasks(RootVisitor* visitor) {
  if (size_) {
    // Iterate pending Microtasks as root objects to avoid the write barrier for
    // all single Microtask. If this hurts the GC performance, use a FixedArray.
    visitor->VisitRootPointers(
        Root::kStrongRoots, nullptr, ObjectSlot(ring_buffer_ + start_),
        ObjectSlot(ring_buffer_ + std::min(start_ + size_, capacity_)));
    visitor->VisitRootPointers(
        Root::kStrongRoots, nullptr, ObjectSlot(ring_buffer_),
        ObjectSlot(ring_buffer_ + std::max(start_ + size_ - capacity_,
                                           static_cast<intptr_t>(0))));
  }

  if (capacity_ <= kMinimumCapacity) {
    return;
  }

  intptr_t new_capacity = capacity_;
  while (new_capacity > 2 * size_) {
    new_capacity >>= 1;
  }
  new_capacity = std::max(new_capacity, kMinimumCapacity);
  if (new_capacity < capacity_) {
    ResizeBuffer(new_capacity);
  }
}

void MicrotaskQueue::ResizeBuffer(intptr_t new_capacity) {
  DCHECK_LE(size_, new_capacity);
  Object** new_ring_buffer = new Object*[new_capacity];
  for (intptr_t i = 0; i < size_; ++i) {
    new_ring_buffer[i] = ring_buffer_[(start_ + i) % capacity_];
  }

  delete[] ring_buffer_;
  ring_buffer_ = new_ring_buffer;
  capacity_ = new_capacity;
  start_ = 0;
}

}  // namespace internal
}  // namespace v8
