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
//
// Tests of the unbound queue.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/unbound-queue-inl.h"

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
