// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marker.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class MarkerTest : public testing::TestWithHeap {
 public:
  using MarkingConfig = Marker::MarkingConfig;

  void DoMarking(MarkingConfig config) {
    Marker marker(Heap::From(GetHeap()));
    marker.StartMarking(config);
    marker.FinishMarking();
    marker.ProcessWeakness();
  }
};

class GCed : public GarbageCollected<GCed> {
 public:
  void SetChild(GCed* child) { child_ = child; }
  void SetWeakChild(GCed* child) { weak_child_ = child; }
  GCed* child() const { return child_.Get(); }
  GCed* weak_child() const { return weak_child_.Get(); }
  void Trace(cppgc::Visitor* visitor) const {
    visitor->Trace(child_);
    visitor->Trace(weak_child_);
  }

 private:
  Member<GCed> child_;
  WeakMember<GCed> weak_child_;
};

template <typename T>
V8_NOINLINE T access(volatile const T& t) {
  return t;
}

}  // namespace

TEST_F(MarkerTest, PersistentIsMarked) {
  Persistent<GCed> object = MakeGarbageCollected<GCed>(GetHeap());
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);
  EXPECT_FALSE(header.IsMarked());
  DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkerTest, ReachableMemberIsMarked) {
  Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
  parent->SetChild(MakeGarbageCollected<GCed>(GetHeap()));
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(parent->child());
  EXPECT_FALSE(header.IsMarked());
  DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkerTest, UnreachableMemberIsNotMarked) {
  Member<GCed> object = MakeGarbageCollected<GCed>(GetHeap());
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);
  EXPECT_FALSE(header.IsMarked());
  DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkerTest, ObjectReachableFromStackIsMarked) {
  GCed* object = MakeGarbageCollected<GCed>(GetHeap());
  EXPECT_FALSE(HeapObjectHeader::FromPayload(object).IsMarked());
  DoMarking(MarkingConfig(MarkingConfig::StackState::kMayContainHeapPointers));
  EXPECT_TRUE(HeapObjectHeader::FromPayload(object).IsMarked());
  access(object);
}

TEST_F(MarkerTest, ObjectReachableOnlyFromStackIsNotMarkedIfStackIsEmpty) {
  GCed* object = MakeGarbageCollected<GCed>(GetHeap());
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);
  EXPECT_FALSE(header.IsMarked());
  DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
  EXPECT_FALSE(header.IsMarked());
  access(object);
}

TEST_F(MarkerTest, WeakReferenceToUnreachableObjectIsCleared) {
  {
    WeakPersistent<GCed> weak_object = MakeGarbageCollected<GCed>(GetHeap());
    EXPECT_TRUE(weak_object);
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_FALSE(weak_object);
  }
  {
    Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
    parent->SetWeakChild(MakeGarbageCollected<GCed>(GetHeap()));
    EXPECT_TRUE(parent->weak_child());
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_FALSE(parent->weak_child());
  }
}

TEST_F(MarkerTest, WeakReferenceToReachableObjectIsNotCleared) {
  // Reachable from Persistent
  {
    Persistent<GCed> object = MakeGarbageCollected<GCed>(GetHeap());
    WeakPersistent<GCed> weak_object(object);
    EXPECT_TRUE(weak_object);
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_TRUE(weak_object);
  }
  {
    Persistent<GCed> object = MakeGarbageCollected<GCed>(GetHeap());
    Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
    parent->SetWeakChild(object);
    EXPECT_TRUE(parent->weak_child());
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_TRUE(parent->weak_child());
  }
  // Reachable from Member
  {
    Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
    WeakPersistent<GCed> weak_object(MakeGarbageCollected<GCed>(GetHeap()));
    parent->SetChild(weak_object);
    EXPECT_TRUE(weak_object);
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_TRUE(weak_object);
  }
  {
    Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
    parent->SetChild(MakeGarbageCollected<GCed>(GetHeap()));
    parent->SetWeakChild(parent->child());
    EXPECT_TRUE(parent->weak_child());
    DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
    EXPECT_TRUE(parent->weak_child());
  }
  // Reachable from stack
  {
    GCed* object = MakeGarbageCollected<GCed>(GetHeap());
    WeakPersistent<GCed> weak_object(object);
    EXPECT_TRUE(weak_object);
    DoMarking(
        MarkingConfig(MarkingConfig::StackState::kMayContainHeapPointers));
    EXPECT_TRUE(weak_object);
    access(object);
  }
  {
    GCed* object = MakeGarbageCollected<GCed>(GetHeap());
    Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetHeap());
    parent->SetWeakChild(object);
    EXPECT_TRUE(parent->weak_child());
    DoMarking(
        MarkingConfig(MarkingConfig::StackState::kMayContainHeapPointers));
    EXPECT_TRUE(parent->weak_child());
    access(object);
  }
}

TEST_F(MarkerTest, DeepHierarchyIsMarked) {
  static constexpr int kHierarchyDepth = 10;
  Persistent<GCed> root = MakeGarbageCollected<GCed>(GetHeap());
  GCed* parent = root;
  for (int i = 0; i < kHierarchyDepth; ++i) {
    parent->SetChild(MakeGarbageCollected<GCed>(GetHeap()));
    parent->SetWeakChild(parent->child());
    parent = parent->child();
  }
  DoMarking(MarkingConfig(MarkingConfig::StackState::kNoHeapPointers));
  EXPECT_TRUE(HeapObjectHeader::FromPayload(root).IsMarked());
  parent = root;
  for (int i = 0; i < kHierarchyDepth; ++i) {
    EXPECT_TRUE(HeapObjectHeader::FromPayload(parent->child()).IsMarked());
    EXPECT_TRUE(parent->weak_child());
    parent = parent->child();
  }
}

}  // namespace internal
}  // namespace cppgc
