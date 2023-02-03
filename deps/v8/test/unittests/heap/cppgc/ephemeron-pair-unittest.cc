// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/ephemeron-pair.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {
class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(cppgc::Visitor*) const {}
};

class EphemeronHolder : public GarbageCollected<EphemeronHolder> {
 public:
  EphemeronHolder(GCed* key, GCed* value) : ephemeron_pair_(key, value) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(ephemeron_pair_); }

  const EphemeronPair<GCed, GCed>& ephemeron_pair() const {
    return ephemeron_pair_;
  }

 private:
  EphemeronPair<GCed, GCed> ephemeron_pair_;
};

class EphemeronHolderTraceEphemeron
    : public GarbageCollected<EphemeronHolderTraceEphemeron> {
 public:
  EphemeronHolderTraceEphemeron(GCed* key, GCed* value)
      : ephemeron_pair_(key, value) {}
  void Trace(cppgc::Visitor* visitor) const {
    visitor->TraceEphemeron(ephemeron_pair_.key, &ephemeron_pair_.value);
  }

 private:
  EphemeronPair<GCed, GCed> ephemeron_pair_;
};

class EphemeronPairTest : public testing::TestWithHeap {
  static constexpr MarkingConfig IncrementalPreciseMarkingConfig = {
      CollectionType::kMajor, StackState::kNoHeapPointers,
      MarkingConfig::MarkingType::kIncremental};

 public:
  void FinishSteps() {
    while (!SingleStep()) {
    }
  }

  void FinishMarking() {
    marker_->FinishMarking(StackState::kNoHeapPointers);
    // Pretend do finish sweeping as StatsCollector verifies that Notify*
    // methods are called in the right order.
    Heap::From(GetHeap())->stats_collector()->NotifySweepingCompleted(
        GCConfig::SweepingType::kIncremental);
  }

  void InitializeMarker(HeapBase& heap, cppgc::Platform* platform) {
    marker_ = std::make_unique<Marker>(heap, platform,
                                       IncrementalPreciseMarkingConfig);
    marker_->StartMarking();
  }

  Marker* marker() const { return marker_.get(); }

 private:
  bool SingleStep() {
    return marker_->IncrementalMarkingStepForTesting(
        StackState::kNoHeapPointers);
  }

  std::unique_ptr<Marker> marker_;
};

// static
constexpr MarkingConfig EphemeronPairTest::IncrementalPreciseMarkingConfig;

}  // namespace

TEST_F(EphemeronPairTest, ValueMarkedWhenKeyIsMarked) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  HeapObjectHeader::FromObject(key).TryMarkAtomic();
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromObject(value).IsMarked());
}

TEST_F(EphemeronPairTest, ValueNotMarkedWhenKeyIsNotMarked) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  EXPECT_FALSE(HeapObjectHeader::FromObject(key).IsMarked());
  EXPECT_FALSE(HeapObjectHeader::FromObject(value).IsMarked());
}

TEST_F(EphemeronPairTest, ValueNotMarkedBeforeKey) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishSteps();
  EXPECT_FALSE(HeapObjectHeader::FromObject(value).IsMarked());
  HeapObjectHeader::FromObject(key).TryMarkAtomic();
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromObject(value).IsMarked());
}

TEST_F(EphemeronPairTest, TraceEphemeronDispatch) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolderTraceEphemeron> holder =
      MakeGarbageCollected<EphemeronHolderTraceEphemeron>(GetAllocationHandle(),
                                                          key, value);
  HeapObjectHeader::FromObject(key).TryMarkAtomic();
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromObject(value).IsMarked());
}

TEST_F(EphemeronPairTest, EmptyValue) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolderTraceEphemeron> holder =
      MakeGarbageCollected<EphemeronHolderTraceEphemeron>(GetAllocationHandle(),
                                                          key, nullptr);
  HeapObjectHeader::FromObject(key).TryMarkAtomic();
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
}

TEST_F(EphemeronPairTest, EmptyKey) {
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolderTraceEphemeron> holder =
      MakeGarbageCollected<EphemeronHolderTraceEphemeron>(GetAllocationHandle(),
                                                          nullptr, value);
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  // Key is not alive and value should thus not be held alive.
  EXPECT_FALSE(HeapObjectHeader::FromObject(value).IsMarked());
}

