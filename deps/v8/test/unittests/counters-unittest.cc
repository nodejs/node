// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/base/atomic-utils.h"
#include "src/base/platform/time.h"
#include "src/counters-inl.h"
#include "src/counters.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
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
  virtual ~AggregatedMemoryHistogramTest() {}

  void AddSample(double current_ms, double current_value) {
    aggregated_.AddSample(current_ms, current_value);
  }

  std::vector<int>* samples() { return mock_.samples(); }

 private:
  AggregatedMemoryHistogram<MockHistogram> aggregated_;
  MockHistogram mock_;
};

static base::TimeTicks runtime_call_stats_test_time_ = base::TimeTicks();
// Time source used for the RuntimeCallTimer during tests. We cannot rely on
// the native timer since it's too unpredictable on the build bots.
static base::TimeTicks RuntimeCallStatsTestNow() {
  return runtime_call_stats_test_time_;
}

class RuntimeCallStatsTest : public TestWithNativeContext {
 public:
  RuntimeCallStatsTest() {
    base::AsAtomic32::Relaxed_Store(
        &FLAG_runtime_stats,
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE);
    // We need to set {time_} to a non-zero value since it would otherwise
    // cause runtime call timers to think they are uninitialized.
    Sleep(1);
    stats()->Reset();
  }

  ~RuntimeCallStatsTest() {
    // Disable RuntimeCallStats before tearing down the isolate to prevent
    // printing the tests table. Comment the following line for debugging
    // purposes.
    base::AsAtomic32::Relaxed_Store(&FLAG_runtime_stats, 0);
  }

  static void SetUpTestCase() {
    TestWithIsolate::SetUpTestCase();
    // Use a custom time source to precisly emulate system time.
    RuntimeCallTimer::Now = &RuntimeCallStatsTestNow;
  }

  static void TearDownTestCase() {
    TestWithIsolate::TearDownTestCase();
    // Restore the original time source.
    RuntimeCallTimer::Now = &base::TimeTicks::HighResolutionNow;
  }

  RuntimeCallStats* stats() {
    return isolate()->counters()->runtime_call_stats();
  }

  // Print current RuntimeCallStats table. For debugging purposes.
  void PrintStats() { stats()->Print(); }

  RuntimeCallCounterId counter_id() {
    return RuntimeCallCounterId::kTestCounter1;
  }

  RuntimeCallCounterId counter_id2() {
    return RuntimeCallCounterId::kTestCounter2;
  }

  RuntimeCallCounterId counter_id3() {
    return RuntimeCallCounterId::kTestCounter3;
  }

  RuntimeCallCounter* js_counter() {
    return stats()->GetCounter(RuntimeCallCounterId::kJS_Execution);
  }
  RuntimeCallCounter* counter() { return stats()->GetCounter(counter_id()); }
  RuntimeCallCounter* counter2() { return stats()->GetCounter(counter_id2()); }
  RuntimeCallCounter* counter3() { return stats()->GetCounter(counter_id3()); }

  void Sleep(int64_t microseconds) {
    base::TimeDelta delta = base::TimeDelta::FromMicroseconds(microseconds);
    time_ += delta;
    runtime_call_stats_test_time_ =
        base::TimeTicks::FromInternalValue(time_.InMicroseconds());
  }

 private:
  base::TimeDelta time_;
};

// Temporarily use the native time to modify the test time.
class ElapsedTimeScope {
 public:
  explicit ElapsedTimeScope(RuntimeCallStatsTest* test) : test_(test) {
    timer_.Start();
  }
  ~ElapsedTimeScope() { test_->Sleep(timer_.Elapsed().InMicroseconds()); }

 private:
  base::ElapsedTimer timer_;
  RuntimeCallStatsTest* test_;
};

