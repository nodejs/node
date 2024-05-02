// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "include/v8-function.h"
#include "include/v8-platform.h"
#include "src/init/v8.h"
#include "src/tracing/trace-event.h"
#include "test/cctest/cctest.h"

namespace {

struct MockTraceObject {
  char phase;
  std::string name;
  uint64_t id;
  uint64_t bind_id;
  int num_args;
  unsigned int flags;
  int64_t timestamp;
  MockTraceObject(char phase, std::string name, uint64_t id, uint64_t bind_id,
                  int num_args, int flags, int64_t timestamp)
      : phase(phase),
        name(name),
        id(id),
        bind_id(bind_id),
        num_args(num_args),
        flags(flags),
        timestamp(timestamp) {}
};

class MockTracingController : public v8::TracingController {
 public:
  MockTracingController() = default;
  MockTracingController(const MockTracingController&) = delete;
  MockTracingController& operator=(const MockTracingController&) = delete;

  uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override {
    return AddTraceEventWithTimestamp(
        phase, category_enabled_flag, name, scope, id, bind_id, num_args,
        arg_names, arg_types, arg_values, arg_convertables, flags, 0);
  }

  uint64_t AddTraceEventWithTimestamp(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags, int64_t timestamp) override {
    std::unique_ptr<MockTraceObject> to = std::make_unique<MockTraceObject>(
        phase, std::string(name), id, bind_id, num_args, flags, timestamp);
    trace_objects_.push_back(std::move(to));
    return 0;
  }

  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name, uint64_t handle) override {}

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    if (strncmp(name, "v8-cat", 6)) {
      static uint8_t no = 0;
      return &no;
    } else {
      static uint8_t yes = 0x7;
      return &yes;
    }
  }

  const std::vector<std::unique_ptr<MockTraceObject>>& GetMockTraceObjects()
      const {
    return trace_objects_;
  }

 private:
  std::vector<std::unique_ptr<MockTraceObject>> trace_objects_;
};

class MockTracingPlatform : public TestPlatform {
 public:
  v8::TracingController* GetTracingController() override {
    return &tracing_controller_;
  }

  size_t NumberOfTraceObjects() {
    return tracing_controller_.GetMockTraceObjects().size();
  }

  MockTraceObject* GetTraceObject(size_t index) {
    return tracing_controller_.GetMockTraceObjects().at(index).get();
  }

 private:
  MockTracingController tracing_controller_;
};

}  // namespace

TEST_WITH_PLATFORM(TraceEventDisabledCategory, MockTracingPlatform) {
  // Disabled category, will not add events.
  TRACE_EVENT_BEGIN0("cat", "e1");
  TRACE_EVENT_END0("cat", "e1");
  CHECK_EQ(0, platform.NumberOfTraceObjects());
}

TEST_WITH_PLATFORM(TraceEventNoArgs, MockTracingPlatform) {
  // Enabled category will add 2 events.
  TRACE_EVENT_BEGIN0("v8-cat", "e1");
  TRACE_EVENT_END0("v8-cat", "e1");

  CHECK_EQ(2, platform.NumberOfTraceObjects());
  CHECK_EQ('B', platform.GetTraceObject(0)->phase);
  CHECK_EQ("e1", platform.GetTraceObject(0)->name);
  CHECK_EQ(0, platform.GetTraceObject(0)->num_args);

  CHECK_EQ('E', platform.GetTraceObject(1)->phase);
  CHECK_EQ("e1", platform.GetTraceObject(1)->name);
  CHECK_EQ(0, platform.GetTraceObject(1)->num_args);
}

TEST_WITH_PLATFORM(TraceEventWithOneArg, MockTracingPlatform) {
  TRACE_EVENT_BEGIN1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_END1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_BEGIN1("v8-cat", "e2", "arg1", "abc");
  TRACE_EVENT_END1("v8-cat", "e2", "arg1", "abc");

  CHECK_EQ(4, platform.NumberOfTraceObjects());

  CHECK_EQ(1, platform.GetTraceObject(0)->num_args);
  CHECK_EQ(1, platform.GetTraceObject(1)->num_args);
  CHECK_EQ(1, platform.GetTraceObject(2)->num_args);
  CHECK_EQ(1, platform.GetTraceObject(3)->num_args);
}

