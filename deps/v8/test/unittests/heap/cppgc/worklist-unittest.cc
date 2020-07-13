// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/worklist.h"

#include "test/unittests/heap/cppgc/tests.h"

namespace cppgc {
namespace internal {

class SomeObject {};

using TestWorklist = Worklist<SomeObject*, 64>;

TEST(CppgcWorkListTest, SegmentCreate) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_EQ(0u, segment.Size());
  EXPECT_FALSE(segment.IsFull());
}

TEST(CppgcWorkListTest, SegmentPush) {
  TestWorklist::Segment segment;
  EXPECT_EQ(0u, segment.Size());
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
}

TEST(CppgcWorkListTest, SegmentPushPop) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
  SomeObject dummy;
  SomeObject* object = &dummy;
  EXPECT_TRUE(segment.Pop(&object));
  EXPECT_EQ(0u, segment.Size());
  EXPECT_EQ(nullptr, object);
}

TEST(CppgcWorkListTest, SegmentIsEmpty) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
}

TEST(CppgcWorkListTest, SegmentIsFull) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
}

TEST(CppgcWorkListTest, SegmentClear) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
  segment.Clear();
  EXPECT_TRUE(segment.IsEmpty());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
}

TEST(CppgcWorkListTest, SegmentFullPushFails) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
  EXPECT_FALSE(segment.Push(nullptr));
}

TEST(CppgcWorkListTest, SegmentEmptyPopFails) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  SomeObject* object;
  EXPECT_FALSE(segment.Pop(&object));
}

TEST(CppgcWorkListTest, SegmentUpdateFalse) {
  TestWorklist::Segment segment;
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  EXPECT_TRUE(segment.Push(object));
  segment.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(segment.IsEmpty());
}

TEST(CppgcWorkListTest, SegmentUpdate) {
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

TEST(CppgcWorkListTest, CreateEmpty) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  EXPECT_TRUE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist.IsEmpty());
}

TEST(CppgcWorkListTest, LocalPushPop) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_view.Push(&dummy));
  EXPECT_FALSE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist_view.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
}

TEST(CppgcWorkListTest, LocalIsBasedOnId) {
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

TEST(CppgcWorkListTest, LocalPushStaysPrivate) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Push(&dummy));
  EXPECT_FALSE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
  EXPECT_FALSE(worklist_view2.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(CppgcWorkListTest, GlobalUpdateNull) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  worklist.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(CppgcWorkListTest, GlobalUpdate) {
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

TEST(CppgcWorkListTest, FlushToGlobalPushSegment) {
  TestWorklist worklist;
  TestWorklist::View worklist_view0(&worklist, 0);
  TestWorklist::View worklist_view1(&worklist, 1);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  EXPECT_TRUE(worklist_view0.Push(objectA));
  worklist.FlushToGlobal(0);
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(CppgcWorkListTest, FlushToGlobalPopSegment) {
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
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(CppgcWorkListTest, Clear) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  worklist.Clear();
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(CppgcWorkListTest, SingleSegmentSteal) {
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
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  // Stealing.
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(CppgcWorkListTest, MultipleSegmentsStolen) {
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
  EXPECT_EQ(2U, worklist.GlobalPoolSize());
  // Stealing.
  EXPECT_TRUE(worklist_view2.Pop(&retrieved));
  SomeObject* const expect_bag2 = retrieved;
  EXPECT_TRUE(worklist_view3.Pop(&retrieved));
  SomeObject* const expect_bag3 = retrieved;
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
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
  EXPECT_TRUE(worklist.IsEmpty());
}

TEST(CppgcWorkListTest, MergeGlobalPool) {
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
  EXPECT_EQ(1U, worklist1.GlobalPoolSize());
  // Merging global pool into a new Worklist.
  TestWorklist worklist2;
  TestWorklist::View worklist_view2(&worklist2, 0);
  EXPECT_EQ(0U, worklist2.GlobalPoolSize());
  worklist2.MergeGlobalPool(&worklist1);
  EXPECT_EQ(1U, worklist2.GlobalPoolSize());
  EXPECT_FALSE(worklist2.IsEmpty());
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist1.IsEmpty());
  EXPECT_TRUE(worklist2.IsEmpty());
}

}  // namespace internal
}  // namespace cppgc
