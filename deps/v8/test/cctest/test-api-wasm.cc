// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/handles/global-handles.h"
#include "test/cctest/cctest.h"

namespace {

bool wasm_streaming_callback_got_called = false;
bool wasm_streaming_data_got_collected = false;

void WasmStreamingTestFinalizer(const v8::WeakCallbackInfo<void>& data) {
  CHECK(!wasm_streaming_data_got_collected);
  wasm_streaming_data_got_collected = true;
  i::GlobalHandles::Destroy(reinterpret_cast<i::Address*>(data.GetParameter()));
}

void WasmStreamingCallbackTestCallbackIsCalled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!wasm_streaming_callback_got_called);
  wasm_streaming_callback_got_called = true;

  i::Handle<i::Object> global_handle =
      reinterpret_cast<i::Isolate*>(args.GetIsolate())
          ->global_handles()
          ->Create(*v8::Utils::OpenHandle(*args.Data()));
  i::GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                             WasmStreamingTestFinalizer,
                             v8::WeakCallbackType::kParameter);
}

void WasmStreamingCallbackTestOnBytesReceived(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());

  // The first bytes of the WebAssembly magic word.
  const uint8_t bytes[]{0x00, 0x61, 0x73};
  streaming->OnBytesReceived(bytes, arraysize(bytes));
}

void WasmStreamingCallbackTestFinishWithSuccess(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  // The bytes of a minimal WebAssembly module.
  const uint8_t bytes[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  streaming->OnBytesReceived(bytes, arraysize(bytes));
  streaming->Finish();
}

void WasmStreamingCallbackTestFinishWithFailure(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Finish();
}

void WasmStreamingCallbackTestAbortWithReject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort(v8::Object::New(args.GetIsolate()));
}

void WasmStreamingCallbackTestAbortNoReject(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort({});
}

void TestWasmStreaming(v8::WasmStreamingCallback callback,
                       v8::Promise::PromiseState expected_state) {
  CcTest::isolate()->SetWasmStreamingCallback(callback);
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Call {WebAssembly.compileStreaming} with {null} as parameter. The parameter
  // is only really processed by the embedder, so for this test the value is
  // irrelevant.
  v8::Local<v8::Promise> promise = v8::Local<v8::Promise>::Cast(
      CompileRun("WebAssembly.compileStreaming(null)"));

  EmptyMessageQueues(isolate);
  CHECK_EQ(expected_state, promise->State());
}

}  // namespace

TEST(WasmStreamingCallback) {
  TestWasmStreaming(WasmStreamingCallbackTestCallbackIsCalled,
                    v8::Promise::kPending);
  CHECK(wasm_streaming_callback_got_called);
  CcTest::CollectAllAvailableGarbage();
  CHECK(wasm_streaming_data_got_collected);
}

TEST(WasmStreamingOnBytesReceived) {
  TestWasmStreaming(WasmStreamingCallbackTestOnBytesReceived,
                    v8::Promise::kPending);
}

TEST(WasmStreamingFinishWithSuccess) {
  TestWasmStreaming(WasmStreamingCallbackTestFinishWithSuccess,
                    v8::Promise::kFulfilled);
}

TEST(WasmStreamingFinishWithFailure) {
  TestWasmStreaming(WasmStreamingCallbackTestFinishWithFailure,
                    v8::Promise::kRejected);
}

TEST(WasmStreamingAbortWithReject) {
  TestWasmStreaming(WasmStreamingCallbackTestAbortWithReject,
                    v8::Promise::kRejected);
}

TEST(WasmStreamingAbortWithoutReject) {
  TestWasmStreaming(WasmStreamingCallbackTestAbortNoReject,
                    v8::Promise::kPending);
}

namespace {

bool wasm_threads_enabled_value = false;

bool MockWasmThreadsEnabledCallback(v8::Local<v8::Context>) {
  return wasm_threads_enabled_value;
}

}  // namespace

TEST(TestSetWasmThreadsEnabledCallback) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(CcTest::isolate());
  i::Handle<i::Context> i_context = v8::Utils::OpenHandle(*context);

  // {Isolate::AreWasmThreadsEnabled} calls the callback set by the embedder if
  // such a callback exists. Otherwise it returns
  // {FLAG_experimental_wasm_threads}. First we test that the flag is returned
  // correctly if no callback is set. Then we test that the flag is ignored if
  // the callback is set.

  i::FLAG_experimental_wasm_threads = false;
  CHECK(!i_isolate->AreWasmThreadsEnabled(i_context));

  i::FLAG_experimental_wasm_threads = true;
  CHECK(i_isolate->AreWasmThreadsEnabled(i_context));

  isolate->SetWasmThreadsEnabledCallback(MockWasmThreadsEnabledCallback);
  wasm_threads_enabled_value = false;
  CHECK(!i_isolate->AreWasmThreadsEnabled(i_context));

  wasm_threads_enabled_value = true;
  i::FLAG_experimental_wasm_threads = false;
  CHECK(i_isolate->AreWasmThreadsEnabled(i_context));
}
