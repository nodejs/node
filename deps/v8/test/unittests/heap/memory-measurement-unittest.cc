// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-measurement.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "include/v8-array-buffer.h"
#include "include/v8-local-handle.h"
#include "include/v8-platform.h"
#include "include/v8-primitive.h"
#include "include/v8-statistics.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/heap/memory-measurement-inl.h"
#include "src/libplatform/default-platform.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/smi.h"
#include "src/objects/tagged-field-inl.h"
#include "src/utils/allocation.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {
namespace {

class TestResource : public v8::String::ExternalStringResource {
 public:
  explicit TestResource(uint16_t* data) : data_(data) {
    while (data[length_]) ++length_;
  }

  ~TestResource() override { DeleteArray(data_); }

  const uint16_t* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  uint16_t* data_;
  size_t length_ = 0;
};

class MockMeasureMemoryDelegate : public v8::MeasureMemoryDelegate {
 public:
  bool ShouldMeasure(v8::Local<v8::Context> context) override { return true; }

  void MeasurementComplete(Result result) override {}
};

class MockTaskRunner : public v8::TaskRunner {
 public:
  void PostTaskImpl(std::unique_ptr<Task> task,
                    const SourceLocation& location) override {
    task_ = std::move(task);
  }

  void PostNonNestableTaskImpl(std::unique_ptr<Task> task,
                               const SourceLocation& location) override {
    PostTask(std::move(task));
  }

  void PostDelayedTaskImpl(std::unique_ptr<Task> task, double delay_in_seconds,
                           const SourceLocation& location) override {
    delay_ = delay_in_seconds;
    PostTask(std::move(task));
  }

  void PostNonNestableDelayedTaskImpl(std::unique_ptr<Task> task,
                                      double delay_in_seconds,
                                      const SourceLocation& location) override {
    PostDelayedTaskImpl(std::move(task), delay_in_seconds, location);
  }

  void PostIdleTaskImpl(std::unique_ptr<IdleTask> task,
                        const SourceLocation& location) override {
    UNREACHABLE();
  }

  bool IdleTasksEnabled() override { return false; }
  bool NonNestableTasksEnabled() const override { return true; }
  bool NonNestableDelayedTasksEnabled() const override { return true; }

  bool PendingTask() const { return task_ != nullptr; }

  double Delay() const { return delay_; }

  void PerformTask() {
    std::unique_ptr<Task> task = std::move(task_);
    task->Run();
  }

 private:
  double delay_ = -1;
  std::unique_ptr<Task> task_;
};

class MockPlatform : public v8::platform::DefaultPlatform {
 public:
  explicit MockPlatform(v8::Platform* default_platform)
      : default_platform_(default_platform),
        task_runner_(std::make_shared<MockTaskRunner>()) {
    CHECK_NOT_NULL(default_platform_);
    EnsureBackgroundTaskRunnerInitialized();
    V8::SetPlatformForTesting(this);
  }

  ~MockPlatform() override {
    if (V8::GetCurrentPlatform() == this) {
      V8::SetPlatformForTesting(default_platform_);
    }
  }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate*, v8::TaskPriority) override {
    return task_runner_;
  }

  bool PendingTask() const { return task_runner_->PendingTask(); }
  void PerformTask() { task_runner_->PerformTask(); }
  double Delay() const { return task_runner_->Delay(); }

 private:
  v8::Platform* default_platform_;
  std::shared_ptr<MockTaskRunner> task_runner_;
};

template <typename TMixin>
class WithMockPlatformMixin : public TMixin {
 public:
  WithMockPlatformMixin() : mock_platform_(this->platform()) {}
  WithMockPlatformMixin(const WithMockPlatformMixin&) = delete;
  WithMockPlatformMixin& operator=(const WithMockPlatformMixin&) = delete;

  MockPlatform& mock_platform() { return mock_platform_; }

 private:
  MockPlatform mock_platform_;
};

using MemoryMeasurementTest = TestWithHeapInternalsAndContext;
using MemoryMeasurementWithMockPlatformTest = WithIsolateScopeMixin<
    WithIsolateMixin<WithMockPlatformMixin<TestWithPlatform>>>;

}  // namespace

TEST_F(MemoryMeasurementTest, NativeContextInferrerGlobalObject) {
  DirectHandle<JSGlobalObject> global(native_context()->global_object(),
                                      i_isolate());
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  EXPECT_TRUE(
      inferrer.Infer(i_isolate(), global->map(), *global, &inferred_context));
  EXPECT_EQ(native_context()->ptr(), inferred_context);
}

TEST_F(MemoryMeasurementTest, NativeContextInferrerJSFunction) {
  DirectHandle<HeapObject> function =
      RunJS<HeapObject>("(function () { return 1; })");
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  EXPECT_TRUE(inferrer.Infer(i_isolate(), function->map(), *function,
                             &inferred_context));
  EXPECT_EQ(native_context()->ptr(), inferred_context);
}

TEST_F(MemoryMeasurementTest, NativeContextInferrerJSObject) {
  DirectHandle<HeapObject> object = RunJS<HeapObject>("({a : 10})");
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  EXPECT_TRUE(
      inferrer.Infer(i_isolate(), object->map(), *object, &inferred_context));
  EXPECT_EQ(native_context()->ptr(), inferred_context);
}

