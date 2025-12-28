// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <thread>  // NOLINT(build/c++11)

#include "include/v8-context.h"
#include "include/v8-function-callback.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-promise.h"
#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/handles/global-handles.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-js.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/heap/heap-utils.h"
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
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  CHECK(!wasm_streaming_callback_got_called);
  wasm_streaming_callback_got_called = true;

  i::Handle<i::Object> global_handle =
      reinterpret_cast<i::Isolate*>(info.GetIsolate())
          ->global_handles()
          ->Create(*Utils::OpenDirectHandle(*info.Data()));
  i::GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                             WasmStreamingTestFinalizer,
                             WeakCallbackType::kParameter);
}

void WasmStreamingCallbackTestFinishWithSuccess(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  streaming->OnBytesReceived(kMinimalWasmModuleBytes,
                             arraysize(kMinimalWasmModuleBytes));
  streaming->Finish(WasmStreaming::ModuleCachingCallback{});
}

void WasmStreamingCallbackTestFinishWithFailure(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  streaming->Finish(WasmStreaming::ModuleCachingCallback{});
}

void WasmStreamingCallbackTestAbortWithReject(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  streaming->Abort(Object::New(info.GetIsolate()));
}

void WasmStreamingCallbackTestAbortNoReject(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  streaming->Abort({});
}

void WasmStreamingCallbackTestOnBytesReceived(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());

  // The first bytes of the WebAssembly magic word.
  const uint8_t bytes[]{0x00, 0x61, 0x73};
  streaming->OnBytesReceived(bytes, arraysize(bytes));
}

void WasmStreamingMoreFunctionsCanBeSerializedCallback(
    const FunctionCallbackInfo<Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  std::shared_ptr<WasmStreaming> streaming =
      WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  streaming->SetMoreFunctionsCanBeSerializedCallback([](CompiledWasmModule) {});
}

TEST_F(ApiWasmTest, WasmStreamingCallback) {
  TestWasmStreaming(WasmStreamingCallbackTestCallbackIsCalled,
                    Promise::kPending);
  CHECK(wasm_streaming_callback_got_called);
  {
    // We need to invoke GC without stack, otherwise the WasmStreaming data may
    // not be reclaimed.
    i::DisableConservativeStackScanningScopeForTesting no_css_scope(
        i_isolate()->heap());
    InvokeMemoryReducingMajorGCs(i_isolate());
  }
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

TEST_F(ApiWasmTest, WasmStreamingSetCallback) {
  TestWasmStreaming(WasmStreamingMoreFunctionsCanBeSerializedCallback,
                    Promise::kPending);
}

TEST_F(ApiWasmTest, WasmErrorIsSharedCrossOrigin) {
  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope cscope(context);

  TryCatch try_catch(isolate());
  // A fairly minimal Wasm module that produces an error at runtime:
  // it returns {null} from an imported function that's typed to return
  // a non-null reference.
  const char* expected_message =
      "Uncaught TypeError: type incompatibility when transforming from/to JS";
  const char* src =
      "let raw = new Uint8Array(["
      "  0x00, 0x61, 0x73, 0x6d,  // wasm magic                            \n"
      "  0x01, 0x00, 0x00, 0x00,  // wasm version                          \n"

      "  0x01, 0x06,              // Type section, length 6                \n"
      "  0x01, 0x60,              // 1 type, kind: func                    \n"
      "  0x00, 0x01, 0x64, 0x6f,  // 0 params, 1 result: (ref extern)      \n"

      "  0x02, 0x07, 0x01,        // Import section, length 7, 1 import    \n"
      "  0x01, 0x6d, 0x01, 0x6e,  // 'm' 'n'                               \n"
      "  0x00, 0x00,              // kind: function $type0                 \n"

      "  0x03, 0x02,              // Function section, length 2            \n"
      "  0x01, 0x00,              // 1 function, $type0                    \n"

      "  0x07, 0x05, 0x01,        // Export section, length 5, 1 export    \n"
      "  0x01, 0x66, 0x00, 0x01,  // 'f': function #1                      \n"

      "  0x0a, 0x06, 0x01,        // Code section, length 6, 1 function    \n"
      "  0x04, 0x00,              // body size 4, 0 locals                 \n"
      "  0x10, 0x00, 0x0b,        // call $m.n; end                        \n"
      "]);                                                                 \n"

      "let mod = new WebAssembly.Module(raw.buffer);                       \n"
      "let instance = new WebAssembly.Instance(mod, {m: {n: () => null}}); \n"
      "instance.exports.f();";

  TryRunJS(src);
  EXPECT_TRUE(try_catch.HasCaught());
  Local<Message> message = try_catch.Message();
  CHECK_EQ(0, strcmp(*String::Utf8Value(isolate(), message->Get()),
                     expected_message));
  EXPECT_TRUE(message->IsSharedCrossOrigin());
}

TEST_F(ApiWasmTest, WasmEnableDisableCustomDescriptors) {
  Local<Context> context_local = Context::New(isolate());
  Context::Scope context_scope(context_local);
  i::DirectHandle<i::NativeContext> context =
      v8::Utils::OpenDirectHandle(*context_local);
  // Test enabling/disabling via flag.
  {
    i::FlagScope<bool> flag_descriptors(
        &i::v8_flags.experimental_wasm_custom_descriptors, true);
    EXPECT_TRUE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));

    // When flag is on, callback return value has no effect.
    isolate()->SetWasmCustomDescriptorsEnabledCallback(
        [](auto) { return true; });
    EXPECT_TRUE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
    EXPECT_TRUE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                    .has_custom_descriptors());
    isolate()->SetWasmCustomDescriptorsEnabledCallback(
        [](auto) { return false; });
    EXPECT_TRUE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
    EXPECT_TRUE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                    .has_custom_descriptors());
  }
  {
    i::FlagScope<bool> flag_descriptors(
        &i::v8_flags.experimental_wasm_custom_descriptors, false);
    EXPECT_FALSE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));

    // Test enabling/disabling via callback.
    isolate()->SetWasmCustomDescriptorsEnabledCallback(
        [](auto) { return true; });
    EXPECT_TRUE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
    EXPECT_TRUE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                    .has_custom_descriptors());
    isolate()->SetWasmCustomDescriptorsEnabledCallback(
        [](auto) { return false; });
    EXPECT_FALSE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
    EXPECT_FALSE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                     .has_custom_descriptors());
  }
}

