// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/ephemeron-pair.h"

#include "include/cppgc/allocation.h"
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

class EphemeronHolder : public GarbageCollected<GCed> {
 public:
  EphemeronHolder(GCed* key, GCed* value) : ephemeron_pair_(key, value) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(ephemeron_pair_); }

 private:
  EphemeronPair<GCed, GCed> ephemeron_pair_;
};

class EhpemeronPairTest : public testing::TestWithHeap {
  using MarkingConfig = Marker::MarkingConfig;

  static constexpr Marker::MarkingConfig IncrementalPreciseMarkingConfig = {
      MarkingConfig::CollectionType::kMajor,
      MarkingConfig::StackState::kNoHeapPointers,
      MarkingConfig::MarkingType::kIncremental};

 public:
  void FinishSteps() {
    while (!SingleStep()) {
    }
  }

  void FinishMarking() {
    marker_->FinishMarking(MarkingConfig::StackState::kNoHeapPointers);
    // Pretend do finish sweeping as StatsCollector verifies that Notify*
    // methods are called in the right order.
    Heap::From(GetHeap())->stats_collector()->NotifySweepingCompleted();
  }

  void InitializeMarker(HeapBase& heap, cppgc::Platform* platform) {
    marker_ = MarkerFactory::CreateAndStartMarking<Marker>(
        heap, platform, IncrementalPreciseMarkingConfig);
  }

  Marker* marker() const { return marker_.get(); }

 private:
  bool SingleStep() {
    return marker_->IncrementalMarkingStepForTesting(
        MarkingConfig::StackState::kNoHeapPointers);
  }

  std::unique_ptr<Marker> marker_;
};

// static
constexpr Marker::MarkingConfig
    EhpemeronPairTest::IncrementalPreciseMarkingConfig;

}  // namespace

TEST_F(EhpemeronPairTest, ValueMarkedWhenKeyIsMarked) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  HeapObjectHeader::FromPayload(key).TryMarkAtomic();
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(value).IsMarked());
}

TEST_F(EhpemeronPairTest, ValueNotMarkedWhenKeyIsNotMarked) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishMarking();
  EXPECT_FALSE(HeapObjectHeader::FromPayload(key).IsMarked());
  EXPECT_FALSE(HeapObjectHeader::FromPayload(value).IsMarked());
}

TEST_F(EhpemeronPairTest, ValueNotMarkedBeforeKey) {
  GCed* key = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* value = MakeGarbageCollected<GCed>(GetAllocationHandle());
  Persistent<EphemeronHolder> holder =
      MakeGarbageCollected<EphemeronHolder>(GetAllocationHandle(), key, value);
  InitializeMarker(*Heap::From(GetHeap()), GetPlatformHandle().get());
  FinishSteps();
  EXPECT_FALSE(HeapObjectHeader::FromPayload(value).IsMarked());
  HeapObjectHeader::FromPayload(key).TryMarkAtomic();
  FinishMarking();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(value).IsMarked());
}

}  // namespace internal
}  // namespace cppgc
