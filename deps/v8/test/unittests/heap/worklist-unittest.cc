// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/worklist.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

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
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
}

TEST(WorkListTest, SegmentPushPop) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
  SomeObject dummy;
  SomeObject* object = &dummy;
  EXPECT_TRUE(segment.Pop(&object));
  EXPECT_EQ(0u, segment.Size());
  EXPECT_EQ(nullptr, object);
}

TEST(WorkListTest, SegmentIsEmpty) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
}

TEST(WorkListTest, SegmentIsFull) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
}

TEST(WorkListTest, SegmentClear) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
  segment.Clear();
  EXPECT_TRUE(segment.IsEmpty());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
}

TEST(WorkListTest, SegmentFullPushFails) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
  EXPECT_FALSE(segment.Push(nullptr));
}

TEST(WorkListTest, SegmentEmptyPopFails) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  SomeObject* object;
  EXPECT_FALSE(segment.Pop(&object));
}

TEST(WorkListTest, SegmentUpdateFalse) {
  TestWorklist::Segment segment;
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  EXPECT_TRUE(segment.Push(object));
  segment.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(segment.IsEmpty());
}

TEST(WorkListTest, SegmentUpdate) {
  TestWorklist::Segment segment;
  SomeObject* objectA;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  EXPECT_TRUE(segment.Push(objectA));
  segment.Update([objectB](SomeObject* object, SomeObject** out) {
    *out = objectB;
    return true;
  });
  SomeObject* object;
  EXPECT_TRUE(segment.Pop(&object));
  EXPECT_EQ(object, objectB);
}

TEST(WorkListTest, CreateEmpty) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  EXPECT_TRUE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, LocalPushPop) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_view.Push(&dummy));
  EXPECT_FALSE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist_view.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
}

TEST(WorkListTest, LocalIsBasedOnId) {
  TestWorklist worklist;
  // Use the same id.
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 0);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_view1.Push(&dummy));
  EXPECT_FALSE(worklist_view1.IsLocalEmpty());
  EXPECT_FALSE(worklist_view2.IsLocalEmpty());
  EXPECT_TRUE(worklist_view2.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_TRUE(worklist_view1.IsLocalEmpty());
  EXPECT_TRUE(worklist_view2.IsLocalEmpty());
}

TEST(WorkListTest, LocalPushStaysPrivate) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_TRUE(worklist_view1.Push(&dummy));
  EXPECT_FALSE(worklist.IsGlobalEmpty());
  EXPECT_FALSE(worklist_view2.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, GlobalUpdateNull) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  worklist.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, GlobalUpdate) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB = nullptr;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  SomeObject* objectC = nullptr;
  objectC = reinterpret_cast<SomeObject*>(&objectC);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(objectA));
  }
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(objectB));
  }
  EXPECT_TRUE(worklist_view.Push(objectA));
  worklist.Update([objectA, objectC](SomeObject* object, SomeObject** out) {
    if (object != objectA) {
      *out = objectC;
      return true;
    }
    return false;
  });
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    SomeObject* object;
    EXPECT_TRUE(worklist_view.Pop(&object));
    EXPECT_EQ(object, objectC);
  }
}

TEST(WorkListTest, FlushToGlobalPushSegment) {
  TestWorklist worklist;
  TestWorklist::View worklist_view0(&worklist, 0);
  TestWorklist::View worklist_view1(&worklist, 1);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  EXPECT_TRUE(worklist_view0.Push(objectA));
  worklist.FlushToGlobal(0);
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(WorkListTest, FlushToGlobalPopSegment) {
  TestWorklist worklist;
  TestWorklist::View worklist_view0(&worklist, 0);
  TestWorklist::View worklist_view1(&worklist, 1);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  EXPECT_TRUE(worklist_view0.Push(objectA));
  EXPECT_TRUE(worklist_view0.Push(objectA));
  EXPECT_TRUE(worklist_view0.Pop(&object));
  worklist.FlushToGlobal(0);
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(WorkListTest, Clear) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  worklist.Clear();
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, SingleSegmentSteal) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy));
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(nullptr));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  // Stealing.
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, MultipleSegmentsStolen) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  TestWorklist::View worklist_view3(&worklist, 2);
  SomeObject dummy1;
  SomeObject dummy2;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy1));
  }
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy2));
  }
  SomeObject* retrieved = nullptr;
  SomeObject dummy3;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(&dummy3));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(&dummy3, retrieved);
  // Stealing.
  EXPECT_TRUE(worklist_view2.Pop(&retrieved));
  SomeObject* const expect_bag2 = retrieved;
  EXPECT_TRUE(worklist_view3.Pop(&retrieved));
  SomeObject* const expect_bag3 = retrieved;
  EXPECT_NE(expect_bag2, expect_bag3);
  EXPECT_TRUE(expect_bag2 == &dummy1 || expect_bag2 == &dummy2);
  EXPECT_TRUE(expect_bag3 == &dummy1 || expect_bag3 == &dummy2);
  for (size_t i = 1; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(expect_bag2, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  for (size_t i = 1; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view3.Pop(&retrieved));
    EXPECT_EQ(expect_bag3, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorkListTest, MergeGlobalPool) {
  TestWorklist worklist1;
  TestWorklist::View worklist_view1(&worklist1, 0);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy));
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(nullptr));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  // Merging global pool into a new Worklist.
  TestWorklist worklist2;
  TestWorklist::View worklist_view2(&worklist2, 0);
  worklist2.MergeGlobalPool(&worklist1);
  EXPECT_FALSE(worklist2.IsGlobalEmpty());
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist1.IsGlobalEmpty());
  EXPECT_TRUE(worklist2.IsGlobalEmpty());
}

}  // namespace internal
}  // namespace v8
