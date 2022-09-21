// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8-context.h"
#include "include/v8-function-callback.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-promise.h"
#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/handles/global-handles.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

bool wasm_streaming_callback_got_called = false;
bool wasm_streaming_data_got_collected = false;

// The bytes of a minimal WebAssembly module.
const uint8_t kMinimalWasmModuleBytes[]{0x00, 0x61, 0x73, 0x6d,
                                        0x01, 0x00, 0x00, 0x00};

class ApiWasmTest : public TestWithIsolate {
 public:
  void TestWasmStreaming(WasmStreamingCallback callback,
                         Promise::PromiseState expected_state) {
    isolate()->SetWasmStreamingCallback(callback);
    HandleScope scope(isolate());

    Local<Context> context = Context::New(isolate());
    Context::Scope context_scope(context);
    // Call {WebAssembly.compileStreaming} with {null} as parameter. The
    // parameter is only really processed by the embedder, so for this test the
    // value is irrelevant.
    Local<Promise> promise =
        Local<Promise>::Cast(RunJS("WebAssembly.compileStreaming(null)"));
    EmptyMessageQueues();
    CHECK_EQ(expected_state, promise->State());
  }
};

void WasmStreamingTestFinalizer(const WeakCallbackInfo<void>& data) {
  CHECK(!wasm_streaming_data_got_collected);
  wasm_streaming_data_got_collected = true;
  i::GlobalHandles::Destroy(reinterpret_cast<i::Address*>(data.GetParameter()));
}

void WasmStreamingCallbackTestCallbackIsCalled(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(!wasm_streaming_callback_got_called);
  wasm_streaming_callback_got_called = true;

  i::Handle<i::Object> global_handle =
      reinterpret_cast<i::Isolate*>(args.GetIsolate())
          ->global_handles()
          ->Create(*Utils::OpenHandle(*args.Data()));
  i::GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                             WasmStreamingTestFinalizer,
                             WeakCallbackType::kParameter);
}

void WasmStreamingCallbackTestFinishWithSuccess(
    const FunctionCallbackInfo<Value>& args) {
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->OnBytesReceived(kMinimalWasmModuleBytes,
                             arraysize(kMinimalWasmModuleBytes));
  streaming->Finish();
}

void WasmStreamingCallbackTestFinishWithFailure(
    const FunctionCallbackInfo<Value>& args) {
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Finish();
}

void WasmStreamingCallbackTestAbortWithReject(
    const FunctionCallbackInfo<Value>& args) {
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort(Object::New(args.GetIsolate()));
}

void WasmStreamingCallbackTestAbortNoReject(
    const FunctionCallbackInfo<Value>& args) {
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  streaming->Abort({});
}

void WasmStreamingCallbackTestOnBytesReceived(
    const FunctionCallbackInfo<Value>& args) {
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(args.GetIsolate(), args.Data());

  // The first bytes of the WebAssembly magic word.
  const uint8_t bytes[]{0x00, 0x61, 0x73};
  streaming->OnBytesReceived(bytes, arraysize(bytes));
}

TEST_F(ApiWasmTest, WasmStreamingCallback) {
  TestWasmStreaming(WasmStreamingCallbackTestCallbackIsCalled,
                    Promise::kPending);
  CHECK(wasm_streaming_callback_got_called);
  CollectAllAvailableGarbage();
  CHECK(wasm_streaming_data_got_collected);
}

TEST_F(ApiWasmTest, WasmStreamingOnBytesReceived) {
  TestWasmStreaming(WasmStreamingCallbackTestOnBytesReceived,
                    Promise::kPending);
}

TEST_F(ApiWasmTest, WasmStreamingFinishWithSuccess) {
  TestWasmStreaming(WasmStreamingCallbackTestFinishWithSuccess,
                    Promise::kFulfilled);
}

TEST_F(ApiWasmTest, WasmStreamingFinishWithFailure) {
  TestWasmStreaming(WasmStreamingCallbackTestFinishWithFailure,
                    Promise::kRejected);
}

TEST_F(ApiWasmTest, WasmStreamingAbortWithReject) {
  TestWasmStreaming(WasmStreamingCallbackTestAbortWithReject,
                    Promise::kRejected);
}

TEST_F(ApiWasmTest, WasmStreamingAbortWithoutReject) {
  TestWasmStreaming(WasmStreamingCallbackTestAbortNoReject, Promise::kPending);
}

TEST_F(ApiWasmTest, WasmCompileToWasmModuleObject) {
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);
  auto maybe_module = WasmModuleObject::Compile(
      isolate(), {kMinimalWasmModuleBytes, arraysize(kMinimalWasmModuleBytes)});
  CHECK(!maybe_module.IsEmpty());
}

namespace {

bool wasm_simd_enabled_value = false;
bool wasm_exceptions_enabled_value = false;

bool MockWasmSimdEnabledCallback(Local<Context>) {
  return wasm_simd_enabled_value;
}

bool MockWasmExceptionsEnabledCallback(Local<Context>) {
  return wasm_exceptions_enabled_value;
}

}  // namespace

TEST_F(ApiWasmTest, TestSetWasmSimdEnabledCallback) {
  Local<Context> context = Context::New(isolate());
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);

  // {Isolate::IsWasmSimdEnabled} calls the callback set by the embedder if
  // such a callback exists. Otherwise it returns
  // {v8_flags.experimental_wasm_simd}. First we test that the flag is returned
  // correctly if no callback is set. Then we test that the flag is ignored if
  // the callback is set.

  i::v8_flags.experimental_wasm_simd = false;
  CHECK(!i_isolate()->IsWasmSimdEnabled(i_context));

  i::v8_flags.experimental_wasm_simd = true;
  CHECK(i_isolate()->IsWasmSimdEnabled(i_context));

  isolate()->SetWasmSimdEnabledCallback(MockWasmSimdEnabledCallback);
  wasm_simd_enabled_value = false;
  CHECK(!i_isolate()->IsWasmSimdEnabled(i_context));

  wasm_simd_enabled_value = true;
  i::v8_flags.experimental_wasm_simd = false;
  CHECK(i_isolate()->IsWasmSimdEnabled(i_context));
}

TEST_F(ApiWasmTest, TestSetWasmExceptionsEnabledCallback) {
  Local<Context> context = Context::New(isolate());
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);

  // {Isolate::AreWasmExceptionsEnabled} calls the callback set by the embedder
  // if such a callback exists. Otherwise it returns
  // {v8_flags.experimental_wasm_eh}. First we test that the flag is returned
  // correctly if no callback is set. Then we test that the flag is ignored if
  // the callback is set.

  i::v8_flags.experimental_wasm_eh = false;
  CHECK(!i_isolate()->AreWasmExceptionsEnabled(i_context));

  i::v8_flags.experimental_wasm_eh = true;
  CHECK(i_isolate()->AreWasmExceptionsEnabled(i_context));

  isolate()->SetWasmExceptionsEnabledCallback(
      MockWasmExceptionsEnabledCallback);
  wasm_exceptions_enabled_value = false;
  CHECK(!i_isolate()->AreWasmExceptionsEnabled(i_context));

  wasm_exceptions_enabled_value = true;
  i::v8_flags.experimental_wasm_eh = false;
  CHECK(i_isolate()->AreWasmExceptionsEnabled(i_context));
}

}  // namespace v8
