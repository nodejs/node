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

namespace v8 {
namespace internal {


// Lock-based blocking circular queue for small records.  Intended for
// transfer of small records between a single producer and a single
// consumer. Blocks on enqueue operation if the queue is full.
template<typename Record>
class CircularQueue {
 public:
  inline explicit CircularQueue(int desired_buffer_size_in_bytes);
  inline ~CircularQueue();

  INLINE(void Dequeue(Record* rec));
  INLINE(void Enqueue(const Record& rec));
  INLINE(bool IsEmpty()) { return enqueue_pos_ == dequeue_pos_; }

 private:
  INLINE(Record* Next(Record* curr));

  Record* buffer_;
  Record* const buffer_end_;
  Semaphore* enqueue_semaphore_;
  Record* enqueue_pos_;
  Record* dequeue_pos_;

  DISALLOW_COPY_AND_ASSIGN(CircularQueue);
};


// Lock-free cache-friendly sampling circular queue for large
// records. Intended for fast transfer of large records between a
// single producer and a single consumer. If the queue is full,
// previous unread records are overwritten. The queue is designed with
// a goal in mind to evade cache lines thrashing by preventing
// simultaneous reads and writes to adjanced memory locations.
//
// IMPORTANT: as a producer never checks for chunks cleanness, it is
// possible that it can catch up and overwrite a chunk that a consumer
// is currently reading, resulting in a corrupt record being read.
class SamplingCircularQueue {
 public:
  // Executed on the application thread.
  SamplingCircularQueue(int record_size_in_bytes,
                        int desired_chunk_size_in_bytes,
                        int buffer_size_in_chunks);
  ~SamplingCircularQueue();

  // Enqueue returns a pointer to a memory location for storing the next
  // record.
  INLINE(void* Enqueue());

  // Executed on the consumer (analyzer) thread.
  // StartDequeue returns a pointer to a memory location for retrieving
  // the next record. After the record had been read by a consumer,
  // FinishDequeue must be called. Until that moment, subsequent calls
  // to StartDequeue will return the same pointer.
  void* StartDequeue();
  void FinishDequeue();
  // Due to a presence of slipping between the producer and the consumer,
  // the queue must be notified whether producing has been finished in order
  // to process remaining records from the buffer.
  void FlushResidualRecords();

  typedef AtomicWord Cell;
  // Reserved values for the first cell of a record.
  static const Cell kClear = 0;  // Marks clean (processed) chunks.
  static const Cell kEnd = -1;   // Marks the end of the buffer.

 private:
  struct ProducerPosition {
    Cell* enqueue_pos;
  };
  struct ConsumerPosition {
    Cell* dequeue_chunk_pos;
    Cell* dequeue_chunk_poll_pos;
    Cell* dequeue_pos;
    Cell* dequeue_end_pos;
  };

  INLINE(void WrapPositionIfNeeded(Cell** pos));

  const int record_size_;
  const int chunk_size_in_bytes_;
  const int chunk_size_;
  const int buffer_size_;
  const int producer_consumer_distance_;
  Cell* buffer_;
  byte* positions_;
  ProducerPosition* producer_pos_;
  ConsumerPosition* consumer_pos_;
};


} }  // namespace v8::internal

#endif  // V8_CIRCULAR_QUEUE_H_
