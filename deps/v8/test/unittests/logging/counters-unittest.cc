// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/api/api-inl.h"
#include "src/base/atomic-utils.h"
#include "src/base/platform/time.h"
#include "src/handles/handles-inl.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/tracing/tracing-category-observer.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

class MockHistogram : public Histogram {
 public:
  void AddSample(int value) { samples_.push_back(value); }
  std::vector<int>* samples() { return &samples_; }

 private:
  std::vector<int> samples_;
};

class AggregatedMemoryHistogramTest : public ::testing::Test {
 public:
  AggregatedMemoryHistogramTest() : aggregated_(&mock_) {}
  ~AggregatedMemoryHistogramTest() override = default;

  void AddSample(double current_ms, double current_value) {
    aggregated_.AddSample(current_ms, current_value);
  }

  std::vector<int>* samples() { return mock_.samples(); }

 private:
  AggregatedMemoryHistogram<MockHistogram> aggregated_;
  MockHistogram mock_;
};

class SnapshotNativeCounterTest : public TestWithNativeContextAndCounters {
 public:
  SnapshotNativeCounterTest() {}

  bool SupportsNativeCounters() const {
#ifdef V8_SNAPSHOT_NATIVE_CODE_COUNTERS
    return true;
#else
    return false;
#endif  // V8_SNAPSHOT_NATIVE_CODE_COUNTERS
  }

#define SC(name, caption)                                        \
  int name() {                                                   \
    CHECK(isolate()->counters()->name()->Enabled());             \
    return *isolate()->counters()->name()->GetInternalPointer(); \
  }
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC

  void PrintAll() {
#define SC(name, caption) PrintF(#caption " = %d\n", name());
    STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC
  }
};

}  // namespace

TEST_F(AggregatedMemoryHistogramTest, OneSample1) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
}

TEST_F(AggregatedMemoryHistogramTest, OneSample2) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 500);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}

TEST_F(AggregatedMemoryHistogramTest, OneSample3) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 500);
  AddSample(15, 500);
  AddSample(15, 1000);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}

TEST_F(AggregatedMemoryHistogramTest, OneSample4) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 500);
  AddSample(15, 750);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples1) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples2) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples3) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  AddSample(20, 500);
  AddSample(30, 500);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(500, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples4) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(30, 0);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
  EXPECT_EQ(250, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples5) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 0);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(250, (*samples())[0]);
  EXPECT_EQ(750, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples6) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 0);
  AddSample(15, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 1000) / 2, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples7) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 0);
  AddSample(15, 1000);
  AddSample(25, 0);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 750) / 2, (*samples())[0]);
  EXPECT_EQ((250 + 500) / 2, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, TwoSamples8) {
  v8_flags.histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(15, 0);
  AddSample(25, 1000);
  AddSample(30, 0);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 250) / 2, (*samples())[0]);
  EXPECT_EQ((750 + 500) / 2, (*samples())[1]);
}

TEST_F(AggregatedMemoryHistogramTest, ManySamples1) {
  v8_flags.histogram_interval = 10;
  const int kMaxSamples = 1000;
  AddSample(0, 0);
  AddSample(10 * kMaxSamples, 10 * kMaxSamples);
  EXPECT_EQ(static_cast<unsigned>(kMaxSamples), samples()->size());
  for (int i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(i * 10 + 5, (*samples())[i]);
  }
}

TEST_F(AggregatedMemoryHistogramTest, ManySamples2) {
  v8_flags.histogram_interval = 10;
  const int kMaxSamples = 1000;
  AddSample(0, 0);
  AddSample(10 * (2 * kMaxSamples), 10 * (2 * kMaxSamples));
  EXPECT_EQ(static_cast<unsigned>(kMaxSamples), samples()->size());
  for (int i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(i * 10 + 5, (*samples())[i]);
  }
}

TEST_F(SnapshotNativeCounterTest, WriteBarrier) {
  RunJS("let o = {a: 42};");

  if (!v8_flags.single_generation && SupportsNativeCounters()) {
    EXPECT_NE(0, write_barriers());
  } else {
    EXPECT_EQ(0, write_barriers());
  }

  PrintAll();
}

}  // namespace internal
}  // namespace v8
