// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-measurement-inl.h"
#include "src/heap/memory-measurement.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {
Handle<NativeContext> GetNativeContext(Isolate* isolate,
                                       v8::Local<v8::Context> v8_context) {
  Handle<Context> context = v8::Utils::OpenHandle(*v8_context);
  return handle(context->native_context(), isolate);
}
}  // anonymous namespace

TEST(NativeContextInferrerGlobalObject) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  Handle<JSGlobalObject> global =
      handle(native_context->global_object(), isolate);
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  CHECK(inferrer.Infer(isolate, global->map(), *global, &inferred_context));
  CHECK_EQ(native_context->ptr(), inferred_context);
}

TEST(NativeContextInferrerJSFunction) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  v8::Local<v8::Value> result = CompileRun("(function () { return 1; })");
  Handle<Object> object = Utils::OpenHandle(*result);
  Handle<HeapObject> function = Handle<HeapObject>::cast(object);
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  CHECK(inferrer.Infer(isolate, function->map(), *function, &inferred_context));
  CHECK_EQ(native_context->ptr(), inferred_context);
}

TEST(NativeContextInferrerJSObject) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  v8::Local<v8::Value> result = CompileRun("({a : 10})");
  Handle<Object> object = Utils::OpenHandle(*result);
  Handle<HeapObject> function = Handle<HeapObject>::cast(object);
  NativeContextInferrer inferrer;
  Address inferred_context = 0;
  // TODO(ulan): Enable this test once we have more precise native
  // context inference.
  CHECK(inferrer.Infer(isolate, function->map(), *function, &inferred_context));
  CHECK_EQ(native_context->ptr(), inferred_context);
}

TEST(NativeContextStatsMerge) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  v8::Local<v8::Value> result = CompileRun("({a : 10})");
  Handle<HeapObject> object =
      Handle<HeapObject>::cast(Utils::OpenHandle(*result));
  NativeContextStats stats1, stats2;
  stats1.IncrementSize(native_context->ptr(), object->map(), *object, 10);
  stats2.IncrementSize(native_context->ptr(), object->map(), *object, 20);
  stats1.Merge(stats2);
  CHECK_EQ(30, stats1.Get(native_context->ptr()));
}

TEST(NativeContextStatsArrayBuffers) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(CcTest::isolate(), 1000);
  Handle<JSArrayBuffer> i_array_buffer = Utils::OpenHandle(*array_buffer);
  NativeContextStats stats;
  stats.IncrementSize(native_context->ptr(), i_array_buffer->map(),
                      *i_array_buffer, 10);
  CHECK_EQ(1010, stats.Get(native_context->ptr()));
}

namespace {

class TestResource : public v8::String::ExternalStringResource {
 public:
  explicit TestResource(uint16_t* data) : data_(data), length_(0) {
    while (data[length_]) ++length_;
  }

  ~TestResource() override { i::DeleteArray(data_); }

  const uint16_t* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  uint16_t* data_;
  size_t length_;
};

}  // anonymous namespace

TEST(NativeContextStatsExternalString) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = GetNativeContext(isolate, env.local());
  const char* c_source = "0123456789";
  uint16_t* two_byte_source = AsciiToTwoByteString(c_source);
  TestResource* resource = new TestResource(two_byte_source);
  Local<v8::String> string =
      v8::String::NewExternalTwoByte(CcTest::isolate(), resource)
          .ToLocalChecked();
  Handle<String> i_string = Utils::OpenHandle(*string);
  NativeContextStats stats;
  stats.IncrementSize(native_context->ptr(), i_string->map(), *i_string, 10);
  CHECK_EQ(10 + 10 * 2, stats.Get(native_context->ptr()));
}

namespace {

class MockPlatform : public TestPlatform {
 public:
  MockPlatform()
      : old_platform_(i::V8::GetCurrentPlatform()),
        mock_task_runner_(new MockTaskRunner()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }

  ~MockPlatform() override { i::V8::SetPlatformForTesting(old_platform_); }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate*) override {
    return mock_task_runner_;
  }

  double Delay() { return mock_task_runner_->Delay(); }

  void PerformTask() { mock_task_runner_->PerformTask(); }

 private:
  class MockTaskRunner : public v8::TaskRunner {
   public:
    void PostTask(std::unique_ptr<v8::Task> task) override {}

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      task_ = std::move(task);
      delay_ = delay_in_seconds;
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      UNREACHABLE();
    }

    bool IdleTasksEnabled() override { return false; }

    double Delay() { return delay_; }

    void PerformTask() {
      std::unique_ptr<Task> task = std::move(task_);
      task->Run();
    }

   private:
    double delay_ = -1;
    std::unique_ptr<Task> task_;
  };
  v8::Platform* old_platform_;
  std::shared_ptr<MockTaskRunner> mock_task_runner_;
};

class MockMeasureMemoryDelegate : public v8::MeasureMemoryDelegate {
 public:
  bool ShouldMeasure(v8::Local<v8::Context> context) override { return true; }

  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override {
    // Empty.
  }
};

}  // namespace

TEST(RandomizedTimeout) {
  CcTest::InitializeVM();
  MockPlatform platform;
  std::vector<double> delays;
  for (int i = 0; i < 10; i++) {
    CcTest::isolate()->MeasureMemory(
        std::make_unique<MockMeasureMemoryDelegate>());
    delays.push_back(platform.Delay());
    platform.PerformTask();
  }
  std::sort(delays.begin(), delays.end());
  CHECK_LT(delays[0], delays.back());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
