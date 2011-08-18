// Copyright 2010 the V8 project authors. All rights reserved.
//
// Tests of the circular queue.

#include "v8.h"
#include "circular-queue-inl.h"
#include "cctest.h"

using i::SamplingCircularQueue;


TEST(SamplingCircularQueue) {
  typedef SamplingCircularQueue::Cell Record;
  const int kRecordsPerChunk = 4;
  SamplingCircularQueue scq(sizeof(Record),
                            kRecordsPerChunk * sizeof(Record),
                            3);

  // Check that we are using non-reserved values.
  CHECK_NE(SamplingCircularQueue::kClear, 1);
  CHECK_NE(SamplingCircularQueue::kEnd, 1);
  // Fill up the first chunk.
  CHECK_EQ(NULL, scq.StartDequeue());
  for (Record i = 1; i < 1 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Enqueue());
    CHECK_NE(NULL, rec);
    *rec = i;
    CHECK_EQ(NULL, scq.StartDequeue());
  }

  // Fill up the second chunk. Consumption must still be unavailable.
  CHECK_EQ(NULL, scq.StartDequeue());
  for (Record i = 10; i < 10 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.Enqueue());
    CHECK_NE(NULL, rec);
    *rec = i;
    CHECK_EQ(NULL, scq.StartDequeue());
  }

  Record* rec = reinterpret_cast<Record*>(scq.Enqueue());
  CHECK_NE(NULL, rec);
  *rec = 20;
  // Now as we started filling up the third chunk, consumption
  // must become possible.
  CHECK_NE(NULL, scq.StartDequeue());

  // Consume the first chunk.
  for (Record i = 1; i < 1 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartDequeue());
    CHECK_NE(NULL, rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
    scq.FinishDequeue();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
  }
  // Now consumption must not be possible, as consumer now polls
  // the first chunk for emptinness.
  CHECK_EQ(NULL, scq.StartDequeue());

  scq.FlushResidualRecords();
  // From now, consumer no more polls ahead of the current chunk,
  // so it's possible to consume the second chunk.
  CHECK_NE(NULL, scq.StartDequeue());
  // Consume the second chunk
  for (Record i = 10; i < 10 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartDequeue());
    CHECK_NE(NULL, rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
    scq.FinishDequeue();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
  }
  // Consumption must still be possible as the first cell of the
  // last chunk is not clean.
  CHECK_NE(NULL, scq.StartDequeue());
}


namespace {

class ProducerThread: public i::Thread {
 public:
  typedef SamplingCircularQueue::Cell Record;

  ProducerThread(SamplingCircularQueue* scq,
                 int records_per_chunk,
                 Record value,
                 i::Semaphore* finished)
      : Thread("producer"),
        scq_(scq),
        records_per_chunk_(records_per_chunk),
        value_(value),
        finished_(finished) { }

  virtual void Run() {
    for (Record i = value_; i < value_ + records_per_chunk_; ++i) {
      Record* rec = reinterpret_cast<Record*>(scq_->Enqueue());
      CHECK_NE(NULL, rec);
      *rec = i;
    }

    finished_->Signal();
  }

 private:
  SamplingCircularQueue* scq_;
  const int records_per_chunk_;
  Record value_;
  i::Semaphore* finished_;
};

}  // namespace

TEST(SamplingCircularQueueMultithreading) {
  // Emulate multiple VM threads working 'one thread at a time.'
  // This test enqueues data from different threads. This corresponds
  // to the case of profiling under Linux, where signal handler that
  // does sampling is called in the context of different VM threads.

  typedef ProducerThread::Record Record;
  const int kRecordsPerChunk = 4;
  SamplingCircularQueue scq(sizeof(Record),
                            kRecordsPerChunk * sizeof(Record),
                            3);
  i::Semaphore* semaphore = i::OS::CreateSemaphore(0);
  // Don't poll ahead, making possible to check data in the buffer
  // immediately after enqueuing.
  scq.FlushResidualRecords();

  // Check that we are using non-reserved values.
  CHECK_NE(SamplingCircularQueue::kClear, 1);
  CHECK_NE(SamplingCircularQueue::kEnd, 1);
  ProducerThread producer1(&scq, kRecordsPerChunk, 1, semaphore);
  ProducerThread producer2(&scq, kRecordsPerChunk, 10, semaphore);
  ProducerThread producer3(&scq, kRecordsPerChunk, 20, semaphore);

  CHECK_EQ(NULL, scq.StartDequeue());
  producer1.Start();
  semaphore->Wait();
  for (Record i = 1; i < 1 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartDequeue());
    CHECK_NE(NULL, rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
    scq.FinishDequeue();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
  }

  CHECK_EQ(NULL, scq.StartDequeue());
  producer2.Start();
  semaphore->Wait();
  for (Record i = 10; i < 10 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartDequeue());
    CHECK_NE(NULL, rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
    scq.FinishDequeue();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
  }

  CHECK_EQ(NULL, scq.StartDequeue());
  producer3.Start();
  semaphore->Wait();
  for (Record i = 20; i < 20 + kRecordsPerChunk; ++i) {
    Record* rec = reinterpret_cast<Record*>(scq.StartDequeue());
    CHECK_NE(NULL, rec);
    CHECK_EQ(static_cast<int64_t>(i), static_cast<int64_t>(*rec));
    CHECK_EQ(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
    scq.FinishDequeue();
    CHECK_NE(rec, reinterpret_cast<Record*>(scq.StartDequeue()));
  }

  CHECK_EQ(NULL, scq.StartDequeue());

  delete semaphore;
}
