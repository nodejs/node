// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/worklist.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace heap {
namespace base {

class SomeObject {};

using TestWorklist = Worklist<SomeObject*, 64>;

TEST(WorkListTest, SegmentCreate) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_EQ(0u, segment.Size());
  EXPECT_FALSE(segment.IsFull());
}

TEST(WorkListTest, SegmentPush) {
  TestWorklist::Segment segment;
  EXPECT_EQ(0u, segment.Size());
  segment.Push(nullptr);
  EXPECT_EQ(1u, segment.Size());
}

TEST(WorkListTest, SegmentPushPop) {
  TestWorklist::Segment segment;
  segment.Push(nullptr);
  EXPECT_EQ(1u, segment.Size());
  SomeObject dummy;
  SomeObject* object = &dummy;
  segment.Pop(&object);
  EXPECT_EQ(0u, segment.Size());
  EXPECT_EQ(nullptr, object);
}

TEST(WorkListTest, SegmentIsEmpty) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  segment.Push(nullptr);
  EXPECT_FALSE(segment.IsEmpty());
}

TEST(WorkListTest, SegmentIsFull) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kSize; i++) {
    segment.Push(nullptr);
  }
  EXPECT_TRUE(segment.IsFull());
}

TEST(WorkListTest, SegmentClear) {
  TestWorklist::Segment segment;
  segment.Push(nullptr);
  EXPECT_FALSE(segment.IsEmpty());
  segment.Clear();
  EXPECT_TRUE(segment.IsEmpty());
  for (size_t i = 0; i < TestWorklist::Segment::kSize; i++) {
    segment.Push(nullptr);
  }
}

TEST(WorkListTest, SegmentUpdateFalse) {
  TestWorklist::Segment segment;
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  segment.Push(object);
  segment.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(segment.IsEmpty());
}

TEST(WorkListTest, SegmentUpdate) {
  TestWorklist::Segment segment;
  SomeObject* objectA;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  segment.Push(objectA);
  segment.Update([objectB](SomeObject* object, SomeObject** out) {
    *out = objectB;
    return true;
  });
  SomeObject* object;
  segment.Pop(&object);
  EXPECT_EQ(object, objectB);
}

TEST(WorkListTest, CreateEmpty) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  EXPECT_TRUE(worklist_local.IsLocalEmpty());
  EXPECT_TRUE(worklist.IsEmpty());
}

TEST(WorkListTest, LocalPushPop) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  worklist_local.Push(&dummy);
  EXPECT_FALSE(worklist_local.IsLocalEmpty());
  EXPECT_TRUE(worklist_local.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
}

TEST(WorkListTest, LocalPushStaysPrivate) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local1(&worklist);
  TestWorklist::Local worklist_local2(&worklist);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.Size());
  worklist_local1.Push(&dummy);
  EXPECT_EQ(0U, worklist.Size());
  EXPECT_FALSE(worklist_local2.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_TRUE(worklist_local1.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_EQ(0U, worklist.Size());
}

TEST(WorkListTest, LocalClear) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  // Check push segment:
  EXPECT_TRUE(worklist_local.IsEmpty());
  worklist_local.Push(object);
  EXPECT_FALSE(worklist_local.IsEmpty());
  worklist_local.Clear();
  EXPECT_TRUE(worklist_local.IsEmpty());
  // Check pop segment:
  worklist_local.Push(object);
  worklist_local.Push(object);
  EXPECT_FALSE(worklist_local.IsEmpty());
  worklist_local.Publish();
  EXPECT_TRUE(worklist_local.IsEmpty());
  SomeObject* retrieved;
  worklist_local.Pop(&retrieved);
  EXPECT_FALSE(worklist_local.IsEmpty());
  worklist_local.Clear();
  EXPECT_TRUE(worklist_local.IsEmpty());
}

TEST(WorkListTest, GlobalUpdateNull) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local.Push(object);
  }
  worklist_local.Push(object);
  worklist_local.Publish();
  worklist.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.Size());
}

TEST(WorkListTest, GlobalUpdate) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB = nullptr;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  SomeObject* objectC = nullptr;
  objectC = reinterpret_cast<SomeObject*>(&objectC);
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local.Push(objectA);
  }
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local.Push(objectB);
  }
  worklist_local.Push(objectA);
  worklist_local.Publish();
  worklist.Update([objectA, objectC](SomeObject* object, SomeObject** out) {
    if (object != objectA) {
      *out = objectC;
      return true;
    }
    return false;
  });
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    SomeObject* object;
    EXPECT_TRUE(worklist_local.Pop(&object));
    EXPECT_EQ(object, objectC);
  }
}

TEST(WorkListTest, FlushToGlobalPushSegment) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local0(&worklist);
  TestWorklist::Local worklist_local1(&worklist);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  worklist_local0.Push(objectA);
  worklist_local0.Publish();
  EXPECT_EQ(1U, worklist.Size());
  EXPECT_TRUE(worklist_local1.Pop(&object));
}

