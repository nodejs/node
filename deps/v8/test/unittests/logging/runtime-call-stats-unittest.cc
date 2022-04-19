// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/runtime-call-stats.h"

#include <atomic>

#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/atomic-utils.h"
#include "src/base/platform/time.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/tracing/tracing-category-observer.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

static std::atomic<base::TimeTicks> runtime_call_stats_test_time_ =
    base::TimeTicks();
// Time source used for the RuntimeCallTimer during tests. We cannot rely on
// the native timer since it's too unpredictable on the build bots.
static base::TimeTicks RuntimeCallStatsTestNow() {
  return runtime_call_stats_test_time_;
}

class RuntimeCallStatsTest : public TestWithNativeContext {
 public:
  RuntimeCallStatsTest() {
    TracingFlags::runtime_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE,
        std::memory_order_relaxed);
    // We need to set {time_} to a non-zero value since it would otherwise
    // cause runtime call timers to think they are uninitialized.
    Sleep(1);
    stats()->Reset();
  }

  ~RuntimeCallStatsTest() override {
    // Disable RuntimeCallStats before tearing down the isolate to prevent
    // printing the tests table. Comment the following line for debugging
    // purposes.
    TracingFlags::runtime_stats.store(0, std::memory_order_relaxed);
  }

  static void SetUpTestSuite() {
    TestWithIsolate::SetUpTestSuite();
    // Use a custom time source to precisly emulate system time.
    RuntimeCallTimer::Now = &RuntimeCallStatsTestNow;
  }

  static void TearDownTestSuite() {
    TestWithIsolate::TearDownTestSuite();
    // Restore the original time source.
    RuntimeCallTimer::Now = &base::TimeTicks::Now;
  }

  RuntimeCallStats* stats() {
    return isolate()->counters()->runtime_call_stats();
  }

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
class V8_NODISCARD ElapsedTimeScope {
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
class V8_NODISCARD NativeTimeScope {
 public:
  NativeTimeScope() {
    CHECK_EQ(RuntimeCallTimer::Now, &RuntimeCallStatsTestNow);
    RuntimeCallTimer::Now = &base::TimeTicks::Now;
  }
  ~NativeTimeScope() {
    CHECK_EQ(RuntimeCallTimer::Now, &base::TimeTicks::Now);
    RuntimeCallTimer::Now = &RuntimeCallStatsTestNow;
  }
};

}  // namespace

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
    RCS_SCOPE(stats(), counter_id());
    Sleep(50);
  }
  Sleep(100);
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(50, counter()->time().InMicroseconds());
  {
    RCS_SCOPE(stats(), counter_id());
    Sleep(50);
  }
  EXPECT_EQ(2, counter()->count());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScopeRecursive) {
  {
    RCS_SCOPE(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    {
      RCS_SCOPE(stats(), counter_id());
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
    RCS_SCOPE(stats(), counter_id());
    Sleep(50);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter2()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    EXPECT_EQ(0, counter2()->time().InMicroseconds());
    {
      RCS_SCOPE(stats(), counter_id());
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
    RCS_SCOPE(stats(), counter_id());
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
    RCS_SCOPE(stats(), counter_id());
    Sleep(100);
    EXPECT_EQ(0, counter()->count());
    EXPECT_EQ(0, counter()->time().InMicroseconds());
    {
      RCS_SCOPE(stats(), counter_id2());
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
    RCS_SCOPE(stats(), counter_id());
    Sleep(100);
    {
      RCS_SCOPE(stats(), counter_id2());
      Sleep(100);
      {
        RCS_SCOPE(stats(), counter_id3());
        Sleep(50);
      }
      Sleep(50);
      {
        RCS_SCOPE(stats(), counter_id3());
        Sleep(50);
      }
      Sleep(50);
    }
    Sleep(100);
    {
      RCS_SCOPE(stats(), counter_id2());
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
    RunJS("function f() { return 1; };");
  }
  EXPECT_EQ(1, counter->count());
  int64_t time = counter->time().InMicroseconds();
  EXPECT_LT(0, time);

  {
    NativeTimeScope native_timer_scope;
    RunJS("f();");
  }
  EXPECT_EQ(2, counter->count());
  EXPECT_LE(time, counter->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, FunctionLengthGetter) {
  RuntimeCallCounter* getter_counter =
      stats()->GetCounter(RuntimeCallCounterId::kFunctionLengthGetter);
  EXPECT_EQ(0, getter_counter->count());
  EXPECT_EQ(0, js_counter()->count());
  EXPECT_EQ(0, getter_counter->time().InMicroseconds());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());

  {
    NativeTimeScope native_timer_scope;
    RunJS("function f(array) { return array.length; };");
  }
  EXPECT_EQ(0, getter_counter->count());
  EXPECT_EQ(1, js_counter()->count());
  EXPECT_EQ(0, getter_counter->time().InMicroseconds());
  int64_t js_time = js_counter()->time().InMicroseconds();
  EXPECT_LT(0, js_time);

  {
    NativeTimeScope native_timer_scope;
    RunJS("f.length;");
  }
  EXPECT_EQ(1, getter_counter->count());
  EXPECT_EQ(2, js_counter()->count());
  EXPECT_LE(0, getter_counter->time().InMicroseconds());
  EXPECT_LE(js_time, js_counter()->time().InMicroseconds());

  {
    NativeTimeScope native_timer_scope;
    RunJS("for (let i = 0; i < 50; i++) { f.length };");
  }
  EXPECT_EQ(51, getter_counter->count());
  EXPECT_EQ(3, js_counter()->count());

  {
    NativeTimeScope native_timer_scope;
    RunJS("for (let i = 0; i < 1000; i++) { f.length; };");
  }
  EXPECT_EQ(1051, getter_counter->count());
  EXPECT_EQ(4, js_counter()->count());
}

namespace {
static RuntimeCallStatsTest* current_test;
static const int kCustomCallbackTime = 1234;
static void CustomCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RCS_SCOPE(current_test->stats(), current_test->counter_id2());
  current_test->Sleep(kCustomCallbackTime);
}
}  // namespace

TEST_F(RuntimeCallStatsTest, CallbackFunction) {
  FLAG_allow_natives_syntax = true;
  FLAG_incremental_marking = false;

  RuntimeCallCounter* callback_counter =
      stats()->GetCounter(RuntimeCallCounterId::kFunctionCallback);

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

  EXPECT_EQ(0, js_counter()->count());
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(0, callback_counter->count());
  EXPECT_EQ(0, counter2()->count());
  {
    RCS_SCOPE(stats(), counter_id());
    Sleep(100);
    RunJS("custom_object.callback();");
  }
  EXPECT_EQ(1, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, callback_counter->count());
  EXPECT_EQ(1, counter2()->count());
  // Given that no native timers are used, only the two scopes explitly
  // mentioned above will track the time.
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 9; i++) { custom_object.callback(); };");
  EXPECT_EQ(2, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(10, callback_counter->count());
  EXPECT_EQ(10, counter2()->count());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 10, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 4000; i++) { custom_object.callback(); };");
  EXPECT_EQ(3, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(4010, callback_counter->count());
  EXPECT_EQ(4010, counter2()->count());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 4010, counter2()->time().InMicroseconds());

  // Check that the FunctionCallback tracing also works properly
  // when the `callback` is called from optimized code.
  RunJS(
      "function wrap(o) { return o.callback(); };\n"
      "%PrepareFunctionForOptimization(wrap);\n"
      "wrap(custom_object);\n"
      "wrap(custom_object);\n"
      "%OptimizeFunctionOnNextCall(wrap);\n"
      "wrap(custom_object);\n");
  EXPECT_EQ(4, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(4013, callback_counter->count());
  EXPECT_EQ(4013, counter2()->count());
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 4013, counter2()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, ApiGetter) {
  FLAG_allow_natives_syntax = true;
  FLAG_incremental_marking = false;

  RuntimeCallCounter* callback_counter =
      stats()->GetCounter(RuntimeCallCounterId::kFunctionCallback);
  current_test = this;
  // Set up a function template with an api accessor.
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);
  object_template->SetAccessorProperty(
      NewString("apiGetter"),
      v8::FunctionTemplate::New(isolate, CustomCallback));
  v8::Local<v8::Object> object =
      object_template->NewInstance(v8_context()).ToLocalChecked();
  SetGlobalProperty("custom_object", object);

  // TODO(cbruni): Check api accessor timer (one above the custom callback).
  EXPECT_EQ(0, js_counter()->count());
  EXPECT_EQ(0, counter()->count());
  EXPECT_EQ(0, callback_counter->count());
  EXPECT_EQ(0, counter2()->count());

  {
    RCS_SCOPE(stats(), counter_id());
    Sleep(100);
    RunJS("custom_object.apiGetter;");
  }

  EXPECT_EQ(1, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(1, callback_counter->count());
  EXPECT_EQ(1, counter2()->count());
  // Given that no native timers are used, only the two scopes explitly
  // mentioned above will track the time.
  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 9; i++) { custom_object.apiGetter };");

  EXPECT_EQ(2, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(10, callback_counter->count());
  EXPECT_EQ(10, counter2()->count());

  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 10, counter2()->time().InMicroseconds());

  RunJS("for (let i = 0; i < 4000; i++) { custom_object.apiGetter };");

  EXPECT_EQ(3, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(4010, callback_counter->count());
  EXPECT_EQ(4010, counter2()->count());

  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 4010, counter2()->time().InMicroseconds());

  // Check that the FunctionCallback tracing also works properly
  // when the `apiGetter` is called from optimized code.
  RunJS(
      "function wrap(o) { return o.apiGetter; };\n"
      "%PrepareFunctionForOptimization(wrap);\n"
      "wrap(custom_object);\n"
      "wrap(custom_object);\n"
      "%OptimizeFunctionOnNextCall(wrap);\n"
      "wrap(custom_object);\n");

  EXPECT_EQ(4, js_counter()->count());
  EXPECT_EQ(1, counter()->count());
  EXPECT_EQ(4013, callback_counter->count());
  EXPECT_EQ(4013, counter2()->count());

  EXPECT_EQ(0, js_counter()->time().InMicroseconds());
  EXPECT_EQ(100, counter()->time().InMicroseconds());
  EXPECT_EQ(0, callback_counter->time().InMicroseconds());
  EXPECT_EQ(kCustomCallbackTime * 4013, counter2()->time().InMicroseconds());
}

TEST_F(RuntimeCallStatsTest, GarbageCollection) {
  if (FLAG_stress_incremental_marking) return;
  FLAG_expose_gc = true;
  // Disable concurrent GC threads because otherwise they may continue
  // running after this test completes and race with is_runtime_stats_enabled()
  // updates.
  FLAG_single_threaded_gc = true;

  FlagList::EnforceFlagImplications();
  v8::Isolate* isolate = v8_isolate();
  RunJS(
      "let root = [];"
      "for (let i = 0; i < 10; i++) root.push((new Array(1000)).fill(0));"
      "root.push((new Array(1000000)).fill(0));"
      "((new Array(1000000)).fill(0));");
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace internal
}  // namespace v8