using EphemeronPairGCTest = testing::TestWithHeap;

TEST_F(EphemeronPairGCTest, EphemeronPairValueIsCleared) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  // The precise GC will not find the `key` anywhere and thus clear the
  // ephemeron.
  PreciseGC();
  EXPECT_EQ(nullptr, holder->ephemeron_pair().value.Get());
}

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  void Trace(Visitor* v) const override {}
};

class OtherMixin : public GarbageCollectedMixin {
 public:
  void Trace(Visitor* v) const override {}
};

class GCedWithMixin : public GarbageCollected<GCedWithMixin>,
                      public OtherMixin,
                      public Mixin {
 public:
  void Trace(Visitor* v) const override {
    OtherMixin::Trace(v);
    Mixin::Trace(v);
  }
};

class EphemeronHolderWithMixins
    : public GarbageCollected<EphemeronHolderWithMixins> {
 public:
  EphemeronHolderWithMixins(Mixin* key, Mixin* value)
      : ephemeron_pair_(key, value) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(ephemeron_pair_); }

  const EphemeronPair<Mixin, Mixin>& ephemeron_pair() const {
    return ephemeron_pair_;
  }

 private:
  EphemeronPair<Mixin, Mixin> ephemeron_pair_;
};

}  // namespace

TEST_F(EphemeronPairTest, EphemeronPairWithMixinKey) {
  GCedWithMixin* key =
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
  GCedWithMixin* value =
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
  Persistent<EphemeronHolderWithMixins> holder =
      MakeGarbageCollected<EphemeronHolderWithMixins>(GetAllocationHandle(),
                                                      key, value);
  EXPECT_NE(static_cast<void*>(key), holder->ephemeron_pair().key.Get());
  EXPECT_NE(static_cast<void*>(value), holder->ephemeron_pair().value.Get());
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishSteps();
  EXPECT_FALSE(HeapObjectHeader::FromObject(value).IsMarked());
  EXPECT_TRUE(HeapObjectHeader::FromObject(key).TryMarkAtomic());
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromObject(value).IsMarked());
}

TEST_F(EphemeronPairTest, EphemeronPairWithEmptyMixinValue) {
  GCedWithMixin* key =
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
  Persistent<EphemeronHolderWithMixins> holder =
      MakeGarbageCollected<EphemeronHolderWithMixins>(GetAllocationHandle(),
                                                      key, nullptr);
  EXPECT_NE(static_cast<void*>(key), holder->ephemeron_pair().key.Get());
  EXPECT_TRUE(HeapObjectHeader::FromObject(key).TryMarkAtomic());
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishSteps();
  FinishMarking();
}

namespace {

class KeyWithCallback final : public GarbageCollected<KeyWithCallback> {
 public:
  template <typename Callback>
  explicit KeyWithCallback(Callback callback) {
    callback(this);
  }
  void Trace(Visitor*) const {}
};

class EphemeronHolderForKeyWithCallback final
    : public GarbageCollected<EphemeronHolderForKeyWithCallback> {
 public:
  EphemeronHolderForKeyWithCallback(KeyWithCallback* key, GCed* value)
      : ephemeron_pair_(key, value) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(ephemeron_pair_); }

 private:
  const EphemeronPair<KeyWithCallback, GCed> ephemeron_pair_;
};

}  // namespace

TEST_F(EphemeronPairTest, EphemeronPairWithKeyInConstruction) {
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolderForKeyWithCallback> holder;
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishSteps();
  MakeGarbageCollected<KeyWithCallback>(
      GetAllocationHandle(), [this, &holder, value](KeyWithCallback* thiz) {
        // The test doesn't use conservative stack scanning to retain key to
        // avoid retaining value as a side effect.
        EXPECT_TRUE(HeapObjectHeader::FromObject(thiz).TryMarkAtomic());
        holder = MakeGarbageCollected<EphemeronHolderForKeyWithCallback>(
            GetAllocationHandle(), thiz, value);
        // Finishing marking at this point will leave an ephemeron pair
        // reachable where the key is still in construction. The GC needs to
        // mark the value for such pairs as live in the atomic pause as they key
        // is considered live.
        FinishMarking();
      });
  EXPECT_TRUE(HeapObjectHeader::FromObject(value).IsMarked());
}

}  // namespace internal
}  // namespace cppgc
