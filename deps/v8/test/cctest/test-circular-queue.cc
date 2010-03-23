// Copyright 2010 the V8 project authors. All rights reserved.
//
// Tests of circular queues.

#include "v8.h"
#include "circular-queue-inl.h"
#include "cctest.h"

namespace i = v8::internal;

using i::CircularQueue;
using i::SamplingCircularQueue;


TEST(SingleRecordCircularQueue) {
  typedef int Record;
  CircularQueue<Record> cq(sizeof(Record) * 2);
  CHECK(cq.IsEmpty());
  cq.Enqueue(1);
  CHECK(!cq.IsEmpty());
  Record rec = 0;
  cq.Dequeue(&rec);
  CHECK_EQ(1, rec);
  CHECK(cq.IsEmpty());
}


TEST(MultipleRecordsCircularQueue) {
  typedef int Record;
  const int kQueueSize = 10;
  CircularQueue<Record> cq(sizeof(Record) * (kQueueSize + 1));
  CHECK(cq.IsEmpty());
  cq.Enqueue(1);
  CHECK(!cq.IsEmpty());
  for (int i = 2; i <= 5; ++i) {
    cq.Enqueue(i);
    CHECK(!cq.IsEmpty());
  }
  Record rec = 0;
  for (int i = 1; i <= 4; ++i) {
    CHECK(!cq.IsEmpty());
    cq.Dequeue(&rec);
    CHECK_EQ(i, rec);
  }
  for (int i = 6; i <= 12; ++i) {
    cq.Enqueue(i);
    CHECK(!cq.IsEmpty());
  }
  for (int i = 5; i <= 12; ++i) {
    CHECK(!cq.IsEmpty());
    cq.Dequeue(&rec);
    CHECK_EQ(i, rec);
  }
  CHECK(cq.IsEmpty());
}


TEST(SamplingCircularQueue) {
  typedef SamplingCircularQueue::Cell Record;
  const int kRecordsPerChunk = 4;
  SamplingCircularQueue scq(sizeof(Record),
                            kRecordsPerChunk * sizeof(Record),
                            3);
  scq.SetUpProducer();
  scq.SetUpConsumer();

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

  scq.TearDownConsumer();
  scq.TearDownProducer();
}
