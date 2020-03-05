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

}  // namespace heap
}  // namespace internal
}  // namespace v8
