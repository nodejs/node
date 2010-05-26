// Copyright 2010 the V8 project authors. All rights reserved.
//
// Tests of the unbound queue.

#include "v8.h"
#include "unbound-queue-inl.h"
#include "cctest.h"

namespace i = v8::internal;

using i::UnboundQueue;


TEST(SingleRecord) {
  typedef int Record;
  UnboundQueue<Record> cq;
  CHECK(cq.IsEmpty());
  cq.Enqueue(1);
  CHECK(!cq.IsEmpty());
  Record rec = 0;
  cq.Dequeue(&rec);
  CHECK_EQ(1, rec);
  CHECK(cq.IsEmpty());
}


TEST(MultipleRecords) {
  typedef int Record;
  UnboundQueue<Record> cq;
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

