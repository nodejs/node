// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <string.h>

#include "src/v8.h"

#include "test/cctest/cctest.h"

#include "src/tracing/trace-event.h"

#define GET_TRACE_OBJECTS_LIST platform.GetMockTraceObjects()

#define GET_TRACE_OBJECT(Index) GET_TRACE_OBJECTS_LIST->at(Index)


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

typedef std::vector<MockTraceObject*> MockTraceObjectList;

class MockTracingController : public v8::TracingController {
 public:
  MockTracingController() = default;
  ~MockTracingController() override {
    for (size_t i = 0; i < trace_object_list_.size(); ++i) {
      delete trace_object_list_[i];
    }
    trace_object_list_.clear();
  }

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
    MockTraceObject* to = new MockTraceObject(
        phase, std::string(name), id, bind_id, num_args, flags, timestamp);
    trace_object_list_.push_back(to);
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

  MockTraceObjectList* GetMockTraceObjects() { return &trace_object_list_; }

 private:
  MockTraceObjectList trace_object_list_;

  DISALLOW_COPY_AND_ASSIGN(MockTracingController);
};

class MockTracingPlatform : public TestPlatform {
 public:
  MockTracingPlatform() {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }
  ~MockTracingPlatform() override = default;

  v8::TracingController* GetTracingController() override {
    return &tracing_controller_;
  }

  MockTraceObjectList* GetMockTraceObjects() {
    return tracing_controller_.GetMockTraceObjects();
  }

 private:
  MockTracingController tracing_controller_;
};


TEST(TraceEventDisabledCategory) {
  MockTracingPlatform platform;

  // Disabled category, will not add events.
  TRACE_EVENT_BEGIN0("cat", "e1");
  TRACE_EVENT_END0("cat", "e1");
  CHECK(GET_TRACE_OBJECTS_LIST->empty());
}


TEST(TraceEventNoArgs) {
  MockTracingPlatform platform;

  // Enabled category will add 2 events.
  TRACE_EVENT_BEGIN0("v8-cat", "e1");
  TRACE_EVENT_END0("v8-cat", "e1");

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ('B', GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ("e1", GET_TRACE_OBJECT(0)->name);
  CHECK_EQ(0, GET_TRACE_OBJECT(0)->num_args);

  CHECK_EQ('E', GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ("e1", GET_TRACE_OBJECT(1)->name);
  CHECK_EQ(0, GET_TRACE_OBJECT(1)->num_args);
}


TEST(TraceEventWithOneArg) {
  MockTracingPlatform platform;

  TRACE_EVENT_BEGIN1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_END1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_BEGIN1("v8-cat", "e2", "arg1", "abc");
  TRACE_EVENT_END1("v8-cat", "e2", "arg1", "abc");

  CHECK_EQ(4, GET_TRACE_OBJECTS_LIST->size());

  CHECK_EQ(1, GET_TRACE_OBJECT(0)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(2)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(3)->num_args);
}


TEST(TraceEventWithTwoArgs) {
  MockTracingPlatform platform;

  TRACE_EVENT_BEGIN2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_END2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_BEGIN2("v8-cat", "e2", "arg1", "abc", "arg2", 43);
  TRACE_EVENT_END2("v8-cat", "e2", "arg1", "abc", "arg2", 43);

  CHECK_EQ(4, GET_TRACE_OBJECTS_LIST->size());

  CHECK_EQ(2, GET_TRACE_OBJECT(0)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(1)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(2)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(3)->num_args);
}


TEST(ScopedTraceEvent) {
  MockTracingPlatform platform;

  { TRACE_EVENT0("v8-cat", "e"); }

  CHECK_EQ(1, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(0, GET_TRACE_OBJECT(0)->num_args);

  { TRACE_EVENT1("v8-cat", "e1", "arg1", "abc"); }

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);

  { TRACE_EVENT2("v8-cat", "e1", "arg1", "abc", "arg2", 42); }

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(2, GET_TRACE_OBJECT(2)->num_args);
}


TEST(TestEventWithFlow) {
  MockTracingPlatform platform;

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

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(0)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_OUT, GET_TRACE_OBJECT(0)->flags);
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(1)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
           GET_TRACE_OBJECT(1)->flags);
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(2)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN, GET_TRACE_OBJECT(2)->flags);
}


