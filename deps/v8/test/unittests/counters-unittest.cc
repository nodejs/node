// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/counters-inl.h"
#include "src/counters.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/tracing/tracing-category-observer.h"
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
  AggregatedMemoryHistogramTest() {
    aggregated_ = AggregatedMemoryHistogram<MockHistogram>(&mock_);
  }
  virtual ~AggregatedMemoryHistogramTest() {}

  void AddSample(double current_ms, double current_value) {
    aggregated_.AddSample(current_ms, current_value);
  }

  std::vector<int>* samples() { return mock_.samples(); }

 private:
  AggregatedMemoryHistogram<MockHistogram> aggregated_;
  MockHistogram mock_;
};

class RuntimeCallStatsTest : public ::testing::Test {
 public:
  RuntimeCallStatsTest() {
    FLAG_runtime_stats =
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE;
  }
  virtual ~RuntimeCallStatsTest() {}

  RuntimeCallStats* stats() { return &stats_; }
  RuntimeCallStats::CounterId counter_id() {
    return &RuntimeCallStats::TestCounter1;
  }
  RuntimeCallStats::CounterId counter_id2() {
    return &RuntimeCallStats::TestCounter2;
  }
  RuntimeCallStats::CounterId counter_id3() {
    return &RuntimeCallStats::TestCounter3;
  }
  RuntimeCallCounter* counter() { return &(stats()->*counter_id()); }
  RuntimeCallCounter* counter2() { return &(stats()->*counter_id2()); }
  RuntimeCallCounter* counter3() { return &(stats()->*counter_id3()); }
  void Sleep(int32_t milliseconds) {
    base::ElapsedTimer timer;
    base::TimeDelta delta = base::TimeDelta::FromMilliseconds(milliseconds);
    timer.Start();
    while (!timer.HasExpired(delta)) {
      base::OS::Sleep(base::TimeDelta::FromMicroseconds(0));
    }
  }

  const uint32_t kEpsilonMs = 20;

 private:
  RuntimeCallStats stats_;
};

}  // namespace


TEST_F(AggregatedMemoryHistogramTest, OneSample1) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
}


TEST_F(AggregatedMemoryHistogramTest, OneSample2) {
  FLAG_histogram_interval = 10;
  AddSample(10, 500);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}


TEST_F(AggregatedMemoryHistogramTest, OneSample3) {
  FLAG_histogram_interval = 10;
  AddSample(10, 500);
  AddSample(15, 500);
  AddSample(15, 1000);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}


TEST_F(AggregatedMemoryHistogramTest, OneSample4) {
  FLAG_histogram_interval = 10;
  AddSample(10, 500);
  AddSample(15, 750);
  AddSample(20, 1000);
  EXPECT_EQ(1U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples1) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples2) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples3) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(20, 1000);
  AddSample(20, 500);
  AddSample(30, 500);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(1000, (*samples())[0]);
  EXPECT_EQ(500, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples4) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(30, 0);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(750, (*samples())[0]);
  EXPECT_EQ(250, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples5) {
  FLAG_histogram_interval = 10;
  AddSample(10, 0);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ(250, (*samples())[0]);
  EXPECT_EQ(750, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples6) {
  FLAG_histogram_interval = 10;
  AddSample(10, 0);
  AddSample(15, 1000);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 1000) / 2, (*samples())[0]);
  EXPECT_EQ(1000, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples7) {
  FLAG_histogram_interval = 10;
  AddSample(10, 0);
  AddSample(15, 1000);
  AddSample(25, 0);
  AddSample(30, 1000);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 750) / 2, (*samples())[0]);
  EXPECT_EQ((250 + 500) / 2, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, TwoSamples8) {
  FLAG_histogram_interval = 10;
  AddSample(10, 1000);
  AddSample(15, 0);
  AddSample(25, 1000);
  AddSample(30, 0);
  EXPECT_EQ(2U, samples()->size());
  EXPECT_EQ((500 + 250) / 2, (*samples())[0]);
  EXPECT_EQ((750 + 500) / 2, (*samples())[1]);
}