// Temporarily use the default time source.
class NativeTimeScope {
 public:
  NativeTimeScope() {
    CHECK_EQ(RuntimeCallTimer::Now, &RuntimeCallStatsTestNow);
    RuntimeCallTimer::Now = &base::TimeTicks::HighResolutionNow;
  }
  ~NativeTimeScope() {
    CHECK_EQ(RuntimeCallTimer::Now, &base::TimeTicks::HighResolutionNow);
    RuntimeCallTimer::Now = &RuntimeCallStatsTestNow;
  }
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

TEST_F(RuntimeCallStatsTest, RuntimeCallTimer) {
  RuntimeCallTimer timer;

  Sleep(50);
  stats()->Enter(&timer, counter_id());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(100);

  stats()->Leave(&timer);
  Sleep(50);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerSubTimer) {
  RuntimeCallTimer timer;
  RuntimeCallTimer timer2;

  stats()->Enter(&timer, counter_id());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(50);

  stats()->Enter(&timer2, counter_id2());
  // timer 1 is paused, while timer 2 is active.
  EXPECT_TRUE(timer2.IsStarted());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(counter2(), timer2.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, timer2.parent());
  EXPECT_EQ(&timer2, stats()->current_timer());

  Sleep(100);
  stats()->Leave(&timer2);

  // The subtimer subtracts its time from the parent timer.
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(0, counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter2()->time().InMicroseconds());
  EXPECT_EQ(&timer, stats()->current_timer());

  Sleep(100);

  stats()->Leave(&timer);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(150, counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter2()->time().InMicroseconds());
  EXPECT_EQ(nullptr, stats()->current_timer());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerRecursive) {
  RuntimeCallTimer timer;
  RuntimeCallTimer timer2;

  stats()->Enter(&timer, counter_id());
  EXPECT_EQ(counter(), timer.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(&timer, stats()->current_timer());

  stats()->Enter(&timer2, counter_id());
  EXPECT_EQ(counter(), timer2.counter());
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_EQ(&timer, timer2.parent());
  EXPECT_TRUE(timer2.IsStarted());
  EXPECT_EQ(&timer2, stats()->current_timer());

  Sleep(50);

  stats()->Leave(&timer2);
  EXPECT_EQ(nullptr, timer.parent());
  EXPECT_FALSE(timer2.IsStarted());
  EXPECT_TRUE(timer.IsStarted());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(50, counter()->time().InMicroseconds());

  Sleep(100);

  stats()->Leave(&timer);
  EXPECT_FALSE(timer.IsStarted());
  EXPECT_EQ(2, counter()->count());
  EXPECT_EQ(150, counter()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScope) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
  }
  Sleep(100);
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(50, counter()->time().InMicroseconds());
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
  }
  EXPECT_EQ(2, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScopeRecursive) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id());
      Sleep(50);
    }
    EXPECT_EQ(1, counter()->count());
    EXPECT_EQ(50, counter()->time().InMicroseconds());
  }
  EXPECT_EQ(2, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, RenameTimer) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter2()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    EXPECT_EQ(0, counter2()->time().InMicroseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id());
      Sleep(100);
    }
    CHANGE_CURRENT_RUNTIME_COUNTER(stats(),
                                   RuntimeCallCounterId::kTestCounter2);
    EXPECT_EQ(1, counter()->count());
    EXPECT_EQ(0, counter2()->count());
    EXPECT_EQ(100, counter()->time().InMicroseconds());
    EXPECT_EQ(0, counter2()->time().InMicroseconds());
  }
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(50, counter2()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, BasicPrintAndSnapshot) {
  std::ostringstream out;
  stats()->Print(out);
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(0, counter2()->count());
  EXPECT_EQ(0, counter3()->count());
  EXPECT_EQ(0, counter()->time().InMicroseconds());
  EXPECT_EQ(0, counter2()->time().InMicroseconds());
  EXPECT_EQ(0, counter3()->time().InMicroseconds());

  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(50);
    stats()->Print(out);
  }
  stats()->Print(out);
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(0, counter2()->count());
  EXPECT_EQ(0, counter3()->count());
  EXPECT_EQ(50, counter()->time().InMicroseconds());
  EXPECT_EQ(0, counter2()->time().InMicroseconds());
  EXPECT_EQ(0, counter3()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, PrintAndSnapshot) {
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(100);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    {
      RuntimeCallTimerScope scope(stats(), counter_id2());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_EQ(0, counter2()->time().InMicroseconds());
      Sleep(50);

      // This calls Snapshot on the current active timer and sychronizes and
      // commits the whole timer stack.
      std::ostringstream out;
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_EQ(100, counter()->time().InMicroseconds());
      EXPECT_EQ(50, counter2()->time().InMicroseconds());
      // Calling Print several times shouldn't have a (big) impact on the
      // measured times.
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_EQ(100, counter()->time().InMicroseconds());
      EXPECT_EQ(50, counter2()->time().InMicroseconds());

      Sleep(50);
      stats()->Print(out);
      EXPECT_EQ(0, counter()->count());
      EXPECT_EQ(0, counter2()->count());
      EXPECT_EQ(100, counter()->time().InMicroseconds());
      EXPECT_EQ(100, counter2()->time().InMicroseconds());
      Sleep(50);
    }
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(1, counter2()->count());
    EXPECT_EQ(100, counter()->time().InMicroseconds());
    EXPECT_EQ(150, counter2()->time().InMicroseconds());
    Sleep(50);
  }
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(200, counter()->time().InMicroseconds());
  EXPECT_EQ(150, counter2()->time().InMicroseconds());
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
  EXPECT_EQ(250, counter()->time().InMicroseconds());
  EXPECT_EQ(300, counter2()->time().InMicroseconds());
  EXPECT_EQ(100, counter3()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, BasicJavaScript) {
  RuntimeCallCounter* counter =
      stats()->GetCounter(RuntimeCallCounterId::kJS_Execution);
  EXPECT_EQ(0, counter->count());
  EXPECT_EQ(0, counter->time().InMicroseconds());

  {
    NativeTimeScope native_timer_scope;
    RunJS("function f() { return 1; }");
  }
  EXPECT_EQ(1, counter->count());
  int64_t time = counter->time().InMicroseconds();
  EXPECT_LT(0, time);

  {
    NativeTimeScope native_timer_scope;
    RunJS("f()");
  }
  EXPECT_EQ(2, counter->count());
  EXPECT_LE(time, counter->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, FunctionLengthGetter) {
  RuntimeCallCounter* getter_counter =
      stats()->GetCounter(RuntimeCallCounterId::kFunctionLengthGetter);
  RuntimeCallCounter* js_counter =
      stats()->GetCounter(RuntimeCallCounterId::kJS_Execution);
  EXPECT_EQ(0, getter_counter->count());
  EXPECT_EQ(0, js_counter->count());
  EXPECT_EQ(0, getter_counter->time().InMicroseconds());
  EXPECT_EQ(0, js_counter->time().InMicroseconds());

  {
    NativeTimeScope native_timer_scope;
    RunJS("function f(array) { return array.length; }");
  }
  EXPECT_EQ(0, getter_counter->count());
  EXPECT_EQ(1, js_counter->count());
  EXPECT_EQ(0, getter_counter->time().InMicroseconds());
  int64_t js_time = js_counter->time().InMicroseconds();
  EXPECT_LT(0, js_time);

  {
    NativeTimeScope native_timer_scope;
    RunJS("f.length");
  }
  EXPECT_EQ(1, getter_counter->count());
  EXPECT_EQ(2, js_counter->count());
  EXPECT_LE(0, getter_counter->time().InMicroseconds());
  EXPECT_LE(js_time, js_counter->time().InMicroseconds());

  {
    NativeTimeScope native_timer_scope;
    RunJS("for (let i = 0; i < 50; i++) { f.length }");
  }
  EXPECT_EQ(51, getter_counter->count());
  EXPECT_EQ(3, js_counter->count());
}

namespace {
static RuntimeCallStatsTest* current_test;
static const int kCustomCallbackTime = 1234;
static void CustomCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RuntimeCallTimerScope scope(current_test->stats(),
                              current_test->counter_id2());
  current_test->Sleep(kCustomCallbackTime);
}
}  // namespace

