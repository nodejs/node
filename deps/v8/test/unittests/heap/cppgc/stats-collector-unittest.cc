// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/stats-collector.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

constexpr size_t kNoMarkedBytes = 0;

constexpr size_t kMinReportedSize = StatsCollector::kAllocationThresholdBytes;

class StatsCollectorTest : public ::testing::Test {
 public:
  StatsCollectorTest()
      : stats(nullptr /* metric_recorder */, nullptr /* platform */) {}

  void FakeAllocate(size_t bytes) {
    stats.NotifyAllocation(bytes);
    stats.NotifySafePointForConservativeCollection();
  }

  void FakeFree(size_t bytes) {
    stats.NotifyExplicitFree(bytes);
    stats.NotifySafePointForConservativeCollection();
  }

  StatsCollector stats;
};

}  // namespace

TEST_F(StatsCollectorTest, NoMarkedBytes) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  stats.NotifyMarkingCompleted(kNoMarkedBytes);
  stats.NotifySweepingCompleted();
  auto event = stats.GetPreviousEventForTesting();
  EXPECT_EQ(0u, event.marked_bytes);
}

TEST_F(StatsCollectorTest, EventPrevGCMarkedObjectSize) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  stats.NotifyMarkingCompleted(1024);
  stats.NotifySweepingCompleted();
  auto event = stats.GetPreviousEventForTesting();
  EXPECT_EQ(1024u, event.marked_bytes);
}

TEST_F(StatsCollectorTest, AllocationNoReportBelowAllocationThresholdBytes) {
  constexpr size_t kObjectSize = 17;
  EXPECT_LT(kObjectSize, StatsCollector::kAllocationThresholdBytes);
  FakeAllocate(kObjectSize);
  EXPECT_EQ(0u, stats.allocated_object_size());
}

TEST_F(StatsCollectorTest, AlllocationReportAboveAllocationThresholdBytes) {
  constexpr size_t kObjectSize = StatsCollector::kAllocationThresholdBytes;
  EXPECT_GE(kObjectSize, StatsCollector::kAllocationThresholdBytes);
  FakeAllocate(kObjectSize);
  EXPECT_EQ(kObjectSize, stats.allocated_object_size());
}

TEST_F(StatsCollectorTest, InitialAllocatedObjectSize) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  EXPECT_EQ(0u, stats.allocated_object_size());
  stats.NotifyMarkingCompleted(kNoMarkedBytes);
  EXPECT_EQ(0u, stats.allocated_object_size());
  stats.NotifySweepingCompleted();
  EXPECT_EQ(0u, stats.allocated_object_size());
}

TEST_F(StatsCollectorTest, AllocatedObjectSize) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  FakeAllocate(kMinReportedSize);
  EXPECT_EQ(kMinReportedSize, stats.allocated_object_size());
  stats.NotifyMarkingCompleted(kMinReportedSize);
  EXPECT_EQ(kMinReportedSize, stats.allocated_object_size());
  stats.NotifySweepingCompleted();
  EXPECT_EQ(kMinReportedSize, stats.allocated_object_size());
}

TEST_F(StatsCollectorTest, AllocatedObjectSizeNoMarkedBytes) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  FakeAllocate(kMinReportedSize);
  EXPECT_EQ(kMinReportedSize, stats.allocated_object_size());
  stats.NotifyMarkingCompleted(kNoMarkedBytes);
  EXPECT_EQ(0u, stats.allocated_object_size());
  stats.NotifySweepingCompleted();
  EXPECT_EQ(0u, stats.allocated_object_size());
}

TEST_F(StatsCollectorTest, AllocatedObjectSizeAllocateAfterMarking) {
  stats.NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                             GarbageCollector::Config::IsForcedGC::kNotForced);
  FakeAllocate(kMinReportedSize);
  EXPECT_EQ(kMinReportedSize, stats.allocated_object_size());
  stats.NotifyMarkingCompleted(kMinReportedSize);
  FakeAllocate(kMinReportedSize);
  EXPECT_EQ(2 * kMinReportedSize, stats.allocated_object_size());
  stats.NotifySweepingCompleted();
  EXPECT_EQ(2 * kMinReportedSize, stats.allocated_object_size());
}

class MockAllocationObserver : public StatsCollector::AllocationObserver {
 public:
  MOCK_METHOD(void, AllocatedObjectSizeIncreased, (size_t), (override));
  MOCK_METHOD(void, AllocatedObjectSizeDecreased, (size_t), (override));
  MOCK_METHOD(void, ResetAllocatedObjectSize, (size_t), (override));
  MOCK_METHOD(void, AllocatedSizeIncreased, (size_t), (override));
  MOCK_METHOD(void, AllocatedSizeDecreased, (size_t), (override));
};

