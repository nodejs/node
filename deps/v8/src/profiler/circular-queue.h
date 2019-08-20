// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_CIRCULAR_QUEUE_H_
#define V8_PROFILER_CIRCULAR_QUEUE_H_

#include "src/base/atomicops.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Lock-free cache-friendly sampling circular queue for large
// records. Intended for fast transfer of large records between a
// single producer and a single consumer. If the queue is full,
// StartEnqueue will return nullptr. The queue is designed with
// a goal in mind to evade cache lines thrashing by preventing
// simultaneous reads and writes to adjanced memory locations.
template<typename T, unsigned Length>
class SamplingCircularQueue {
 public:
  // Executed on the application thread.
  SamplingCircularQueue();
  ~SamplingCircularQueue();

  // StartEnqueue returns a pointer to a memory location for storing the next
  // record or nullptr if all entries are full at the moment.
  T* StartEnqueue();
  // Notifies the queue that the producer has complete writing data into the
  // memory returned by StartEnqueue and it can be passed to the consumer.
  void FinishEnqueue();

  // Executed on the consumer (analyzer) thread.
  // Retrieves, but does not remove, the head of this queue, returning nullptr
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

  struct alignas(PROCESSOR_CACHE_LINE_SIZE) Entry {
    Entry() : marker(kEmpty) {}
    T record;
    base::Atomic32 marker;
  };

  Entry* Next(Entry* entry);

  Entry buffer_[Length];
  alignas(PROCESSOR_CACHE_LINE_SIZE) Entry* enqueue_pos_;
  alignas(PROCESSOR_CACHE_LINE_SIZE) Entry* dequeue_pos_;

  DISALLOW_COPY_AND_ASSIGN(SamplingCircularQueue);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_CIRCULAR_QUEUE_H_