TEST_F(RuntimeCallStatsTest, CustomCallback) {
  current_test = this;
  // Set up a function template with a custom callback.
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);
  object_template->Set(isolate, "callback",
                       v8::FunctionTemplate::New(isolate, CustomCallback));
  v8::Local<v8::Object> object =
      object_template->NewInstance(v8_context()).ToLocalChecked();
  SetGlobalProperty("custom_object", object);

  // TODO(cbruni): Check api accessor timer (one above the custom callback).
  EXPECT_EQ(0, js_counter()->count());
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(0, counter2()->count());
  {
    RuntimeCallTimerScope scope(stats(), counter_id());
    Sleep(100);
    RunJS("custom_object.callback();");
  }
  EXPECT_EQ(1, js_counter()->count());
  // Given that no native timers are used, only the two scopes explitly
  // mentioned above will track the time.
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(1, counter2()->count());
  EXPECT_EQ(kCustomCallbackTime, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 9; i++) { custom_object.callback() };");
  EXPECT_EQ(2, js_counter()->count());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(10, counter2()->count());
  EXPECT_EQ(kCustomCallbackTime * 10, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 4000; i++) { custom_object.callback() };");
  EXPECT_EQ(3, js_counter()->count());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(4010, counter2()->count());
  EXPECT_EQ(kCustomCallbackTime * 4010, counter2()->time().InMicroseconds());
}

}  // namespace internal
}  // namespace v8