TEST_F(MemoryMeasurementTest, NativeContextStatsMerge) {
  DirectHandle<HeapObject> object = RunJS<HeapObject>("({a : 10})");
  NativeContextStats stats1, stats2;
  stats1.IncrementSize(native_context()->ptr(), object->map(), *object, 10);
  stats2.IncrementSize(native_context()->ptr(), object->map(), *object, 20);
  stats1.Merge(stats2);
  EXPECT_EQ(30u, stats1.Get(native_context()->ptr()));
}

TEST_F(MemoryMeasurementTest, NativeContextStatsArrayBuffers) {
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(v8_isolate(), 1000);
  DirectHandle<JSArrayBuffer> i_array_buffer =
      Utils::OpenDirectHandle(*array_buffer);
  NativeContextStats stats;
  stats.IncrementSize(native_context()->ptr(), i_array_buffer->map(),
                      *i_array_buffer, 10);
  EXPECT_EQ(1010u, stats.Get(native_context()->ptr()));
}

TEST_F(MemoryMeasurementTest, NativeContextStatsExternalString) {
  const char* c_source = "0123456789";
  uint16_t* two_byte_source = AsciiToTwoByteString(c_source);
  TestResource* resource = new TestResource(two_byte_source);
  v8::Local<v8::String> string =
      v8::String::NewExternalTwoByte(v8_isolate(), resource).ToLocalChecked();
  DirectHandle<String> i_string = Utils::OpenDirectHandle(*string);
  NativeContextStats stats;
  stats.IncrementSize(native_context()->ptr(), i_string->map(), *i_string, 10);
  EXPECT_EQ(10u + 10u * 2, stats.Get(native_context()->ptr()));
}

TEST_F(MemoryMeasurementWithMockPlatformTest, RandomizedTimeout) {
  std::vector<double> delays;
  for (int i = 0; i < 10; i++) {
    v8_isolate()->MeasureMemory(std::make_unique<MockMeasureMemoryDelegate>());
    delays.push_back(mock_platform().Delay());
    ASSERT_TRUE(mock_platform().PendingTask());
    mock_platform().PerformTask();
  }
  std::sort(delays.begin(), delays.end());
  EXPECT_LT(delays[0], delays.back());
}

TEST_F(MemoryMeasurementWithMockPlatformTest, LazyMemoryMeasurement) {
  v8_isolate()->MeasureMemory(std::make_unique<MockMeasureMemoryDelegate>(),
                              v8::MeasureMemoryExecution::kLazy);
  EXPECT_FALSE(mock_platform().PendingTask());
}

TEST_F(MemoryMeasurementTest, PartiallyInitializedJSFunction) {
  Isolate* isolate = i_isolate();
  // Use indirect handles across EmptyMessageQueues()'s stackless GC task.
  Handle<JSFunction> js_function = factory()->NewFunctionForTesting(
      factory()->NewStringFromAsciiChecked("test"));
  Handle<Context> context(js_function->context(), isolate);

  // 1. Start simulating deserialization.
  isolate->RegisterDeserializerStarted();
  // 2. Set the context field to the uninitialized sentinel.
  TaggedField<Object, offsetof(JSFunction, context_)>::store(
      *js_function, Smi::uninitialized_deserialization_value());
  // 3. Request memory measurement and run all tasks. GC that runs as part
  // of the measurement should not crash.
  v8_isolate()->MeasureMemory(std::make_unique<MockMeasureMemoryDelegate>(),
                              v8::MeasureMemoryExecution::kEager);
  EmptyMessageQueues();
  // 4. Restore the value and complete deserialization.
  TaggedField<Object, offsetof(JSFunction, context_)>::store(*js_function,
                                                             *context);
  isolate->RegisterDeserializerFinished();
}

TEST_F(MemoryMeasurementTest, PartiallyInitializedContext) {
  Isolate* isolate = i_isolate();
  DirectHandle<ScopeInfo> scope_info =
      factory()->global_this_binding_scope_info();
  Handle<Context> context = indirect_handle(
      factory()->NewScriptContext(native_context(), scope_info), isolate);
  Handle<Map> map(context->map(), isolate);
  Handle<NativeContext> saved_native_context(map->native_context(), isolate);
  // 1. Start simulating deserialization.
  isolate->RegisterDeserializerStarted();
  // 2. Set the native context field to the uninitialized sentinel.
  TaggedField<Object,
              offsetof(Map, constructor_or_back_pointer_or_native_context_)>::
      store(*map, Smi::uninitialized_deserialization_value());
  // 3. Request memory measurement and run all tasks. GC that runs as part
  // of the measurement should not crash.
  v8_isolate()->MeasureMemory(std::make_unique<MockMeasureMemoryDelegate>(),
                              v8::MeasureMemoryExecution::kEager);
  EmptyMessageQueues();
  // 4. Restore the value and complete deserialization.
  TaggedField<Object,
              offsetof(Map, constructor_or_back_pointer_or_native_context_)>::
      store(*map, *saved_native_context);
  isolate->RegisterDeserializerFinished();
}

}  // namespace v8::internal
