// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/globals.h"
#include "src/heap/concurrent-marking-deque.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class ConcurrentMarkingDequeTest : public TestWithIsolate {
 public:
  ConcurrentMarkingDequeTest() {
    marking_deque_ = new ConcurrentMarkingDeque(i_isolate()->heap());
    object_ = i_isolate()->heap()->undefined_value();
  }

  ~ConcurrentMarkingDequeTest() { delete marking_deque_; }

  ConcurrentMarkingDeque* marking_deque() { return marking_deque_; }

  HeapObject* object() { return object_; }

 private:
  ConcurrentMarkingDeque* marking_deque_;
  HeapObject* object_;
  DISALLOW_COPY_AND_ASSIGN(ConcurrentMarkingDequeTest);
};

TEST_F(ConcurrentMarkingDequeTest, Empty) {
  EXPECT_TRUE(marking_deque()->IsEmpty());
  EXPECT_EQ(0, marking_deque()->Size());
}

TEST_F(ConcurrentMarkingDequeTest, SharedDeque) {
  marking_deque()->Push(object());
  EXPECT_FALSE(marking_deque()->IsEmpty());
  EXPECT_EQ(1, marking_deque()->Size());
  EXPECT_EQ(object(), marking_deque()->Pop(MarkingThread::kConcurrent));
}

TEST_F(ConcurrentMarkingDequeTest, BailoutDeque) {
  marking_deque()->Push(object(), MarkingThread::kConcurrent,
                        TargetDeque::kBailout);
  EXPECT_FALSE(marking_deque()->IsEmpty());
  EXPECT_EQ(1, marking_deque()->Size());
  EXPECT_EQ(nullptr, marking_deque()->Pop(MarkingThread::kConcurrent));
}

}  // namespace internal
}  // namespace v8
