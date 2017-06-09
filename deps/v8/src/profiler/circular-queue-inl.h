// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_CIRCULAR_QUEUE_INL_H_
#define V8_PROFILER_CIRCULAR_QUEUE_INL_H_

#include "src/profiler/circular-queue.h"

namespace v8 {
namespace internal {

template<typename T, unsigned L>
SamplingCircularQueue<T, L>::SamplingCircularQueue()
    : enqueue_pos_(buffer_),
      dequeue_pos_(buffer_) {
}


template<typename T, unsigned L>
SamplingCircularQueue<T, L>::~SamplingCircularQueue() {
}


template<typename T, unsigned L>
T* SamplingCircularQueue<T, L>::Peek() {
  base::MemoryBarrier();
  if (base::Acquire_Load(&dequeue_pos_->marker) == kFull) {
    return &dequeue_pos_->record;
  }
  return NULL;
}


template<typename T, unsigned L>
void SamplingCircularQueue<T, L>::Remove() {
  base::Release_Store(&dequeue_pos_->marker, kEmpty);
  dequeue_pos_ = Next(dequeue_pos_);
}


template<typename T, unsigned L>
T* SamplingCircularQueue<T, L>::StartEnqueue() {
  base::MemoryBarrier();
  if (base::Acquire_Load(&enqueue_pos_->marker) == kEmpty) {
    return &enqueue_pos_->record;
  }
  return NULL;
}


template<typename T, unsigned L>
void SamplingCircularQueue<T, L>::FinishEnqueue() {
  base::Release_Store(&enqueue_pos_->marker, kFull);
  enqueue_pos_ = Next(enqueue_pos_);
}


template<typename T, unsigned L>
typename SamplingCircularQueue<T, L>::Entry* SamplingCircularQueue<T, L>::Next(
    Entry* entry) {
  Entry* next = entry + 1;
  if (next == &buffer_[L]) return buffer_;
  return next;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_CIRCULAR_QUEUE_INL_H_
