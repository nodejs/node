// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/gc-tracer.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using GCTracerTest = TestWithContext;

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
                           size_t per_space_counter_bytes) {
  // Increment counters of all spaces.
  tracer->SampleAllocation(time_ms, per_space_counter_bytes,
                           per_space_counter_bytes, per_space_counter_bytes);
  tracer->AddAllocation(time_ms);
}

}  // namespace

TEST_F(GCTracerTest, AllocationThroughput) {
  // GCTracer::AllocationThroughputInBytesPerMillisecond ignores global memory.
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  const int time1 = 100;
  const size_t counter1 = 1000;
  SampleAndAddAllocaton(tracer, time1, counter1);
  const int time2 = 200;
  const size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2);
  // Will only consider the current sample.
  EXPECT_EQ(2 * (counter2 - counter1) / (time2 - time1),
            static_cast<size_t>(
                tracer->AllocationThroughputInBytesPerMillisecond(100)));
  const int time3 = 1000;
  const size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3);
  // Only consider last sample.
  EXPECT_EQ(2 * (counter3 - counter2) / (time3 - time2),
            static_cast<size_t>(
                tracer->AllocationThroughputInBytesPerMillisecond(800)));
  // Considers last 2 samples.
  EXPECT_EQ(2 * (counter3 - counter1) / (time3 - time1),
            static_cast<size_t>(
                tracer->AllocationThroughputInBytesPerMillisecond(801)));
}

TEST_F(GCTracerTest, PerGenerationAllocationThroughput) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  const int time1 = 100;
  const size_t counter1 = 1000;
  SampleAndAddAllocaton(tracer, time1, counter1);
  const int time2 = 200;
  const size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2);
  const size_t expected_throughput1 = (counter2 - counter1) / (time2 - time1);
  EXPECT_EQ(expected_throughput1,
            static_cast<size_t>(
                tracer->NewSpaceAllocationThroughputInBytesPerMillisecond()));
  EXPECT_EQ(
      expected_throughput1,
      static_cast<size_t>(
          tracer->OldGenerationAllocationThroughputInBytesPerMillisecond()));
  EXPECT_EQ(expected_throughput1,
            static_cast<size_t>(
                tracer->EmbedderAllocationThroughputInBytesPerMillisecond()));
  const int time3 = 1000;
  const size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3);
  const size_t expected_throughput2 = (counter3 - counter1) / (time3 - time1);
  EXPECT_EQ(expected_throughput2,
            static_cast<size_t>(
                tracer->NewSpaceAllocationThroughputInBytesPerMillisecond()));
  EXPECT_EQ(
      expected_throughput2,
      static_cast<size_t>(
          tracer->OldGenerationAllocationThroughputInBytesPerMillisecond()));
  EXPECT_EQ(expected_throughput2,
            static_cast<size_t>(
                tracer->EmbedderAllocationThroughputInBytesPerMillisecond()));
}

TEST_F(GCTracerTest, PerGenerationAllocationThroughputWithProvidedTime) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  const int time1 = 100;
  const size_t counter1 = 1000;
  SampleAndAddAllocaton(tracer, time1, counter1);
  const int time2 = 200;
  const size_t counter2 = 2000;
  SampleAndAddAllocaton(tracer, time2, counter2);
  const size_t expected_throughput1 = (counter2 - counter1) / (time2 - time1);
  EXPECT_EQ(
      expected_throughput1,
      static_cast<size_t>(
          tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(100)));
  EXPECT_EQ(
      expected_throughput1,
      static_cast<size_t>(
          tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(100)));
  const int time3 = 1000;
  const size_t counter3 = 30000;
  SampleAndAddAllocaton(tracer, time3, counter3);
  const size_t expected_throughput2 = (counter3 - counter2) / (time3 - time2);
  // Only consider last sample.
  EXPECT_EQ(
      expected_throughput2,
      static_cast<size_t>(
          tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(800)));
  EXPECT_EQ(
      expected_throughput2,
      static_cast<size_t>(
          tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(800)));
  const size_t expected_throughput3 = (counter3 - counter1) / (time3 - time1);
  // Consider last two samples.
  EXPECT_EQ(
      expected_throughput3,
      static_cast<size_t>(
          tracer->NewSpaceAllocationThroughputInBytesPerMillisecond(801)));
  EXPECT_EQ(
      expected_throughput3,
      static_cast<size_t>(
          tracer->OldGenerationAllocationThroughputInBytesPerMillisecond(801)));
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

