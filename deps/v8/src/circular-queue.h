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

#ifndef V8_CIRCULAR_QUEUE_H_
#define V8_CIRCULAR_QUEUE_H_

#include "v8globals.h"

namespace v8 {
namespace internal {


// Lock-free cache-friendly sampling circular queue for large
// records. Intended for fast transfer of large records between a
// single producer and a single consumer. If the queue is full,
// StartEnqueue will return NULL. The queue is designed with
// a goal in mind to evade cache lines thrashing by preventing
// simultaneous reads and writes to adjanced memory locations.
template<typename T, unsigned Length>
class SamplingCircularQueue {
 public:
  // Executed on the application thread.
  SamplingCircularQueue();
  ~SamplingCircularQueue();

  // StartEnqueue returns a pointer to a memory location for storing the next
  // record or NULL if all entries are full at the moment.
  T* StartEnqueue();
  // Notifies the queue that the producer has complete writing data into the
  // memory returned by StartEnqueue and it can be passed to the consumer.
  void FinishEnqueue();

  // Executed on the consumer (analyzer) thread.
  // Retrieves, but does not remove, the head of this queue, returning NULL
  // if this queue is empty. After the record had been read by a consumer,
  // Remove must be called.
  T* Peek();
  void Remove();

 private:
  // Reserved values for the entry marker.
  enum {
    kEmpty,  // Marks clean (processed) entries.
    kFull    // Marks entries already filled by the producer but not yet
             // completely processed by the consumer.
  };

  struct V8_ALIGNED(PROCESSOR_CACHE_LINE_SIZE) Entry {
    Entry() : marker(kEmpty) {}
    T record;
    Atomic32 marker;
  };

  Entry* Next(Entry* entry);

  Entry buffer_[Length];
  V8_ALIGNED(PROCESSOR_CACHE_LINE_SIZE) Entry* enqueue_pos_;
  V8_ALIGNED(PROCESSOR_CACHE_LINE_SIZE) Entry* dequeue_pos_;

  DISALLOW_COPY_AND_ASSIGN(SamplingCircularQueue);
};


} }  // namespace v8::internal

#endif  // V8_CIRCULAR_QUEUE_H_
