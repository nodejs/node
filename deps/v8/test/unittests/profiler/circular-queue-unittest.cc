// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests of the circular queue.
#include "src/init/v8.h"
#include "src/profiler/circular-queue-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using i::SamplingCircularQueue;
using CircularQueueTest = ::testing::Test;

TEST_F(CircularQueueTest, SamplingCircularQueue) {
  using Record = v8::base::AtomicWord;
  const int kMaxRecordsInQueue = 4;
  SamplingCircularQueue<Record, kMaxRecordsInQueue> scq;

  // Check that we are using non-reserved values.
  // Fill up the first chunk.
  CHECK(!scq.Peek());
  for (Record i = 1; i < 1 + kMaxRecordsInQueue; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartEnqueue());
    CHECK(rec);
    *rec = i;
    scq.FinishEnqueue();
  }

  // The queue is full, enqueue is not allowed.
  CHECK(!scq.StartEnqueue());

  // Try to enqueue when the the queue is full. Consumption must be available.
  CHECK(scq.Peek());
  for (int i = 0; i < 10; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartEnqueue());
    CHECK(!rec);
    CHECK(scq.Peek());
  }

  // Consume all records.
  for (Record i = 1; i < 1 + kMaxRecordsInQueue; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Peek());
    CHECK(rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.Peek()));
    scq.Remove();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.Peek()));
  }
  // The queue is empty.
  CHECK(!scq.Peek());

  CHECK(!scq.Peek());
  for (Record i = 0; i < kMaxRecordsInQueue / 2; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartEnqueue());
    CHECK(rec);
    *rec = i;
    scq.FinishEnqueue();
  }

  // Consume all available kMaxRecordsInQueue / 2 records.
  CHECK(scq.Peek());
  for (Record i = 0; i < kMaxRecordsInQueue / 2; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Peek());
    CHECK(rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.Peek()));
    scq.Remove();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.Peek()));
  }

  // The queue is empty.
  CHECK(!scq.Peek());
}

namespace {

using Record = v8::base::AtomicWord;
using TestSampleQueue = SamplingCircularQueue<Record, 12>;

class ProducerThread : public v8::base::Thread {
 public:
  ProducerThread(TestSampleQueue* scq, int records_per_chunk, Record value,
                 v8::base::Semaphore* finished)
      : Thread(Options("producer")),
        scq_(scq),
        records_per_chunk_(records_per_chunk),
        value_(value),
        finished_(finished) {}

  void Run() override {
    for (Record i = value_; i < value_ + records_per_chunk_; ++i) {
      Record* rec = reinterpret_cast<Record*>(scq_->StartEnqueue());
      CHECK(rec);
      *rec = i;
      scq_->FinishEnqueue();
    }

    finished_->Signal();
  }

 private:
  TestSampleQueue* scq_;
  const int records_per_chunk_;
  Record value_;
  v8::base::Semaphore* finished_;
};

}  // namespace

TEST_F(CircularQueueTest, SamplingCircularQueueMultithreading) {
  // Emulate multiple VM threads working 'one thread at a time.'
  // This test enqueues data from different threads. This corresponds
  // to the case of profiling under Linux, where signal handler that
  // does sampling is called in the context of different VM threads.

  const int kRecordsPerChunk = 4;
  TestSampleQueue scq;
  v8::base::Semaphore semaphore(0);

  ProducerThread producer1(&scq, kRecordsPerChunk, 1, &semaphore);
  ProducerThread producer2(&scq, kRecordsPerChunk, 10, &semaphore);
  ProducerThread producer3(&scq, kRecordsPerChunk, 20, &semaphore);

  CHECK(!scq.Peek());
  CHECK(producer1.Start());
  semaphore.Wait();
  for (Record i = 1; i < 1 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Peek());
    CHECK(rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.Peek()));
    scq.Remove();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.Peek()));
  }

  CHECK(!scq.Peek());
  CHECK(producer2.Start());
  semaphore.Wait();
  for (Record i = 10; i < 10 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Peek());
    CHECK(rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.Peek()));
    scq.Remove();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.Peek()));
  }

  CHECK(!scq.Peek());
  CHECK(producer3.Start());
  semaphore.Wait();
  for (Record i = 20; i < 20 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Peek());
    CHECK(rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.Peek()));
    scq.Remove();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.Peek()));
  }

  CHECK(!scq.Peek());
}