TEST_WITH_PLATFORM(TraceEventWithTwoArgs, MockTracingPlatform) {
  TRACE_EVENT_BEGIN2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_END2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_BEGIN2("v8-cat", "e2", "arg1", "abc", "arg2", 43);
  TRACE_EVENT_END2("v8-cat", "e2", "arg1", "abc", "arg2", 43);

  CHECK_EQ(4, platform.NumberOfTraceObjects());

  CHECK_EQ(2, platform.GetTraceObject(0)->num_args);
  CHECK_EQ(2, platform.GetTraceObject(1)->num_args);
  CHECK_EQ(2, platform.GetTraceObject(2)->num_args);
  CHECK_EQ(2, platform.GetTraceObject(3)->num_args);
}

TEST_WITH_PLATFORM(ScopedTraceEvent, MockTracingPlatform) {
  { TRACE_EVENT0("v8-cat", "e"); }

  CHECK_EQ(1, platform.NumberOfTraceObjects());
  CHECK_EQ(0, platform.GetTraceObject(0)->num_args);

  { TRACE_EVENT1("v8-cat", "e1", "arg1", "abc"); }

  CHECK_EQ(2, platform.NumberOfTraceObjects());
  CHECK_EQ(1, platform.GetTraceObject(1)->num_args);

  { TRACE_EVENT2("v8-cat", "e1", "arg1", "abc", "arg2", 42); }

  CHECK_EQ(3, platform.NumberOfTraceObjects());
  CHECK_EQ(2, platform.GetTraceObject(2)->num_args);
}

TEST_WITH_PLATFORM(TestEventWithFlow, MockTracingPlatform) {
  static uint64_t bind_id = 21;
  {
    TRACE_EVENT_WITH_FLOW0("v8-cat", "f1", bind_id, TRACE_EVENT_FLAG_FLOW_OUT);
  }
  {
    TRACE_EVENT_WITH_FLOW0(
        "v8-cat", "f2", bind_id,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  }
  { TRACE_EVENT_WITH_FLOW0("v8-cat", "f3", bind_id, TRACE_EVENT_FLAG_FLOW_IN); }

  CHECK_EQ(3, platform.NumberOfTraceObjects());
  CHECK_EQ(bind_id, platform.GetTraceObject(0)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_OUT, platform.GetTraceObject(0)->flags);
  CHECK_EQ(bind_id, platform.GetTraceObject(1)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
           platform.GetTraceObject(1)->flags);
  CHECK_EQ(bind_id, platform.GetTraceObject(2)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN, platform.GetTraceObject(2)->flags);
}

TEST_WITH_PLATFORM(TestEventWithId, MockTracingPlatform) {
  static uint64_t event_id = 21;
  TRACE_EVENT_ASYNC_BEGIN0("v8-cat", "a1", event_id);
  TRACE_EVENT_ASYNC_END0("v8-cat", "a1", event_id);

  CHECK_EQ(2, platform.NumberOfTraceObjects());
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, platform.GetTraceObject(0)->phase);
  CHECK_EQ(event_id, platform.GetTraceObject(0)->id);
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_END, platform.GetTraceObject(1)->phase);
  CHECK_EQ(event_id, platform.GetTraceObject(1)->id);
}

TEST_WITH_PLATFORM(TestEventWithTimestamp, MockTracingPlatform) {
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0("v8-cat", "0arg",
                                      TRACE_EVENT_SCOPE_GLOBAL, 1729);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1("v8-cat", "1arg",
                                      TRACE_EVENT_SCOPE_GLOBAL, 4104, "val", 1);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("v8-cat", "mark", 13832, "a", 1, "b", 2);

  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0("v8-cat", "begin", 5,
                                                        20683);
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("v8-cat", "end", 5,
                                                      32832);

  CHECK_EQ(5, platform.NumberOfTraceObjects());

  CHECK_EQ(1729, platform.GetTraceObject(0)->timestamp);
  CHECK_EQ(0, platform.GetTraceObject(0)->num_args);

  CHECK_EQ(4104, platform.GetTraceObject(1)->timestamp);
  CHECK_EQ(1, platform.GetTraceObject(1)->num_args);

  CHECK_EQ(13832, platform.GetTraceObject(2)->timestamp);
  CHECK_EQ(2, platform.GetTraceObject(2)->num_args);

  CHECK_EQ(20683, platform.GetTraceObject(3)->timestamp);
  CHECK_EQ(32832, platform.GetTraceObject(4)->timestamp);
}