TEST(WorkListTest, FlushToGlobalPopSegment) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local0(&worklist);
  TestWorklist::Local worklist_local1(&worklist);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  worklist_local0.Push(objectA);
  worklist_local0.Push(objectA);
  worklist_local0.Pop(&object);
  worklist_local0.Publish();
  EXPECT_EQ(1U, worklist.Size());
  EXPECT_TRUE(worklist_local1.Pop(&object));
}

TEST(WorkListTest, Clear) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local(&worklist);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  worklist_local.Push(object);
  worklist_local.Publish();
  EXPECT_EQ(1U, worklist.Size());
  worklist.Clear();
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.Size());
}

TEST(WorkListTest, SingleSegmentSteal) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local1(&worklist);
  TestWorklist::Local worklist_local2(&worklist);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local1.Push(&dummy);
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  worklist_local1.Push(nullptr);
  EXPECT_TRUE(worklist_local1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_EQ(1U, worklist.Size());
  // Stealing.
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    EXPECT_TRUE(worklist_local2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_local1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.Size());
}

TEST(WorkListTest, MultipleSegmentsStolen) {
  TestWorklist worklist;
  TestWorklist::Local worklist_local1(&worklist);
  TestWorklist::Local worklist_local2(&worklist);
  TestWorklist::Local worklist_local3(&worklist);
  SomeObject dummy1;
  SomeObject dummy2;
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local1.Push(&dummy1);
  }
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local1.Push(&dummy2);
  }
  SomeObject* retrieved = nullptr;
  SomeObject dummy3;
  // One more push/pop to publish the full segment.
  worklist_local1.Push(&dummy3);
  EXPECT_TRUE(worklist_local1.Pop(&retrieved));
  EXPECT_EQ(&dummy3, retrieved);
  EXPECT_EQ(2U, worklist.Size());
  // Stealing.
  EXPECT_TRUE(worklist_local2.Pop(&retrieved));
  SomeObject* const expect_bag2 = retrieved;
  EXPECT_TRUE(worklist_local3.Pop(&retrieved));
  SomeObject* const expect_bag3 = retrieved;
  EXPECT_EQ(0U, worklist.Size());
  EXPECT_NE(expect_bag2, expect_bag3);
  EXPECT_TRUE(expect_bag2 == &dummy1 || expect_bag2 == &dummy2);
  EXPECT_TRUE(expect_bag3 == &dummy1 || expect_bag3 == &dummy2);
  for (size_t i = 1; i < TestWorklist::kSegmentSize; i++) {
    EXPECT_TRUE(worklist_local2.Pop(&retrieved));
    EXPECT_EQ(expect_bag2, retrieved);
    EXPECT_FALSE(worklist_local1.Pop(&retrieved));
  }
  for (size_t i = 1; i < TestWorklist::kSegmentSize; i++) {
    EXPECT_TRUE(worklist_local3.Pop(&retrieved));
    EXPECT_EQ(expect_bag3, retrieved);
    EXPECT_FALSE(worklist_local1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsEmpty());
}

TEST(WorkListTest, MergeGlobalPool) {
  TestWorklist worklist1;
  TestWorklist::Local worklist_local1(&worklist1);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    worklist_local1.Push(&dummy);
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  worklist_local1.Push(nullptr);
  EXPECT_TRUE(worklist_local1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_EQ(1U, worklist1.Size());
  // Merging global pool into a new Worklist.
  TestWorklist worklist2;
  TestWorklist::Local worklist_local2(&worklist2);
  EXPECT_EQ(0U, worklist2.Size());
  worklist2.Merge(&worklist1);
  EXPECT_EQ(1U, worklist2.Size());
  EXPECT_FALSE(worklist2.IsEmpty());
  for (size_t i = 0; i < TestWorklist::kSegmentSize; i++) {
    EXPECT_TRUE(worklist_local2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_local1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist1.IsEmpty());
  EXPECT_TRUE(worklist2.IsEmpty());
}

TEST(WorkListTest, SwapGlobalPool) {
  TestWorklist worklist1;
  TestWorklist::Local worklist_local1(&worklist1);
  SomeObject dummy;
  worklist_local1.Push(&dummy);
  worklist_local1.Publish();
  TestWorklist worklist2;
  EXPECT_FALSE(worklist1.IsEmpty());
  EXPECT_TRUE(worklist2.IsEmpty());
  worklist1.Swap(&worklist2);
  EXPECT_TRUE(worklist1.IsEmpty());
  EXPECT_FALSE(worklist2.IsEmpty());
  TestWorklist::Local worklist_local2(&worklist2);
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_local2.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_FALSE(worklist_local2.Pop(&retrieved));
  EXPECT_TRUE(worklist2.IsEmpty());
}

}  // namespace base
}  // namespace heap
