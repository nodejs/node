// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_CIRCULAR_QUEUE_INL_H_
#define V8_CIRCULAR_QUEUE_INL_H_

#include "circular-queue.h"

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
  MemoryBarrier();
  if (Acquire_Load(&dequeue_pos_->marker) == kFull) {
    return &dequeue_pos_->record;
  }
  return NULL;
}


template<typename T, unsigned L>
void SamplingCircularQueue<T, L>::Remove() {
  Release_Store(&dequeue_pos_->marker, kEmpty);
  dequeue_pos_ = Next(dequeue_pos_);
}


template<typename T, unsigned L>
T* SamplingCircularQueue<T, L>::StartEnqueue() {
  MemoryBarrier();
  if (Acquire_Load(&enqueue_pos_->marker) == kEmpty) {
    return &enqueue_pos_->record;
  }
  return NULL;
}


template<typename T, unsigned L>
void SamplingCircularQueue<T, L>::FinishEnqueue() {
  Release_Store(&enqueue_pos_->marker, kFull);
  enqueue_pos_ = Next(enqueue_pos_);
}


template<typename T, unsigned L>
typename SamplingCircularQueue<T, L>::Entry* SamplingCircularQueue<T, L>::Next(
    Entry* entry) {
  Entry* next = entry + 1;
  if (next == &buffer_[L]) return buffer_;
  return next;
}

} }  // namespace v8::internal

#endif  // V8_CIRCULAR_QUEUE_INL_H_
