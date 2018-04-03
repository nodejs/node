// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "src/base/platform/platform.h"
#include "src/globals.h"
#include "src/heap/gc-tracer.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

typedef TestWithContext GCTracerTest;

TEST(GCTracer, AverageSpeed) {
  base::RingBuffer<BytesAndDuration> buffer;
  EXPECT_EQ(100 / 2,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 0));
  buffer.Push(MakeBytesAndDuration(100, 8));
  EXPECT_EQ(100 / 2,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 2));
  EXPECT_EQ(200 / 10,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(100, 2), 3));
  const int max_speed = 1024 * MB;
  buffer.Reset();
  buffer.Push(MakeBytesAndDuration(max_speed, 0.5));
  EXPECT_EQ(max_speed,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 1));
  const int min_speed = 1;
  buffer.Reset();
  buffer.Push(MakeBytesAndDuration(1, 10000));
  EXPECT_EQ(min_speed,
            GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 1));
  buffer.Reset();
  int sum = 0;
  for (int i = 0; i < buffer.kSize; i++) {
    sum += i + 1;
    buffer.Push(MakeBytesAndDuration(i + 1, 1));
  }
  EXPECT_EQ(
      sum * 1.0 / buffer.kSize,
      GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), buffer.kSize));
  buffer.Push(MakeBytesAndDuration(100, 1));
  EXPECT_EQ(
      (sum * 1.0 - 1 + 100) / buffer.kSize,
      GCTracer::AverageSpeed(buffer, MakeBytesAndDuration(0, 0), buffer.kSize));
}

namespace {

void SampleAndAddAllocaton(v8::internal::GCTracer* tracer, double time_ms,
                           size_t new_space_counter_bytes,
                           size_t old_generation_counter_bytes) {
  tracer->SampleAllocation(time_ms, new_space_counter_bytes,
                           old_generation_counter_bytes);
  tracer->AddAllocation(time_ms);
}

}  // namespace

TEST_F(GCTracerTest, AllocationThroughput) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  int time1 = 100;
  size_t counter1 = 1000;
  // First sample creates baseline but is not part of the recorded samples.
  tracer->SampleAllocation(time1, counter1, counter1);
  SampleAndAddAllocaton(tracer, time1, counter1, counter1);
  int time2 = 200;
  size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2, counter2);
  // Will only consider the current sample.
  size_t throughput = static_cast<size_t>(
      tracer->AllocationThroughputInBytesPerMillisecond(100));
  EXPECT_EQ(2 * (counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3, counter3);
  // Considers last 2 samples.
  throughput = tracer->AllocationThroughputInBytesPerMillisecond(801);
  EXPECT_EQ(2 * (counter3 - counter1) / (time3 - time1), throughput);
}

TEST_F(GCTracerTest, NewSpaceAllocationThroughput) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  int time1 = 100;
  size_t counter1 = 1000;
  SampleAndAddAllocaton(tracer, time1, counter1, 0);
  int time2 = 200;
  size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2, 0);
  size_t throughput =
      tracer->NewSpaceAllocationThroughputInBytesPerMillisecond();
  EXPECT_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3, 0);
  throughput = tracer->NewSpaceAllocationThroughputInBytesPerMillisecond();
  EXPECT_EQ((counter3 - counter1) / (time3 - time1), throughput);
}

TEST_F(GCTracerTest, NewSpaceAllocationThroughputWithProvidedTime) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  int time1 = 100;
  size_t counter1 = 1000;
  // First sample creates baseline but is not part of the recorded samples.
  SampleAndAddAllocaton(tracer, time1, counter1, 0);
  int time2 = 200;
  size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2, 0);
  // Will only consider the current sample.
  size_t throughput =
      tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(100);
  EXPECT_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3, 0);
  // Considers last 2 samples.
  throughput = tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(801);
  EXPECT_EQ((counter3 - counter1) / (time3 - time1), throughput);
}

