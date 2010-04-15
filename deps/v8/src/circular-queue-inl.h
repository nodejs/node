// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_CIRCULAR_BUFFER_INL_H_
#define V8_CIRCULAR_BUFFER_INL_H_

#include "circular-queue.h"

namespace v8 {
namespace internal {


template<typename Record>
CircularQueue<Record>::CircularQueue(int desired_buffer_size_in_bytes)
    : buffer_(NewArray<Record>(desired_buffer_size_in_bytes / sizeof(Record))),
      buffer_end_(buffer_ + desired_buffer_size_in_bytes / sizeof(Record)),
      enqueue_semaphore_(
          OS::CreateSemaphore(static_cast<int>(buffer_end_ - buffer_) - 1)),
      enqueue_pos_(buffer_),
      dequeue_pos_(buffer_) {
  // To be able to distinguish between a full and an empty queue
  // state, the queue must be capable of containing at least 2
  // records.
  ASSERT((buffer_end_ - buffer_) >= 2);
}


template<typename Record>
CircularQueue<Record>::~CircularQueue() {
  DeleteArray(buffer_);
  delete enqueue_semaphore_;
}


template<typename Record>
void CircularQueue<Record>::Dequeue(Record* rec) {
  ASSERT(!IsEmpty());
  *rec = *dequeue_pos_;
  dequeue_pos_ = Next(dequeue_pos_);
  // Tell we have a spare record.
  enqueue_semaphore_->Signal();
}


template<typename Record>
void CircularQueue<Record>::Enqueue(const Record& rec) {
  // Wait until we have at least one spare record.
  enqueue_semaphore_->Wait();
  ASSERT(Next(enqueue_pos_) != dequeue_pos_);
  *enqueue_pos_ = rec;
  enqueue_pos_ = Next(enqueue_pos_);
}


template<typename Record>
Record* CircularQueue<Record>::Next(Record* curr) {
  return ++curr != buffer_end_ ? curr : buffer_;
}


void* SamplingCircularQueue::Enqueue() {
  WrapPositionIfNeeded(&producer_pos_->enqueue_pos);
  void* result = producer_pos_->enqueue_pos;
  producer_pos_->enqueue_pos += record_size_;
  return result;
}


void SamplingCircularQueue::WrapPositionIfNeeded(
    SamplingCircularQueue::Cell** pos) {
  if (**pos == kEnd) *pos = buffer_;
}


} }  // namespace v8::internal

#endif  // V8_CIRCULAR_BUFFER_INL_H_
