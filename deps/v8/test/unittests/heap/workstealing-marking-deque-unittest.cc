// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/workstealing-marking-deque.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

class HeapObject {};

TEST(WorkStealingMarkingDeque, LocalEmpty) {
  WorkStealingMarkingDeque marking_deque;
  LocalWorkStealingMarkingDeque local_marking_deque(&marking_deque, 0);
  EXPECT_TRUE(local_marking_deque.IsEmpty());
}

TEST(WorkStealingMarkingDeque, LocalPushPop) {
  WorkStealingMarkingDeque marking_deque;
  LocalWorkStealingMarkingDeque local_marking_deque(&marking_deque, 0);
  HeapObject* object1 = new HeapObject();
  HeapObject* object2 = nullptr;
  EXPECT_TRUE(local_marking_deque.Push(object1));
  EXPECT_FALSE(local_marking_deque.IsEmpty());
  EXPECT_TRUE(local_marking_deque.Pop(&object2));
  EXPECT_EQ(object1, object2);
  delete object1;
}

}  // namespace internal
}  // namespace v8