TEST_F(StatsCollectorTest, RegisterUnregisterObserver) {
  MockAllocationObserver observer;
  stats.RegisterObserver(&observer);
  stats.UnregisterObserver(&observer);
}

TEST_F(StatsCollectorTest, ObserveAllocatedObjectSizeIncreaseAndDecrease) {
  MockAllocationObserver observer;
  stats.RegisterObserver(&observer);
  EXPECT_CALL(observer, AllocatedObjectSizeIncreased(kMinReportedSize));
  FakeAllocate(kMinReportedSize);
  EXPECT_CALL(observer, AllocatedObjectSizeDecreased(kMinReportedSize));
  FakeFree(kMinReportedSize);
  stats.UnregisterObserver(&observer);
}

namespace {

void FakeGC(StatsCollector* stats, size_t marked_bytes) {
  stats->NotifyMarkingStarted(GarbageCollector::Config::CollectionType::kMajor,
                              GarbageCollector::Config::IsForcedGC::kNotForced);
  stats->NotifyMarkingCompleted(marked_bytes);
  stats->NotifySweepingCompleted();
}

}  // namespace

TEST_F(StatsCollectorTest, ObserveResetAllocatedObjectSize) {
  MockAllocationObserver observer;
  stats.RegisterObserver(&observer);
  EXPECT_CALL(observer, AllocatedObjectSizeIncreased(kMinReportedSize));
  FakeAllocate(kMinReportedSize);
  EXPECT_CALL(observer, ResetAllocatedObjectSize(64));
  FakeGC(&stats, 64);
  stats.UnregisterObserver(&observer);
}

TEST_F(StatsCollectorTest, ObserveAllocatedMemoryIncreaseAndDecrease) {
  MockAllocationObserver observer;
  stats.RegisterObserver(&observer);
  static constexpr size_t kAllocatedMemorySize = 4096;
  EXPECT_CALL(observer, AllocatedSizeIncreased(kAllocatedMemorySize));
  stats.NotifyAllocatedMemory(kAllocatedMemorySize);
  static constexpr size_t kFreedMemorySize = 2048;
  EXPECT_CALL(observer, AllocatedSizeDecreased(kFreedMemorySize));
  stats.NotifyFreedMemory(kFreedMemorySize);
  stats.UnregisterObserver(&observer);
}

namespace {

class AllocationObserverTriggeringGC final
    : public StatsCollector::AllocationObserver {
 public:
  AllocationObserverTriggeringGC(StatsCollector* stats, double survival_ratio)
      : stats(stats), survival_ratio_(survival_ratio) {}

  void AllocatedObjectSizeIncreased(size_t bytes) final {
    increase_call_count++;
    increased_size_bytes += bytes;
    if (increase_call_count == 1) {
      FakeGC(stats, bytes * survival_ratio_);
    }
  }

  // // Mock out the rest to trigger warnings if used.
  MOCK_METHOD(void, AllocatedObjectSizeDecreased, (size_t), (override));
  MOCK_METHOD(void, ResetAllocatedObjectSize, (size_t), (override));

  size_t increase_call_count = 0;
  size_t increased_size_bytes = 0;
  StatsCollector* stats;
  double survival_ratio_;
};

}  // namespace

TEST_F(StatsCollectorTest, ObserverTriggersGC) {
  constexpr double kSurvivalRatio = 0.5;
  AllocationObserverTriggeringGC gc_observer(&stats, kSurvivalRatio);
  MockAllocationObserver mock_observer;
  // Internal detail: First registered observer is also notified first.
  stats.RegisterObserver(&gc_observer);
  stats.RegisterObserver(&mock_observer);

  // Both observers see the exact allocated object size byte count.
  EXPECT_CALL(mock_observer,
              ResetAllocatedObjectSize(kMinReportedSize * kSurvivalRatio));
  EXPECT_CALL(gc_observer,
              ResetAllocatedObjectSize(kMinReportedSize * kSurvivalRatio));

  // Since the GC clears counters, mock_observer should see an increase call
  // with a delta of zero bytes. This expectation makes use of the internal
  // detail that first registered observer triggers GC.
  EXPECT_CALL(mock_observer, AllocatedObjectSizeIncreased(0));

  // Trigger scenario.
  FakeAllocate(kMinReportedSize);

  EXPECT_EQ(1u, gc_observer.increase_call_count);
  EXPECT_EQ(kMinReportedSize, gc_observer.increased_size_bytes);

  stats.UnregisterObserver(&gc_observer);
  stats.UnregisterObserver(&mock_observer);
}

}  // namespace internal
}  // namespace cppgc