TEST_F(GCTracerTest, OldGenerationAllocationThroughputWithProvidedTime) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  int time1 = 100;
  size_t counter1 = 1000;
  // First sample creates baseline but is not part of the recorded samples.
  SampleAndAddAllocaton(tracer, time1, 0, counter1);
  int time2 = 200;
  size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, 0, counter2);
  // Will only consider the current sample.
  size_t throughput = static_cast<size_t>(
      tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(100));
  EXPECT_EQ((counter2 - counter1) / (time2 - time1), throughput);
  int time3 = 1000;
  size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, 0, counter3);
  // Considers last 2 samples.
  throughput = static_cast<size_t>(
      tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(801));
  EXPECT_EQ((counter3 - counter1) / (time3 - time1), throughput);
}

TEST_F(GCTracerTest, RegularScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  EXPECT_DOUBLE_EQ(0.0, tracer->current_.scopes[GCTracer::Scope::MC_MARK]);
  // Sample not added because it's not within a started tracer.
  tracer->AddScopeSample(GCTracer::Scope::MC_MARK, 100);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->AddScopeSample(GCTracer::Scope::MC_MARK, 100);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(100.0, tracer->current_.scopes[GCTracer::Scope::MC_MARK]);
}

TEST_F(GCTracerTest, IncrementalScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  EXPECT_DOUBLE_EQ(
      0.0, tracer->current_.scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]);
  // Sample is added because its ScopeId is listed as incremental sample.
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 100);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  // Switch to incremental MC to enable writing back incremental scopes.
  tracer->current_.type = GCTracer::Event::INCREMENTAL_MARK_COMPACTOR;
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 100);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(
      200.0, tracer->current_.scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]);
}

TEST_F(GCTracerTest, IncrementalMarkingDetails) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  // Round 1.
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 50);
  // Scavenger has no impact on incremental marking details.
  tracer->Start(SCAVENGER, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->Stop(SCAVENGER);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  // Switch to incremental MC to enable writing back incremental scopes.
  tracer->current_.type = GCTracer::Event::INCREMENTAL_MARK_COMPACTOR;
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 100);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(
      100,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .longest_step);
  EXPECT_EQ(
      2,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .steps);
  EXPECT_DOUBLE_EQ(
      150,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .duration);

  // Round 2. Numbers should be reset.
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 13);
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 15);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  // Switch to incremental MC to enable writing back incremental scopes.
  tracer->current_.type = GCTracer::Event::INCREMENTAL_MARK_COMPACTOR;
  tracer->AddScopeSample(GCTracer::Scope::MC_INCREMENTAL_FINALIZE, 122);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(
      122,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .longest_step);
  EXPECT_EQ(
      3,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .steps);
  EXPECT_DOUBLE_EQ(
      150,
      tracer->current_
          .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
          .duration);
}

TEST_F(GCTracerTest, IncrementalMarkingSpeed) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  // Round 1.
  // 1000000 bytes in 100ms.
  tracer->AddIncrementalMarkingStep(100, 1000000);
  EXPECT_EQ(1000000 / 100,
            tracer->IncrementalMarkingSpeedInBytesPerMillisecond());
  // 1000000 bytes in 100ms.
  tracer->AddIncrementalMarkingStep(100, 1000000);
  EXPECT_EQ(1000000 / 100,
            tracer->IncrementalMarkingSpeedInBytesPerMillisecond());
  // Scavenger has no impact on incremental marking details.
  tracer->Start(SCAVENGER, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->Stop(SCAVENGER);
  // 1000000 bytes in 100ms.
  tracer->AddIncrementalMarkingStep(100, 1000000);
  EXPECT_EQ(300, tracer->incremental_marking_duration_);
  EXPECT_EQ(3000000u, tracer->incremental_marking_bytes_);
  EXPECT_EQ(1000000 / 100,
            tracer->IncrementalMarkingSpeedInBytesPerMillisecond());
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  // Switch to incremental MC.
  tracer->current_.type = GCTracer::Event::INCREMENTAL_MARK_COMPACTOR;
  // 1000000 bytes in 100ms.
  tracer->AddIncrementalMarkingStep(100, 1000000);
  EXPECT_EQ(400, tracer->incremental_marking_duration_);
  EXPECT_EQ(4000000u, tracer->incremental_marking_bytes_);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_EQ(400, tracer->current_.incremental_marking_duration);
  EXPECT_EQ(4000000u, tracer->current_.incremental_marking_bytes);
  EXPECT_EQ(0, tracer->incremental_marking_duration_);
  EXPECT_EQ(0u, tracer->incremental_marking_bytes_);
  EXPECT_EQ(1000000 / 100,
            tracer->IncrementalMarkingSpeedInBytesPerMillisecond());

  // Round 2.
  tracer->AddIncrementalMarkingStep(2000, 1000);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  // Switch to incremental MC.
  tracer->current_.type = GCTracer::Event::INCREMENTAL_MARK_COMPACTOR;
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ((4000000.0 / 400 + 1000.0 / 2000) / 2,
                   static_cast<double>(
                       tracer->IncrementalMarkingSpeedInBytesPerMillisecond()));
}

