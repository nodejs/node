// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/list.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {
namespace heap {

class TestChunk {
 public:
  heap::ListNode<TestChunk>& list_node() { return list_node_; }
  const heap::ListNode<TestChunk>& list_node() const { return list_node_; }
  heap::ListNode<TestChunk> list_node_;
};

TEST(List, InsertAtTailAndRemove) {
  List<TestChunk> list;
  EXPECT_TRUE(list.Empty());
  TestChunk t1;
  list.PushBack(&t1);
  EXPECT_FALSE(list.Empty());
  EXPECT_TRUE(list.Contains(&t1));
  list.Remove(&t1);
  EXPECT_TRUE(list.Empty());
}

TEST(List, InsertAtHeadAndRemove) {
  List<TestChunk> list;
  EXPECT_TRUE(list.Empty());
  TestChunk t1;
  list.PushFront(&t1);
  EXPECT_FALSE(list.Empty());
  list.Remove(&t1);
  EXPECT_TRUE(list.Empty());
}

TEST(List, InsertMultipleAtTailAndRemoveFromTail) {
  List<TestChunk> list;
  EXPECT_TRUE(list.Empty());
  const int kSize = 10;
  TestChunk chunks[kSize];
  for (int i = 0; i < kSize; i++) {
    list.PushBack(&chunks[i]);
    EXPECT_EQ(list.back(), &chunks[i]);
  }
  for (int i = kSize - 1; i > 0; i--) {
    list.Remove(&chunks[i]);
    EXPECT_EQ(list.back(), &chunks[i - 1]);
  }

  list.Remove(&chunks[0]);
  EXPECT_TRUE(list.Empty());
}

TEST(List, InsertMultipleAtHeadAndRemoveFromHead) {
  List<TestChunk> list;
  EXPECT_TRUE(list.Empty());
  const int kSize = 10;
  TestChunk chunks[kSize];
  for (int i = 0; i < kSize; i++) {
    list.PushFront(&chunks[i]);
    EXPECT_EQ(list.front(), &chunks[i]);
  }
  for (int i = kSize - 1; i > 0; i--) {
    list.Remove(&chunks[i]);
    EXPECT_EQ(list.front(), &chunks[i - 1]);
  }

  list.Remove(&chunks[0]);
  EXPECT_TRUE(list.Empty());
}

TEST(List, InsertMultipleAtTailAndRemoveFromMiddle) {
  List<TestChunk> list;
  EXPECT_TRUE(list.Empty());
  const int kSize = 10;
  TestChunk chunks[kSize];
  for (int i = 0; i < kSize; i++) {
    list.PushBack(&chunks[i]);
    EXPECT_EQ(list.back(), &chunks[i]);
  }
  int i, j;
  for (i = kSize / 2 - 1, j = kSize / 2; i >= 0; i--, j++) {
    list.Remove(&chunks[i]);
    list.Remove(&chunks[j]);
  }
  EXPECT_TRUE(list.Empty());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