TEST_F(AggregatedMemoryHistogramTest, ManySamples1) {
  FLAG_histogram_interval = 10;
  const int kMaxSamples = 1000;
  AddSample(0, 0);
  AddSample(10 * kMaxSamples, 10 * kMaxSamples);
  EXPECT_EQ(static_cast<unsigned>(kMaxSamples), samples()->size());
  for (int i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(i * 10 + 5, (*samples())[i]);
  }
}


TEST_F(AggregatedMemoryHistogramTest, ManySamples2) {
  FLAG_histogram_interval = 10;
  const int kMaxSamples = 1000;
  AddSample(0, 0);
  AddSample(10 * (2 * kMaxSamples), 10 * (2 * kMaxSamples));
  EXPECT_EQ(static_cast<unsigned>(kMaxSamples), samples()->size());
  for (int i = 0; i < kMaxSamples; i++) {
    EXPECT_EQ(i * 10 + 5, (*samples())[i]);
  }
}

#define EXPECT_IN_RANGE(start, value, end) \
  EXPECT_LE(start, value);                 \
  EXPECT_GE(end, value)

TEST_F(RuntimeCallStatsTest, RuntimeCallTimer) {
  RuntimeCallTimer timer;

  Sleep(50);
  RuntimeCallStats::Enter(stats(), &timer, counter_id());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(100);

  RuntimeCallStats::Leave(stats(), &timer);
  Sleep(50);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(), 100 + kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerSubTimer) {
  RuntimeCallTimer timer;
  RuntimeCallTimer timer2;

  RuntimeCallStats::Enter(stats(), &timer, counter_id());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(50);

  RuntimeCallStats::Enter(stats(), &timer2, counter_id2());
  // timer 1 is paused, while timer 2 is active.
  EXPECT_TRUE(timer2.IsStarted());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(counter2(), timer2.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, timer2.parent());
  EXPECT_EQ(&timer2, stats()->current_timer());

  Sleep(100);
  RuntimeCallStats::Leave(stats(), &timer2);

  // The subtimer subtracts its time from the parent timer.
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(0, counter()->time().InMilliseconds());
  EXPECT_IN_RANGE(100, counter2()->time().InMilliseconds(), 100 + kEpsilonMs);
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(100);

  RuntimeCallStats::Leave(stats(), &timer);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_IN_RANGE(150, counter()->time().InMilliseconds(), 150 + kEpsilonMs);
  EXPECT_IN_RANGE(100, counter2()->time().InMilliseconds(), 100 + kEpsilonMs);
  EXPECT_EQ(nullptr, stats()->current_timer());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerRecursive) {
  RuntimeCallTimer timer;
  RuntimeCallTimer timer2;

  RuntimeCallStats::Enter(stats(), &timer, counter_id());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(&timer, stats()->current_timer());

  RuntimeCallStats::Enter(stats(), &timer2, counter_id());
  EXPECT_EQ(counter(), timer2.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, timer2.parent());
  EXPECT_TRUE(timer2.IsStarted());
  EXPECT_EQ(&timer2, stats()->current_timer());

  Sleep(50);

  RuntimeCallStats::Leave(stats(), &timer2);
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_IN_RANGE(50, counter()->time().InMilliseconds(), 50 + kEpsilonMs);

  Sleep(100);

  RuntimeCallStats::Leave(stats(), &timer);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(2, counter()->count());
  EXPECT_IN_RANGE(150, counter()->time().InMilliseconds(),
                  150 + 2 * kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScope) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
  }
  Sleep(100);
  EXPECT_EQ(1, counter()->count());
  EXPECT_IN_RANGE(50, counter()->time().InMilliseconds(), 50 + kEpsilonMs);
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
  }
  EXPECT_EQ(2, counter()->count());
  EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(),
                  100 + 2 * kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScopeRecursive) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMilliseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id());
      Sleep(50);
    }
    EXPECT_EQ(1, counter()->count());
    EXPECT_IN_RANGE(50, counter()->time().InMilliseconds(), 50 + kEpsilonMs);
  }
  EXPECT_EQ(2, counter()->count());
  EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(),
                  100 + 2 * kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, RenameTimer) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter2()->count());
    EXPECT_EQ(0, counter()->time().InMilliseconds());
    EXPECT_EQ(0, counter2()->time().InMilliseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id());
      Sleep(100);
    }
    CHANGE_CURRENT_RUNTIME_COUNTER(stats(), TestCounter2);
    EXPECT_EQ(1, counter()->count());
    EXPECT_EQ(0, counter2()->count());
    EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(), 100 + kEpsilonMs);
    EXPECT_IN_RANGE(0, counter2()->time().InMilliseconds(), 0);
  }
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(), 100 + kEpsilonMs);
  EXPECT_IN_RANGE(50, counter2()->time().InMilliseconds(), 50 + kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, BasicPrintAndSnapshot) {
  std::ostringstream out;
  stats()->Print(out);
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(0, counter2()->count());
  EXPECT_EQ(0, counter3()->count());
  EXPECT_EQ(0, counter()->time().InMilliseconds());
  EXPECT_EQ(0, counter2()->time().InMilliseconds());
  EXPECT_EQ(0, counter3()->time().InMilliseconds());

  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    stats()->Print(out);
  }
  stats()->Print(out);
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(0, counter2()->count());
  EXPECT_EQ(0, counter3()->count());
  EXPECT_IN_RANGE(50, counter()->time().InMilliseconds(), 50 + kEpsilonMs);
  EXPECT_EQ(0, counter2()->time().InMilliseconds());
  EXPECT_EQ(0, counter3()->time().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, PrintAndSnapshot) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(100);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMilliseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id2());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_EQ(0, counter2()->time().InMilliseconds());
      Sleep(50);

      // This calls Snapshot on the current active timer and sychronizes and
      // commits the whole timer stack.
      std::ostringstream out;
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(),
                      100 + kEpsilonMs);
      EXPECT_IN_RANGE(50, counter2()->time().InMilliseconds(), 50 + kEpsilonMs);
      // Calling Print several times shouldn't have a (big) impact on the
      // measured times.
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(),
                      100 + kEpsilonMs);
      EXPECT_IN_RANGE(50, counter2()->time().InMilliseconds(), 50 + kEpsilonMs);

      Sleep(50);
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(),
                      100 + kEpsilonMs);
      EXPECT_IN_RANGE(100, counter2()->time().InMilliseconds(),
                      100 + kEpsilonMs);
      Sleep(50);
    }
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(1, counter2()->count());
    EXPECT_IN_RANGE(100, counter()->time().InMilliseconds(), 100 + kEpsilonMs);
    EXPECT_IN_RANGE(150, counter2()->time().InMilliseconds(), 150 + kEpsilonMs);
    Sleep(50);
  }
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_IN_RANGE(200, counter()->time().InMilliseconds(), 200 + kEpsilonMs);
  EXPECT_IN_RANGE(150, counter2()->time().InMilliseconds(),
                  150 + 2 * kEpsilonMs);
}

TEST_F(RuntimeCallStatsTest, NestedScopes) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(100);
    {
      RuntimeCallTimerScope scope(stats(), counter_id2());
      Sleep(100);
      {
        RuntimeCallTimerScope scope(stats(), counter_id3());
        Sleep(50);
      }
      Sleep(50);
      {
        RuntimeCallTimerScope scope(stats(), counter_id3());
        Sleep(50);
      }
      Sleep(50);
    }
    Sleep(100);
    {
      RuntimeCallTimerScope scope(stats(), counter_id2());
      Sleep(100);
    }
    Sleep(50);
  }
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(2, counter2()->count());
  EXPECT_EQ(2, counter3()->count());
  EXPECT_IN_RANGE(250, counter()->time().InMilliseconds(), 250 + kEpsilonMs);
  EXPECT_IN_RANGE(300, counter2()->time().InMilliseconds(), 300 + kEpsilonMs);
  EXPECT_IN_RANGE(100, counter3()->time().InMilliseconds(), 100 + kEpsilonMs);
}

}  // namespace internal
}  // namespace v8