TEST_WITH_PLATFORM(BuiltinsIsTraceCategoryEnabled, MockTracingPlatform) {
  CcTest::InitializeVM();

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  LocalContext env;

  v8::Local<v8::Object> binding = env->GetExtrasBindingObject();
  CHECK(!binding.IsEmpty());

  auto undefined = v8::Undefined(isolate);
  auto isTraceCategoryEnabled =
      binding->Get(env.local(), v8_str("isTraceCategoryEnabled"))
          .ToLocalChecked()
          .As<v8::Function>();

  {
    // Test with an enabled category
    v8::Local<v8::Value> argv[] = {v8_str("v8-cat")};
    auto result = isTraceCategoryEnabled->Call(env.local(), undefined, 1, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(result->BooleanValue(isolate));
  }

  {
    // Test with a disabled category
    v8::Local<v8::Value> argv[] = {v8_str("cat")};
    auto result = isTraceCategoryEnabled->Call(env.local(), undefined, 1, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(!result->BooleanValue(isolate));
  }

  {
    // Test with an enabled utf8 category
    v8::Local<v8::Value> argv[] = {v8_str("v8-cat\u20ac")};
    auto result = isTraceCategoryEnabled->Call(env.local(), undefined, 1, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(result->BooleanValue(isolate));
  }
}

TEST_WITH_PLATFORM(BuiltinsTrace, MockTracingPlatform) {
  CcTest::InitializeVM();

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  LocalContext env;

  v8::Local<v8::Object> binding = env->GetExtrasBindingObject();
  CHECK(!binding.IsEmpty());

  auto undefined = v8::Undefined(isolate);
  auto trace = binding->Get(env.local(), v8_str("trace"))
                   .ToLocalChecked()
                   .As<v8::Function>();

  // Test with disabled category
  {
    v8::Local<v8::String> category = v8_str("cat");
    v8::Local<v8::String> name = v8_str("name");
    v8::Local<v8::Value> argv[] = {
        v8::Integer::New(isolate, 'b'),                // phase
        category, name, v8::Integer::New(isolate, 0),  // id
        undefined                                      // data
    };
    auto result = trace->Call(env.local(), undefined, 5, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(!result->BooleanValue(isolate));
    CHECK_EQ(0, platform.NumberOfTraceObjects());
  }

  // Test with enabled category
  {
    v8::Local<v8::String> category = v8_str("v8-cat");
    v8::Local<v8::String> name = v8_str("name");
    v8::Local<v8::Object> data = v8::Object::New(isolate);
    data->Set(context, v8_str("foo"), v8_str("bar")).FromJust();
    v8::Local<v8::Value> argv[] = {
        v8::Integer::New(isolate, 'b'),                  // phase
        category, name, v8::Integer::New(isolate, 123),  // id
        data                                             // data arg
    };
    auto result = trace->Call(env.local(), undefined, 5, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(result->BooleanValue(isolate));
    CHECK_EQ(1, platform.NumberOfTraceObjects());

    CHECK_EQ(123, platform.GetTraceObject(0)->id);
    CHECK_EQ('b', platform.GetTraceObject(0)->phase);
    CHECK_EQ("name", platform.GetTraceObject(0)->name);
    CHECK_EQ(1, platform.GetTraceObject(0)->num_args);
  }

  // Test with enabled utf8 category
  {
    v8::Local<v8::String> category = v8_str("v8-cat\u20ac");
    v8::Local<v8::String> name = v8_str("name\u20ac");
    v8::Local<v8::Object> data = v8::Object::New(isolate);
    data->Set(context, v8_str("foo"), v8_str("bar")).FromJust();
    v8::Local<v8::Value> argv[] = {
        v8::Integer::New(isolate, 'b'),                  // phase
        category, name, v8::Integer::New(isolate, 123),  // id
        data                                             // data arg
    };
    auto result = trace->Call(env.local(), undefined, 5, argv)
                      .ToLocalChecked()
                      .As<v8::Boolean>();

    CHECK(result->BooleanValue(isolate));
    CHECK_EQ(2, platform.NumberOfTraceObjects());

    CHECK_EQ(123, platform.GetTraceObject(1)->id);
    CHECK_EQ('b', platform.GetTraceObject(1)->phase);
    CHECK_EQ("name\u20ac", platform.GetTraceObject(1)->name);
    CHECK_EQ(1, platform.GetTraceObject(1)->num_args);
  }
}