TEST_F(GCTracerTest, MutatorUtilization) {
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();

  // Mark-compact #1 ended at 200ms and took 100ms.
  tracer->RecordMutatorUtilization(200, 100);
  // Avarage mark-compact time = 0ms.
  // Avarage mutator time = 0ms.
  EXPECT_DOUBLE_EQ(1.0, tracer->CurrentMarkCompactMutatorUtilization());
  EXPECT_DOUBLE_EQ(1.0, tracer->AverageMarkCompactMutatorUtilization());

  // Mark-compact #2 ended at 400ms and took 100ms.
  tracer->RecordMutatorUtilization(400, 100);
  // The first mark-compactor is ignored.
  // Avarage mark-compact time = 100ms.
  // Avarage mutator time = 100ms.
  EXPECT_DOUBLE_EQ(0.5, tracer->CurrentMarkCompactMutatorUtilization());
  EXPECT_DOUBLE_EQ(0.5, tracer->AverageMarkCompactMutatorUtilization());

  // Mark-compact #3 ended at 600ms and took 200ms.
  tracer->RecordMutatorUtilization(600, 200);
  // Avarage mark-compact time = 100ms * 0.5 + 200ms * 0.5.
  // Avarage mutator time = 100ms * 0.5 + 0ms * 0.5.
  EXPECT_DOUBLE_EQ(0.0, tracer->CurrentMarkCompactMutatorUtilization());
  EXPECT_DOUBLE_EQ(50.0 / 200.0,
                   tracer->AverageMarkCompactMutatorUtilization());

  // Mark-compact #4 ended at 800ms and took 0ms.
  tracer->RecordMutatorUtilization(800, 0);
  // Avarage mark-compact time = 150ms * 0.5 + 0ms * 0.5.
  // Avarage mutator time = 50ms * 0.5 + 200ms * 0.5.
  EXPECT_DOUBLE_EQ(1.0, tracer->CurrentMarkCompactMutatorUtilization());
  EXPECT_DOUBLE_EQ(125.0 / 200.0,
                   tracer->AverageMarkCompactMutatorUtilization());
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

class GcHistogram {
 public:
  static void* CreateHistogram(const char* name, int min, int max,
                               size_t buckets) {
    histograms_[name] = std::unique_ptr<GcHistogram>(new GcHistogram());
    return histograms_[name].get();
  }

  static void AddHistogramSample(void* histogram, int sample) {
    if (histograms_.empty()) return;
    static_cast<GcHistogram*>(histogram)->samples_.push_back(sample);
  }

  static GcHistogram* Get(const char* name) { return histograms_[name].get(); }

  static void CleanUp() { histograms_.clear(); }

  int Total() const {
    int result = 0;
    for (int i : samples_) {
      result += i;
    }
    return result;
  }

  int Count() const { return static_cast<int>(samples_.size()); }

 private:
  std::vector<int> samples_;
  static std::map<std::string, std::unique_ptr<GcHistogram>> histograms_;
};

std::map<std::string, std::unique_ptr<GcHistogram>> GcHistogram::histograms_ =
    std::map<std::string, std::unique_ptr<GcHistogram>>();

TEST_F(GCTracerTest, RecordMarkCompactHistograms) {
  if (FLAG_stress_incremental_marking) return;
  isolate()->SetCreateHistogramFunction(&GcHistogram::CreateHistogram);
  isolate()->SetAddHistogramSampleFunction(&GcHistogram::AddHistogramSample);
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->current_.scopes[GCTracer::Scope::MC_CLEAR] = 1;
  tracer->current_.scopes[GCTracer::Scope::MC_EPILOGUE] = 2;
  tracer->current_.scopes[GCTracer::Scope::MC_EVACUATE] = 3;
  tracer->current_.scopes[GCTracer::Scope::MC_FINISH] = 4;
  tracer->current_.scopes[GCTracer::Scope::MC_MARK] = 5;
  tracer->current_.scopes[GCTracer::Scope::MC_PROLOGUE] = 6;
  tracer->current_.scopes[GCTracer::Scope::MC_SWEEP] = 7;
  tracer->RecordGCPhasesHistograms(i_isolate()->counters()->gc_finalize());
  EXPECT_EQ(1, GcHistogram::Get("V8.GCFinalizeMC.Clear")->Total());
  EXPECT_EQ(2, GcHistogram::Get("V8.GCFinalizeMC.Epilogue")->Total());
  EXPECT_EQ(3, GcHistogram::Get("V8.GCFinalizeMC.Evacuate")->Total());
  EXPECT_EQ(4, GcHistogram::Get("V8.GCFinalizeMC.Finish")->Total());
  EXPECT_EQ(5, GcHistogram::Get("V8.GCFinalizeMC.Mark")->Total());
  EXPECT_EQ(6, GcHistogram::Get("V8.GCFinalizeMC.Prologue")->Total());
  EXPECT_EQ(7, GcHistogram::Get("V8.GCFinalizeMC.Sweep")->Total());
  GcHistogram::CleanUp();
}

TEST_F(GCTracerTest, RecordScavengerHistograms) {
  if (FLAG_stress_incremental_marking) return;
  isolate()->SetCreateHistogramFunction(&GcHistogram::CreateHistogram);
  isolate()->SetAddHistogramSampleFunction(&GcHistogram::AddHistogramSample);
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->current_.scopes[GCTracer::Scope::SCAVENGER_SCAVENGE_ROOTS] = 1;
  tracer->current_.scopes[GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL] = 2;
  tracer->RecordGCPhasesHistograms(i_isolate()->counters()->gc_scavenger());
  EXPECT_EQ(1, GcHistogram::Get("V8.GCScavenger.ScavengeRoots")->Total());
  EXPECT_EQ(2, GcHistogram::Get("V8.GCScavenger.ScavengeMain")->Total());
  GcHistogram::CleanUp();
}

TEST_F(GCTracerTest, RecordGCSumHistograms) {
  if (FLAG_stress_incremental_marking) return;
  isolate()->SetCreateHistogramFunction(&GcHistogram::CreateHistogram);
  isolate()->SetAddHistogramSampleFunction(&GcHistogram::AddHistogramSample);
  GCTracer* tracer = i_isolate()->heap()->tracer();
  tracer->ResetForTesting();
  tracer->current_
      .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_START]
      .duration = 1;
  tracer->current_
      .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_SWEEPING]
      .duration = 2;
  tracer->AddIncrementalMarkingStep(3.0, 1024);
  tracer->current_
      .incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL_FINALIZE]
      .duration = 4;
  const double atomic_pause_duration = 5.0;
  tracer->RecordGCSumCounters(atomic_pause_duration);
  EXPECT_EQ(15, GcHistogram::Get("V8.GCMarkCompactor")->Total());
  GcHistogram::CleanUp();
}

}  // namespace internal
}  // namespace v8
