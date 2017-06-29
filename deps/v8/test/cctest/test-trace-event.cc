// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <string.h>

#include "src/v8.h"

#include "src/list.h"
#include "src/list-inl.h"
#include "test/cctest/cctest.h"

using v8::IdleTask;
using v8::Task;
using v8::Isolate;

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
  MockTraceObject(char phase, std::string name, uint64_t id, uint64_t bind_id,
                  int num_args, int flags)
      : phase(phase),
        name(name),
        id(id),
        bind_id(bind_id),
        num_args(num_args),
        flags(flags) {}
};

typedef v8::internal::List<MockTraceObject*> MockTraceObjectList;

class MockTracingController : public v8::TracingController {
 public:
  MockTracingController() = default;
  ~MockTracingController() {
    for (int i = 0; i < trace_object_list_.length(); ++i) {
      delete trace_object_list_[i];
    }
    trace_object_list_.Clear();
  }

  uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override {
    MockTraceObject* to = new MockTraceObject(phase, std::string(name), id,
                                              bind_id, num_args, flags);
    trace_object_list_.Add(to);
    return 0;
  }

  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name, uint64_t handle) override {}

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    if (strcmp(name, "v8-cat")) {
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

class MockTracingPlatform : public v8::Platform {
 public:
  explicit MockTracingPlatform(v8::Platform* platform) {}
  virtual ~MockTracingPlatform() {}
  void CallOnBackgroundThread(Task* task,
                              ExpectedRuntime expected_runtime) override {}

  void CallOnForegroundThread(Isolate* isolate, Task* task) override {}

  void CallDelayedOnForegroundThread(Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {}

  double MonotonicallyIncreasingTime() override { return 0.0; }

  void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) override {}

  bool IdleTasksEnabled(Isolate* isolate) override { return false; }

  v8::TracingController* GetTracingController() override {
    return &tracing_controller_;
  }

  bool PendingIdleTask() { return false; }

  void PerformIdleTask(double idle_time_in_seconds) {}

  bool PendingDelayedTask() { return false; }

  void PerformDelayedTask() {}

  MockTraceObjectList* GetMockTraceObjects() {
    return tracing_controller_.GetMockTraceObjects();
  }

 private:
  MockTracingController tracing_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockTracingPlatform);
};


TEST(TraceEventDisabledCategory) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  // Disabled category, will not add events.
  TRACE_EVENT_BEGIN0("cat", "e1");
  TRACE_EVENT_END0("cat", "e1");
  CHECK_EQ(0, GET_TRACE_OBJECTS_LIST->length());

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(TraceEventNoArgs) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  // Enabled category will add 2 events.
  TRACE_EVENT_BEGIN0("v8-cat", "e1");
  TRACE_EVENT_END0("v8-cat", "e1");

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ('B', GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ("e1", GET_TRACE_OBJECT(0)->name);
  CHECK_EQ(0, GET_TRACE_OBJECT(0)->num_args);

  CHECK_EQ('E', GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ("e1", GET_TRACE_OBJECT(1)->name);
  CHECK_EQ(0, GET_TRACE_OBJECT(1)->num_args);

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(TraceEventWithOneArg) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  TRACE_EVENT_BEGIN1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_END1("v8-cat", "e1", "arg1", 42);
  TRACE_EVENT_BEGIN1("v8-cat", "e2", "arg1", "abc");
  TRACE_EVENT_END1("v8-cat", "e2", "arg1", "abc");

  CHECK_EQ(4, GET_TRACE_OBJECTS_LIST->length());

  CHECK_EQ(1, GET_TRACE_OBJECT(0)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(2)->num_args);
  CHECK_EQ(1, GET_TRACE_OBJECT(3)->num_args);

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(TraceEventWithTwoArgs) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  TRACE_EVENT_BEGIN2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_END2("v8-cat", "e1", "arg1", 42, "arg2", "abc");
  TRACE_EVENT_BEGIN2("v8-cat", "e2", "arg1", "abc", "arg2", 43);
  TRACE_EVENT_END2("v8-cat", "e2", "arg1", "abc", "arg2", 43);

  CHECK_EQ(4, GET_TRACE_OBJECTS_LIST->length());

  CHECK_EQ(2, GET_TRACE_OBJECT(0)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(1)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(2)->num_args);
  CHECK_EQ(2, GET_TRACE_OBJECT(3)->num_args);

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(ScopedTraceEvent) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  { TRACE_EVENT0("v8-cat", "e"); }

  CHECK_EQ(1, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(0, GET_TRACE_OBJECT(0)->num_args);

  { TRACE_EVENT1("v8-cat", "e1", "arg1", "abc"); }

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(1, GET_TRACE_OBJECT(1)->num_args);

  { TRACE_EVENT2("v8-cat", "e1", "arg1", "abc", "arg2", 42); }

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(2, GET_TRACE_OBJECT(2)->num_args);

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(TestEventWithFlow) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

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

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(0)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_OUT, GET_TRACE_OBJECT(0)->flags);
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(1)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
           GET_TRACE_OBJECT(1)->flags);
  CHECK_EQ(bind_id, GET_TRACE_OBJECT(2)->bind_id);
  CHECK_EQ(TRACE_EVENT_FLAG_FLOW_IN, GET_TRACE_OBJECT(2)->flags);

  i::V8::SetPlatformForTesting(old_platform);
}


TEST(TestEventWithId) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  static uint64_t event_id = 21;
  TRACE_EVENT_ASYNC_BEGIN0("v8-cat", "a1", event_id);
  TRACE_EVENT_ASYNC_END0("v8-cat", "a1", event_id);

  CHECK_EQ(2, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_BEGIN, GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ(event_id, GET_TRACE_OBJECT(0)->id);
  CHECK_EQ(TRACE_EVENT_PHASE_ASYNC_END, GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ(event_id, GET_TRACE_OBJECT(1)->id);

  i::V8::SetPlatformForTesting(old_platform);
}

TEST(TestEventInContext) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockTracingPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);

  static uint64_t isolate_id = 0x20151021;
  {
    TRACE_EVENT_SCOPED_CONTEXT("v8-cat", "Isolate", isolate_id);
    TRACE_EVENT0("v8-cat", "e");
  }

  CHECK_EQ(3, GET_TRACE_OBJECTS_LIST->length());
  CHECK_EQ(TRACE_EVENT_PHASE_ENTER_CONTEXT, GET_TRACE_OBJECT(0)->phase);
  CHECK_EQ("Isolate", GET_TRACE_OBJECT(0)->name);
  CHECK_EQ(isolate_id, GET_TRACE_OBJECT(0)->id);
  CHECK_EQ(TRACE_EVENT_PHASE_COMPLETE, GET_TRACE_OBJECT(1)->phase);
  CHECK_EQ("e", GET_TRACE_OBJECT(1)->name);
  CHECK_EQ(TRACE_EVENT_PHASE_LEAVE_CONTEXT, GET_TRACE_OBJECT(2)->phase);
  CHECK_EQ("Isolate", GET_TRACE_OBJECT(2)->name);
  CHECK_EQ(isolate_id, GET_TRACE_OBJECT(2)->id);

  i::V8::SetPlatformForTesting(old_platform);
}
