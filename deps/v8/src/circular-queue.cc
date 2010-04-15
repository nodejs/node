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

#include "v8.h"

#include "circular-queue-inl.h"

namespace v8 {
namespace internal {


SamplingCircularQueue::SamplingCircularQueue(int record_size_in_bytes,
                                             int desired_chunk_size_in_bytes,
                                             int buffer_size_in_chunks)
    : record_size_(record_size_in_bytes / sizeof(Cell)),
      chunk_size_in_bytes_(desired_chunk_size_in_bytes / record_size_in_bytes *
                        record_size_in_bytes),
      chunk_size_(chunk_size_in_bytes_ / sizeof(Cell)),
      buffer_size_(chunk_size_ * buffer_size_in_chunks),
      // The distance ensures that producer and consumer never step on
      // each other's chunks and helps eviction of produced data from
      // the CPU cache (having that chunk size is bigger than the cache.)
      producer_consumer_distance_(2 * chunk_size_),
      buffer_(NewArray<Cell>(buffer_size_ + 1)) {
  ASSERT(buffer_size_in_chunks > 2);
  // Only need to keep the first cell of a chunk clean.
  for (int i = 0; i < buffer_size_; i += chunk_size_) {
    buffer_[i] = kClear;
  }
  buffer_[buffer_size_] = kEnd;

  // Layout producer and consumer position pointers each on their own
  // cache lines to avoid cache lines thrashing due to simultaneous
  // updates of positions by different processor cores.
  const int positions_size =
      RoundUp(1, kProcessorCacheLineSize) +
      RoundUp(static_cast<int>(sizeof(ProducerPosition)),
              kProcessorCacheLineSize) +
      RoundUp(static_cast<int>(sizeof(ConsumerPosition)),
              kProcessorCacheLineSize);
  positions_ = NewArray<byte>(positions_size);

  producer_pos_ = reinterpret_cast<ProducerPosition*>(
      RoundUp(positions_, kProcessorCacheLineSize));
  producer_pos_->enqueue_pos = buffer_;

  consumer_pos_ = reinterpret_cast<ConsumerPosition*>(
      reinterpret_cast<byte*>(producer_pos_) + kProcessorCacheLineSize);
  ASSERT(reinterpret_cast<byte*>(consumer_pos_ + 1) <=
         positions_ + positions_size);
  consumer_pos_->dequeue_chunk_pos = buffer_;
  consumer_pos_->dequeue_chunk_poll_pos = buffer_ + producer_consumer_distance_;
  consumer_pos_->dequeue_pos = NULL;
}


SamplingCircularQueue::~SamplingCircularQueue() {
  DeleteArray(positions_);
  DeleteArray(buffer_);
}


void* SamplingCircularQueue::StartDequeue() {
  if (consumer_pos_->dequeue_pos != NULL) {
    return consumer_pos_->dequeue_pos;
  } else {
    if (*consumer_pos_->dequeue_chunk_poll_pos != kClear) {
      consumer_pos_->dequeue_pos = consumer_pos_->dequeue_chunk_pos;
      consumer_pos_->dequeue_end_pos = consumer_pos_->dequeue_pos + chunk_size_;
      return consumer_pos_->dequeue_pos;
    } else {
      return NULL;
    }
  }
}


void SamplingCircularQueue::FinishDequeue() {
  consumer_pos_->dequeue_pos += record_size_;
  if (consumer_pos_->dequeue_pos < consumer_pos_->dequeue_end_pos) return;
  // Move to next chunk.
  consumer_pos_->dequeue_pos = NULL;
  *consumer_pos_->dequeue_chunk_pos = kClear;
  consumer_pos_->dequeue_chunk_pos += chunk_size_;
  WrapPositionIfNeeded(&consumer_pos_->dequeue_chunk_pos);
  consumer_pos_->dequeue_chunk_poll_pos += chunk_size_;
  WrapPositionIfNeeded(&consumer_pos_->dequeue_chunk_poll_pos);
}


void SamplingCircularQueue::FlushResidualRecords() {
  // Eliminate producer / consumer distance.
  consumer_pos_->dequeue_chunk_poll_pos = consumer_pos_->dequeue_chunk_pos;
}


} }  // namespace v8::internal
