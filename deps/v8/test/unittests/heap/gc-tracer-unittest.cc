// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

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

}  // namespace internal
}  // namespace v8
