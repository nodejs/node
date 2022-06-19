// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-visitor.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/source-location.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-state.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class MarkingVisitorTest : public testing::TestWithHeap {
 public:
  MarkingVisitorTest()
      : marker_(std::make_unique<Marker>(*Heap::From(GetHeap()),
                                         GetPlatformHandle().get())) {
    marker_->StartMarking();
  }
  ~MarkingVisitorTest() override { marker_->ClearAllWorklistsForTesting(); }

  Marker* GetMarker() { return marker_.get(); }

 private:
  std::unique_ptr<Marker> marker_;
};

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(cppgc::Visitor*) const {}
};

class Mixin : public GarbageCollectedMixin {};
class GCedWithMixin : public GarbageCollected<GCedWithMixin>, public Mixin {
 public:
  void Trace(cppgc::Visitor*) const override {}
};

class TestMarkingVisitor : public MutatorMarkingVisitor {
 public:
  explicit TestMarkingVisitor(Marker* marker)
      : MutatorMarkingVisitor(marker->heap(),
                              marker->MutatorMarkingStateForTesting()) {}
  ~TestMarkingVisitor() { marking_state_.Publish(); }

  BasicMarkingState& marking_state() { return marking_state_; }
};

}  // namespace

TEST_F(MarkingVisitorTest, MarkedBytesAreInitiallyZero) {
  EXPECT_EQ(0u, GetMarker()->MutatorMarkingStateForTesting().marked_bytes());
}

// Strong references are marked.

TEST_F(MarkingVisitorTest, MarkMember) {
  Member<GCed> object(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(object);

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkMemberMixin) {
  GCedWithMixin* object(
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle()));
  Member<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(mixin);

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistent) {
  Persistent<GCed> object(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRootForTesting(object, SourceLocation::Current());

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistentMixin) {
  GCedWithMixin* object(
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle()));
  Persistent<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRootForTesting(mixin, SourceLocation::Current());

  EXPECT_TRUE(header.IsMarked());
}

// Weak references are not marked.

TEST_F(MarkingVisitorTest, DontMarkWeakMember) {
  WeakMember<GCed> object(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(object);

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberMixin) {
  GCedWithMixin* object(
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle()));
  WeakMember<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(mixin);

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistent) {
  WeakPersistent<GCed> object(
      MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRootForTesting(object, SourceLocation::Current());

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistentMixin) {
  GCedWithMixin* object(
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle()));
  WeakPersistent<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRootForTesting(mixin, SourceLocation::Current());

  EXPECT_FALSE(header.IsMarked());
}

// In construction objects are not marked.

namespace {

class GCedWithInConstructionCallback
    : public GarbageCollected<GCedWithInConstructionCallback> {
 public:
  template <typename Callback>
  explicit GCedWithInConstructionCallback(Callback callback) {
    callback(this);
  }
  void Trace(cppgc::Visitor*) const {}
};

class MixinWithInConstructionCallback : public GarbageCollectedMixin {
 public:
  template <typename Callback>
  explicit MixinWithInConstructionCallback(Callback callback) {
    callback(this);
  }
};
class GCedWithMixinWithInConstructionCallback
    : public GarbageCollected<GCedWithMixinWithInConstructionCallback>,
      public MixinWithInConstructionCallback {
 public:
  template <typename Callback>
  explicit GCedWithMixinWithInConstructionCallback(Callback callback)
      : MixinWithInConstructionCallback(callback) {}
  void Trace(cppgc::Visitor*) const override {}
};

}  // namespace

TEST_F(MarkingVisitorTest, MarkMemberInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](GCedWithInConstructionCallback* obj) {
            Member<GCedWithInConstructionCallback> object(obj);
            visitor.Trace(object);
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_TRUE(visitor.marking_state().not_fully_constructed_worklist().Contains(
      &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkMemberMixinInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](MixinWithInConstructionCallback* obj) {
            Member<MixinWithInConstructionCallback> mixin(obj);
            visitor.Trace(mixin);
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_TRUE(visitor.marking_state().not_fully_constructed_worklist().Contains(
      &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](GCedWithInConstructionCallback* obj) {
            WeakMember<GCedWithInConstructionCallback> object(obj);
            visitor.Trace(object);
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_FALSE(
      visitor.marking_state().not_fully_constructed_worklist().Contains(
          &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberMixinInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](MixinWithInConstructionCallback* obj) {
            WeakMember<MixinWithInConstructionCallback> mixin(obj);
            visitor.Trace(mixin);
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_FALSE(
      visitor.marking_state().not_fully_constructed_worklist().Contains(
          &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistentInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](GCedWithInConstructionCallback* obj) {
            Persistent<GCedWithInConstructionCallback> object(obj);
            visitor.TraceRootForTesting(object, SourceLocation::Current());
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_TRUE(visitor.marking_state().not_fully_constructed_worklist().Contains(
      &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistentMixinInConstruction) {
  TestMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetAllocationHandle(),
          [&visitor](MixinWithInConstructionCallback* obj) {
            Persistent<MixinWithInConstructionCallback> mixin(obj);
            visitor.TraceRootForTesting(mixin, SourceLocation::Current());
          });
  HeapObjectHeader& header = HeapObjectHeader::FromObject(gced);
  EXPECT_TRUE(visitor.marking_state().not_fully_constructed_worklist().Contains(
      &header));
  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, StrongTracingMarksWeakMember) {
  WeakMember<GCed> object(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader& header = HeapObjectHeader::FromObject(object);

  TestMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceStrongly(object);

  EXPECT_TRUE(header.IsMarked());
}

}  // namespace internal
}  // namespace cppgc
