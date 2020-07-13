// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-visitor.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/source-location.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/marker.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class MarkingVisitorTest : public testing::TestWithHeap {
 public:
  MarkingVisitorTest()
      : marker_(std::make_unique<Marker>(Heap::From(GetHeap()))) {}
  ~MarkingVisitorTest() { marker_->ClearAllWorklistsForTesting(); }

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
  USING_GARBAGE_COLLECTED_MIXIN();

 public:
  void Trace(cppgc::Visitor*) const override {}
};

}  // namespace

// Strong refernces are marked.

TEST_F(MarkingVisitorTest, MarkMember) {
  Member<GCed> object(MakeGarbageCollected<GCed>(GetHeap()));
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(object);

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkMemberMixin) {
  GCedWithMixin* object(MakeGarbageCollected<GCedWithMixin>(GetHeap()));
  Member<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(mixin);

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistent) {
  Persistent<GCed> object(MakeGarbageCollected<GCed>(GetHeap()));
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRoot(object, SourceLocation::Current());

  EXPECT_TRUE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, MarkPersistentMixin) {
  GCedWithMixin* object(MakeGarbageCollected<GCedWithMixin>(GetHeap()));
  Persistent<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRoot(mixin, SourceLocation::Current());

  EXPECT_TRUE(header.IsMarked());
}

// Weak references are not marked.

TEST_F(MarkingVisitorTest, DontMarkWeakMember) {
  WeakMember<GCed> object(MakeGarbageCollected<GCed>(GetHeap()));
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(object);

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberMixin) {
  GCedWithMixin* object(MakeGarbageCollected<GCedWithMixin>(GetHeap()));
  WeakMember<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.Trace(mixin);

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistent) {
  WeakPersistent<GCed> object(MakeGarbageCollected<GCed>(GetHeap()));
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRoot(object, SourceLocation::Current());

  EXPECT_FALSE(header.IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistentMixin) {
  GCedWithMixin* object(MakeGarbageCollected<GCedWithMixin>(GetHeap()));
  WeakPersistent<Mixin> mixin(object);
  HeapObjectHeader& header = HeapObjectHeader::FromPayload(object);

  MutatorThreadMarkingVisitor visitor(GetMarker());

  EXPECT_FALSE(header.IsMarked());

  visitor.TraceRoot(mixin, SourceLocation::Current());

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
  USING_GARBAGE_COLLECTED_MIXIN();

 public:
  template <typename Callback>
  explicit GCedWithMixinWithInConstructionCallback(Callback callback)
      : MixinWithInConstructionCallback(callback) {}
  void Trace(cppgc::Visitor*) const override {}
};

}  // namespace

TEST_F(MarkingVisitorTest, DontMarkMemberInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetHeap(), [&visitor](GCedWithInConstructionCallback* obj) {
            Member<GCedWithInConstructionCallback> object(obj);
            visitor.Trace(object);
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkMemberMixinInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetHeap(), [&visitor](MixinWithInConstructionCallback* obj) {
            Member<MixinWithInConstructionCallback> mixin(obj);
            visitor.Trace(mixin);
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetHeap(), [&visitor](GCedWithInConstructionCallback* obj) {
            WeakMember<GCedWithInConstructionCallback> object(obj);
            visitor.Trace(object);
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakMemberMixinInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetHeap(), [&visitor](MixinWithInConstructionCallback* obj) {
            WeakMember<MixinWithInConstructionCallback> mixin(obj);
            visitor.Trace(mixin);
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkPersistentInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetHeap(), [&visitor](GCedWithInConstructionCallback* obj) {
            Persistent<GCedWithInConstructionCallback> object(obj);
            visitor.TraceRoot(object, SourceLocation::Current());
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkPersistentMixinInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetHeap(), [&visitor](MixinWithInConstructionCallback* obj) {
            Persistent<MixinWithInConstructionCallback> mixin(obj);
            visitor.TraceRoot(mixin, SourceLocation::Current());
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistentInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithInConstructionCallback>(
          GetHeap(), [&visitor](GCedWithInConstructionCallback* obj) {
            WeakPersistent<GCedWithInConstructionCallback> object(obj);
            visitor.TraceRoot(object, SourceLocation::Current());
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

TEST_F(MarkingVisitorTest, DontMarkWeakPersistentMixinInConstruction) {
  MutatorThreadMarkingVisitor visitor(GetMarker());
  GCedWithMixinWithInConstructionCallback* gced =
      MakeGarbageCollected<GCedWithMixinWithInConstructionCallback>(
          GetHeap(), [&visitor](MixinWithInConstructionCallback* obj) {
            WeakPersistent<MixinWithInConstructionCallback> mixin(obj);
            visitor.TraceRoot(mixin, SourceLocation::Current());
          });
  EXPECT_FALSE(HeapObjectHeader::FromPayload(gced).IsMarked());
}

}  // namespace internal
}  // namespace cppgc