TEST(TestEventWithId) {
  MockTracingPlatform platform;

  static uint64_t event_id = 21;
  TRACE_EVENT_ASYNC_BEGIN0("v8-cat", "a1", event_id);
  TRACE_EVENT_ASYNC_END0("v8-cat", "a1", event_id);

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ(event_id, GET_TRACE_OBJECT(0)->id);
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_END, GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ(event_id, GET_TRACE_OBJECT(1)->id);
}

TEST(TestEventInContext) {
  MockTracingPlatform platform;

  static uint64_t isolate_id = 0x20151021;
  {
    TRACE_EVENT_SCOPED_CONTEXT("v8-cat", "Isolate", isolate_id);
    TRACE_EVENT0("v8-cat", "e");
  }

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->size());
  CHECK_EQ(TRACE_EVENT_PHASE_ENTER_CONTEXT, GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ("Isolate", GET_TRACE_OBJECT(0)->name);
  CHECK_EQ(isolate_id, GET_TRACE_OBJECT(0)->id);
  CHECK_EQ(TRACE_EVENT_PHASE_COMPLETE, GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ("e", GET_TRACE_OBJECT(1)->name);
  CHECK_EQ(TRACE_EVENT_PHASE_LEAVE_CONTEXT, GET_TRACE_OBJECT(2)->phase);
  CHECK_EQ("Isolate", GET_TRACE_OBJECT(2)->name);
  CHECK_EQ(isolate_id, GET_TRACE_OBJECT(2)->id);
}

TEST(TestEventWithTimestamp) {
  MockTracingPlatform platform;

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0("v8-cat", "0arg",
                                      TRACE_EVENT_SCOPE_GLOBAL, 1729);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1("v8-cat", "1arg",
                                      TRACE_EVENT_SCOPE_GLOBAL, 4104, "val", 1);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("v8-cat", "mark", 13832, "a", 1, "b", 2);

  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0("v8-cat", "begin", 5,
                                                        20683);
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("v8-cat", "end", 5,
                                                      32832);

  CHECK_EQ(5, GET_TRACE_OBJECTS_LIST->size());

  CHECK_EQ(1729, GET_TRACE_OBJECT(0)->timestamp);
  CHECK_EQ(0, GET_TRACE_OBJECT(0)->num_args);

  CHECK_EQ(4104, GET_TRACE_OBJECT(1)->timestamp);
  CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);

  CHECK_EQ(13832, GET_TRACE_OBJECT(2)->timestamp);
  CHECK_EQ(2, GET_TRACE_OBJECT(2)->num_args);

  CHECK_EQ(20683, GET_TRACE_OBJECT(3)->timestamp);
  CHECK_EQ(32832, GET_TRACE_OBJECT(4)->timestamp);
}

TEST(BuiltinsIsTraceCategoryEnabled) {
  CcTest::InitializeVM();
  MockTracingPlatform platform;

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

TEST(BuiltinsTrace) {
  CcTest::InitializeVM();
  MockTracingPlatform platform;

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
    CHECK_EQ(0, GET_TRACE_OBJECTS_LIST->size());
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
    CHECK_EQ(1, GET_TRACE_OBJECTS_LIST->size());

    CHECK_EQ(123, GET_TRACE_OBJECT(0)->id);
    CHECK_EQ('b', GET_TRACE_OBJECT(0)->phase);
    CHECK_EQ("name", GET_TRACE_OBJECT(0)->name);
    CHECK_EQ(1, GET_TRACE_OBJECT(0)->num_args);
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
    CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->size());

    CHECK_EQ(123, GET_TRACE_OBJECT(1)->id);
    CHECK_EQ('b', GET_TRACE_OBJECT(1)->phase);
    CHECK_EQ("name\u20ac", GET_TRACE_OBJECT(1)->name);
    CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);
  }
}