TEST_F(ApiWasmTest, WasmModuleCompilation_Basic) {
  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope cscope(context);

  TryCatch try_catch(isolate());

  // Start compilation.
  WasmModuleCompilation compilation;

  // Pass minimal bytes.
  compilation.OnBytesReceived(kMinimalWasmModuleBytes,
                              sizeof(kMinimalWasmModuleBytes));

  // Finish compilation.
  WasmModuleCompilation::ModuleCachingCallback no_caching_callback;
  MaybeLocal<WasmModuleObject> module_object;
  compilation.Finish(
      isolate(), no_caching_callback,
      [&module_object](
          std::variant<Local<WasmModuleObject>, Local<Value>> module_or_error) {
        CHECK(std::holds_alternative<Local<WasmModuleObject>>(module_or_error));
        CHECK(module_object.IsEmpty());
        module_object = std::get<Local<WasmModuleObject>>(module_or_error);
      });

  // Execute pending tasks.
  EmptyMessageQueues();

  // The callback must have been called without any exception.
  CHECK(!module_object.IsEmpty());
  CHECK(!try_catch.HasCaught());
  CHECK(!isolate()->HasPendingException());
}

TEST_F(ApiWasmTest, WasmModuleCompilation_MultiThreaded) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  // The module we are about to compile. It contains two functions, each
  // returning a constant.
  static const uint8_t module_bytes[] = {
      WASM_MODULE_HEADER, SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_x(kI32Code)),
      SECTION(Function, ENTRY_COUNT(2), SIG_INDEX(0), SIG_INDEX(0)),
      SECTION(Code, ENTRY_COUNT(2),
              ADD_COUNT(WASM_NO_LOCALS, WASM_I32V_1(1), WASM_END),
              ADD_COUNT(WASM_NO_LOCALS, WASM_I32V_1(2), WASM_END))};

  base::Vector<const uint8_t> remaining_bytes = base::VectorOf(module_bytes);
  auto next_split =
      [&remaining_bytes,
       rng = base::RandomNumberGenerator(
           i::v8_flags.random_seed)]() mutable -> base::Vector<const uint8_t> {
    if (remaining_bytes.empty()) return {};
    size_t split = static_cast<size_t>(
        rng.NextInt(static_cast<int>(remaining_bytes.size())));
    auto split_bytes = remaining_bytes.SubVector(0, split);
    remaining_bytes += split;
    return split_bytes;
  };
  base::Vector<const uint8_t> bytes_0 = next_split();
  base::Vector<const uint8_t> bytes_1 = next_split();

  // We spawn multiple threads to start compilation and deliver the bytes in
  // pieces from multiple threads. Eventually the foreground task will finish
  // compilation.
  std::atomic<int> next_step{0};
  std::unique_ptr<WasmModuleCompilation> compilation;
  std::thread threads[]{
      std::thread{[&] {
        // Start compilation.
        compilation = std::make_unique<WasmModuleCompilation>();
        next_step.store(1, std::memory_order_release);
      }},
      std::thread{[&] {
        while (next_step.load(std::memory_order_acquire) != 1) continue;
        // Deliver first split of the module bytes.
        compilation->OnBytesReceived(bytes_0.data(), bytes_0.size());
        next_step.store(2, std::memory_order_release);
      }},
      std::thread{[&] {
        while (next_step.load(std::memory_order_acquire) != 2) continue;
        // Deliver second split of the module bytes.
        compilation->OnBytesReceived(bytes_1.data(), bytes_1.size());
        next_step.store(3, std::memory_order_release);
      }},
      std::thread{[&] {
        while (next_step.load(std::memory_order_acquire) != 3) continue;
        // Deliver remaining module bytes.
        compilation->OnBytesReceived(remaining_bytes.data(),
                                     remaining_bytes.size());
        next_step.store(4, std::memory_order_release);
      }},
  };

  // Wait for background work to finish.
  while (next_step.load(std::memory_order_acquire) != 4) continue;

  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope cscope(context);

  TryCatch try_catch(isolate());

  // Finish compilation from foreground.
  WasmModuleCompilation::ModuleCachingCallback no_caching_callback;
  MaybeLocal<WasmModuleObject> module_object;
  compilation->Finish(
      isolate(), no_caching_callback,
      [&module_object](
          std::variant<Local<WasmModuleObject>, Local<Value>> module_or_error) {
        CHECK(std::holds_alternative<Local<WasmModuleObject>>(module_or_error));
        CHECK(module_object.IsEmpty());
        module_object = std::get<Local<WasmModuleObject>>(module_or_error);
      });

  // Execute pending tasks.
  EmptyMessageQueues();

  // The callback must have been called without any exception.
  CHECK(!module_object.IsEmpty());
  CHECK(!try_catch.HasCaught());
  CHECK(!isolate()->HasPendingException());

  // Join all background threads before finishing.
  for (auto& t : threads) t.join();
}

}  // namespace v8