TEST_F(GCTracerTest, BackgroundScavengerScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->Start(SCAVENGER, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL, 10,
      nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL, 1,
      nullptr);
  tracer->Stop(SCAVENGER);
  EXPECT_DOUBLE_EQ(
      11, tracer->current_
              .scopes[GCTracer::Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL]);
}

TEST_F(GCTracerTest, BackgroundMinorMCScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->Start(MINOR_MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_MARKING, 10, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_MARKING, 1, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_EVACUATE_COPY, 20,
      nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_EVACUATE_COPY, 2, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS,
      30, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS,
      3, nullptr);
  tracer->Stop(MINOR_MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(
      11,
      tracer->current_.scopes[GCTracer::Scope::MINOR_MC_BACKGROUND_MARKING]);
  EXPECT_DOUBLE_EQ(
      22, tracer->current_
              .scopes[GCTracer::Scope::MINOR_MC_BACKGROUND_EVACUATE_COPY]);
  EXPECT_DOUBLE_EQ(
      33, tracer->current_.scopes
              [GCTracer::Scope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS]);
}

TEST_F(GCTracerTest, BackgroundMajorMCScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_MARKING, 100, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_SWEEPING, 200, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_MARKING, 10, nullptr);
  // Scavenger should not affect the major mark-compact scopes.
  tracer->Start(SCAVENGER, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->Stop(SCAVENGER);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_SWEEPING, 20, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_MARKING, 1, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_SWEEPING, 2, nullptr);
  tracer->Start(MARK_COMPACTOR, GarbageCollectionReason::kTesting,
                "collector unittest");
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_COPY, 30, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_COPY, 3, nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS, 40,
      nullptr);
  tracer->AddBackgroundScopeSample(
      GCTracer::BackgroundScope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS, 4,
      nullptr);
  tracer->Stop(MARK_COMPACTOR);
  EXPECT_DOUBLE_EQ(
      111, tracer->current_.scopes[GCTracer::Scope::MC_BACKGROUND_MARKING]);
  EXPECT_DOUBLE_EQ(
      222, tracer->current_.scopes[GCTracer::Scope::MC_BACKGROUND_SWEEPING]);
  EXPECT_DOUBLE_EQ(
      33,
      tracer->current_.scopes[GCTracer::Scope::MC_BACKGROUND_EVACUATE_COPY]);
  EXPECT_DOUBLE_EQ(
      44, tracer->current_
              .scopes[GCTracer::Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS]);
}

class ThreadWithBackgroundScope final : public base::Thread {
 public:
  explicit ThreadWithBackgroundScope(GCTracer* tracer)
      : Thread(Options("ThreadWithBackgroundScope")), tracer_(tracer) {}
  void Run() override {
    GCTracer::BackgroundScope scope(
        tracer_, GCTracer::BackgroundScope::MC_BACKGROUND_MARKING);
  }

 private:
  GCTracer* tracer_;
};

TEST_F(GCTracerTest, MultithreadedBackgroundScope) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  ThreadWithBackgroundScope thread1(tracer);
  ThreadWithBackgroundScope thread2(tracer);
  tracer->ResetForTesting();
  thread1.Start();
  thread2.Start();
  tracer->FetchBackgroundMarkCompactCounters();
  thread1.Join();
  thread2.Join();
  tracer->FetchBackgroundMarkCompactCounters();
  EXPECT_LE(0, tracer->current_.scopes[GCTracer::Scope::MC_BACKGROUND_MARKING]);
}

}  // namespace internal
}  // namespace v8
