// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <thread>  // NOLINT(build/c++11)

#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-function-callback.h"
#include "include/v8-function.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-promise.h"
#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/handles/global-handles.h"
#include "src/init/v8.h"
#include "src/objects/object-conversions-inl.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
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

  Local<WasmModuleObject> FinishModuleCompilation(
      WasmModuleCompilation& compilation, std::span<const uint8_t> bytes) {
    compilation.OnBytesReceived(bytes.data(), bytes.size());
    // The resolution callback runs within a separate task with its own
    // HandleScope, so persist the result in a Global.
    Global<WasmModuleObject> module_object;
    compilation.Finish(
        isolate(), {},
        [this,
         &module_object](std::variant<Local<WasmModuleObject>, Local<Value>>
                             module_or_error) {
          CHECK(
              std::holds_alternative<Local<WasmModuleObject>>(module_or_error));
          module_object.Reset(
              isolate(), std::get<Local<WasmModuleObject>>(module_or_error));
        });
    EmptyMessageQueues();
    CHECK(!module_object.IsEmpty());
    return module_object.Get(isolate());
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
  auto maybe_module =
      WasmModuleObject::Compile(isolate(), kMinimalWasmModuleBytes);
  CHECK(!maybe_module.IsEmpty());
}

namespace {
// Number of imports reflected by `WebAssembly.Module.imports`. Compile-time
// imports are bound during compilation and are not reflected here.
int ReflectedImportCount(Isolate* isolate, Local<WasmModuleObject> module) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::DirectHandle<i::WasmModuleObject> module_object =
      i::Cast<i::WasmModuleObject>(Utils::OpenDirectHandle(*module));
  i::DirectHandle<i::JSArray> imports =
      i::wasm::GetImports(i_isolate, module_object);
  return static_cast<int>(i::Object::NumberValue(imports->length()));
}
}  // namespace

TEST_F(ApiWasmTest, WasmCompileWithJsStringBuiltins) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  // Imports `charCodeAt` from the "wasm:js-string" builtins module with the
  // signature (externref, i32) -> i32, matching the builtin.
  const uint8_t valid_module[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1),
              SIG_ENTRY_x_xx(kI32Code, kExternRefCode, kI32Code)),
      SECTION(Import, ENTRY_COUNT(1),
              // module name: "wasm:js-string"
              14, 'w', 'a', 's', 'm', ':', 'j', 's', '-', 's', 't', 'r', 'i',
              'n', 'g',
              // field name: "charCodeAt"
              10, 'c', 'h', 'a', 'r', 'C', 'o', 'd', 'e', 'A', 't',
              kExternalFunction, 0)};
  // The same import but declaring an i64 result, which no builtin provides.
  const uint8_t invalid_module[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1),
              SIG_ENTRY_x_xx(kI64Code, kExternRefCode, kI32Code)),
      SECTION(Import, ENTRY_COUNT(1), 14, 'w', 'a', 's', 'm', ':', 'j', 's',
              '-', 's', 't', 'r', 'i', 'n', 'g', 10, 'c', 'h', 'a', 'r', 'C',
              'o', 'd', 'e', 'A', 't', kExternalFunction, 0)};

  WasmModuleObject::CompileOptions options{
      .builtins = WasmModuleObject::CompileOptions::Builtins::kJsString};

  // Without compile-time imports the "wasm:js-string" import is an ordinary
  // import resolved at instantiation, so it is reflected and both modules
  // compile.
  Local<WasmModuleObject> plain =
      WasmModuleObject::Compile(isolate(), valid_module).ToLocalChecked();
  CHECK_EQ(1, ReflectedImportCount(isolate(), plain));
  CHECK(!WasmModuleObject::Compile(isolate(), invalid_module).IsEmpty());

  // With the builtins enabled the valid import is bound at compile time, so it
  // is no longer reflected.
  Local<WasmModuleObject> bound =
      WasmModuleObject::Compile(isolate(), valid_module, options)
          .ToLocalChecked();
  CHECK_EQ(0, ReflectedImportCount(isolate(), bound));

  // The invalid signature is rejected at compile time.
  {
    TryCatch try_catch(isolate());
    CHECK(WasmModuleObject::Compile(isolate(), invalid_module, options)
              .IsEmpty());
    CHECK(try_catch.HasCaught());
  }
}

TEST_F(ApiWasmTest, WasmCompileWithImportedStringConstants) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  // Imports an externref global "foo" from the "strings" module, which is
  // turned into a string constant when the constants module is provided.
  const uint8_t module[] = {
      WASM_MODULE_HEADER,
      SECTION(Import, ENTRY_COUNT(1),
              // module name: "strings"
              7, 's', 't', 'r', 'i', 'n', 'g', 's',
              // field name: "foo"
              3, 'f', 'o', 'o', kExternalGlobal, kExternRefCode, 0)};

  // Without a constants module the global is an ordinary reflected import.
  Local<WasmModuleObject> plain =
      WasmModuleObject::Compile(isolate(), module).ToLocalChecked();
  CHECK_EQ(1, ReflectedImportCount(isolate(), plain));

  // Naming "strings" as the constants module binds the import at compile time,
  // so it is no longer reflected.
  Local<WasmModuleObject> bound =
      WasmModuleObject::Compile(isolate(), module,
                                {.imported_string_constants_module = "strings"})
          .ToLocalChecked();
  CHECK_EQ(0, ReflectedImportCount(isolate(), bound));
}

TEST_F(ApiWasmTest, WasmCompileWithSourceUrl) {
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  constexpr std::string_view kUrl1 = "file:///test/module1.wasm";
  Local<WasmModuleObject> with_url1 =
      WasmModuleObject::Compile(isolate(), kMinimalWasmModuleBytes,
                                {.source_url = kUrl1})
          .ToLocalChecked();
  CHECK_EQ(kUrl1, with_url1->GetCompiledModule().source_url());

  // Compiling the same module bytes with a different source URL creates a
  // separate script with its own URL rather than reusing the previous URL.
  constexpr std::string_view kUrl2 = "file:///test/module2.wasm";
  Local<WasmModuleObject> with_url2 =
      WasmModuleObject::Compile(isolate(), kMinimalWasmModuleBytes,
                                {.source_url = kUrl2})
          .ToLocalChecked();
  CHECK_EQ(kUrl2, with_url2->GetCompiledModule().source_url());

  // Without a source URL, a wasm:// URL is synthesized even when a script with
  // an explicit URL already exists for the same module bytes.
  Local<WasmModuleObject> plain =
      WasmModuleObject::Compile(isolate(), kMinimalWasmModuleBytes)
          .ToLocalChecked();
  CHECK_EQ(0u, plain->GetCompiledModule().source_url().find("wasm://wasm/"));
}

// Regression test for https://crbug.com/564190098.
TEST_F(ApiWasmTest, WasmCompileSourceUrlNotLeakedAcrossTrapStacks) {
  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());

  // Minimal Wasm module exporting `boom()` which traps with `unreachable`.
  const uint8_t kTrappingModuleBytes[] = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60,
      0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x08, 0x01, 0x04, 0x62, 0x6f,
      0x6f, 0x6d, 0x00, 0x00, 0x0a, 0x05, 0x01, 0x03, 0x00, 0x00, 0x0b};

  constexpr std::string_view kVictimUrl =
      "https://victim.example/module.wasm?secret=VICTIM_SECRET";
  constexpr std::string_view kAttackerUrl =
      "https://attacker.example/module.wasm";

  Local<Context> victim_context = Context::New(isolate());
  Local<WasmModuleObject> victim_module;
  {
    Context::Scope victim_scope(victim_context);
    victim_module = WasmModuleObject::Compile(isolate(), kTrappingModuleBytes,
                                              {.source_url = kVictimUrl})
                        .ToLocalChecked();
    CHECK_EQ(kVictimUrl, victim_module->GetCompiledModule().source_url());
  }

  Local<Context> attacker_context = Context::New(isolate());
  {
    Context::Scope attacker_scope(attacker_context);
    Local<WasmModuleObject> attacker_module =
        WasmModuleObject::Compile(isolate(), kTrappingModuleBytes,
                                  {.source_url = kAttackerUrl})
            .ToLocalChecked();
    CHECK_EQ(kAttackerUrl, attacker_module->GetCompiledModule().source_url());

    CHECK(attacker_context->Global()
              ->Set(attacker_context,
                    String::NewFromUtf8Literal(isolate(), "mod"),
                    attacker_module)
              .FromJust());
    Local<Value> stack_val = RunJS(
        "try {\n"
        "  new WebAssembly.Instance(mod).exports.boom();\n"
        "} catch (e) {\n"
        "  e.stack;\n"
        "}");
    String::Utf8Value stack_utf8(isolate(), stack_val);
    std::string_view stack(*stack_utf8, stack_utf8.length());
    CHECK_NE(std::string_view::npos, stack.find(kAttackerUrl));
    CHECK_EQ(std::string_view::npos, stack.find("VICTIM_SECRET"));
  }
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
    i::FlagScope<bool> flag_descriptors(&i::v8_flags.wasm_custom_descriptors,
                                        true);
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
    i::FlagScope<bool> flag_descriptors(&i::v8_flags.wasm_custom_descriptors,
                                        false);
    EXPECT_FALSE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));

    // Test enabling/disabling via callback.
    isolate()->SetWasmCustomDescriptorsEnabledCallback(
        [](auto) { return true; });
    EXPECT_TRUE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
    EXPECT_TRUE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                    .has_custom_descriptors());
    {
      // Test that the kill switch overrides any Origin Trial.
      i::FlagScope<bool> flag_kill_switch(
          &i::v8_flags.wasm_custom_descriptors_permitted, false);
      EXPECT_FALSE(i_isolate()->IsWasmCustomDescriptorsEnabled(context));
      EXPECT_FALSE(i::wasm::WasmEnabledFeatures::FromIsolate(i_isolate())
                       .has_custom_descriptors());
    }
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

TEST_F(ApiWasmTest, WasmModuleCompilation_CompileOptions) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope cscope(context);
  TryCatch try_catch(isolate());

  // Imports `charCodeAt` from the "wasm:js-string" builtins module, matching
  // the builtin signature, so it binds at compile time when enabled.
  const uint8_t module_bytes[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1),
              SIG_ENTRY_x_xx(kI32Code, kExternRefCode, kI32Code)),
      SECTION(Import, ENTRY_COUNT(1), 14, 'w', 'a', 's', 'm', ':', 'j', 's',
              '-', 's', 't', 'r', 'i', 'n', 'g', 10, 'c', 'h', 'a', 'r', 'C',
              'o', 'd', 'e', 'A', 't', kExternalFunction, 0)};

  constexpr std::string_view kUrl = "file:///test/async-module.wasm";
  WasmModuleCompilation compilation(
      {.builtins = WasmModuleObject::CompileOptions::Builtins::kJsString,
       .source_url = kUrl});
  Local<WasmModuleObject> module =
      FinishModuleCompilation(compilation, module_bytes);
  CHECK(!try_catch.HasCaught());
  CHECK_EQ(0, ReflectedImportCount(isolate(), module));
  CHECK_EQ(kUrl, module->GetCompiledModule().source_url());
}

TEST_F(ApiWasmTest, WasmModuleCompilation_SourceUrl) {
  Isolate::Scope iscope(isolate());
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope cscope(context);
  TryCatch try_catch(isolate());

  // SetUrl cleanly replaces a URL provided via compile options.
  {
    constexpr std::string_view kSetUrl = "file:///test/replaced.wasm";
    WasmModuleCompilation compilation(
        {.source_url = "file:///test/original.wasm"});
    compilation.SetUrl(kSetUrl.data(), kSetUrl.size());
    Local<WasmModuleObject> module =
        FinishModuleCompilation(compilation, kMinimalWasmModuleBytes);
    CHECK_EQ(kSetUrl, module->GetCompiledModule().source_url());
  }

  // Without a URL, a wasm:// URL is synthesized even for the same module bytes.
  {
    WasmModuleCompilation compilation;
    Local<WasmModuleObject> module =
        FinishModuleCompilation(compilation, kMinimalWasmModuleBytes);
    CHECK_EQ(0u, module->GetCompiledModule().source_url().find("wasm://wasm/"));
  }
  CHECK(!try_catch.HasCaught());
}

TEST_F(ApiWasmTest, GetWasmMemoryReservationSizeInBytes) {
  constexpr size_t kCapacity = 64 * 1024;  // 64 KiB
  size_t reservation = V8::GetWasmMemoryReservationSizeInBytes(
      V8::WasmMemoryType::kMemory32, kCapacity);
  size_t reservation64 = V8::GetWasmMemoryReservationSizeInBytes(
      V8::WasmMemoryType::kMemory64, kCapacity);

#if V8_TRAP_HANDLER_SUPPORTED
  EXPECT_GE(reservation, kCapacity);
  EXPECT_GE(reservation64, kCapacity);
#else
  EXPECT_EQ(reservation, kCapacity);
  EXPECT_EQ(reservation64, kCapacity);
#endif  // V8_TRAP_HANDLER_SUPPORTED
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

TEST_F(ApiWasmTest, WasmMemoryObjectGetBuffer) {
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  Local<WasmMemoryObject> unshared_memory =
      RunJS("new WebAssembly.Memory({initial: 1})").As<WasmMemoryObject>();
  Local<ArrayBufferBase> unshared_buffer = unshared_memory->GetBuffer();
  EXPECT_TRUE(unshared_buffer->IsArrayBufferBase());
  EXPECT_TRUE(unshared_buffer->IsArrayBuffer());
  EXPECT_FALSE(unshared_buffer->IsSharedArrayBuffer());
  EXPECT_FALSE(unshared_buffer->IsShared());

  Local<WasmMemoryObject> shared_memory =
      RunJS("new WebAssembly.Memory({initial: 1, maximum: 1, shared: true})")
          .As<WasmMemoryObject>();
  Local<ArrayBufferBase> shared_buffer = shared_memory->GetBuffer();
  EXPECT_TRUE(shared_buffer->IsArrayBufferBase());
  EXPECT_FALSE(shared_buffer->IsArrayBuffer());
  EXPECT_TRUE(shared_buffer->IsSharedArrayBuffer());
  EXPECT_TRUE(shared_buffer->IsShared());
}

TEST_F(ApiWasmTest, OwnedBufferSpanAndMoveSemantics) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  OwnedBuffer empty_buffer;
  EXPECT_TRUE(empty_buffer.empty());
  EXPECT_TRUE(empty_buffer.as_span().empty());
  EXPECT_EQ(nullptr, empty_buffer.as_span().data());

  i::FlagScope<bool> no_liftoff(&i::v8_flags.liftoff, false);
  i::FlagScope<bool> no_lazy_compilation(&i::v8_flags.wasm_lazy_compilation,
                                         false);
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);
  constexpr uint8_t kModuleWithFunctionBytes[] = {
      WASM_MODULE_HEADER, SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
      SECTION(Function, ENTRY_COUNT(1), 0),
      SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0, WASM_END))};
  Local<WasmModuleObject> module =
      WasmModuleObject::Compile(isolate(), kModuleWithFunctionBytes)
          .ToLocalChecked();
  OwnedBuffer owned = module->GetCompiledModule().Serialize();
  ASSERT_FALSE(owned.empty());
  const uint8_t* raw_ptr = owned.as_span().data();
  const size_t size = owned.as_span().size();
  EXPECT_NE(nullptr, raw_ptr);
  EXPECT_GT(size, 0u);
  const std::vector<uint8_t> expected_bytes(owned.as_span().begin(),
                                            owned.as_span().end());

  // Move construction transfers ownership and leaves the source empty.
  OwnedBuffer move_constructed(std::move(owned));
  EXPECT_TRUE(owned.empty());
  EXPECT_TRUE(owned.as_span().empty());
  EXPECT_EQ(nullptr, owned.as_span().data());
  EXPECT_EQ(raw_ptr, move_constructed.as_span().data());
  EXPECT_EQ(size, move_constructed.as_span().size());
  EXPECT_TRUE(std::ranges::equal(expected_bytes, move_constructed.as_span()));

  // Move assignment also leaves the source empty.
  OwnedBuffer move_assigned;
  move_assigned = std::move(move_constructed);
  EXPECT_TRUE(move_constructed.empty());
  EXPECT_TRUE(move_constructed.as_span().empty());
  EXPECT_EQ(nullptr, move_constructed.as_span().data());
  EXPECT_EQ(raw_ptr, move_assigned.as_span().data());
  EXPECT_EQ(size, move_assigned.as_span().size());
  EXPECT_TRUE(std::ranges::equal(expected_bytes, move_assigned.as_span()));
}

TEST_F(ApiWasmTest, WasmObjectSetPrototype) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  constexpr uint8_t kModuleBytes[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(2), kWasmStructTypeCode, 0,
              SIG_ENTRY_x(kExternRefCode)),
      SECTION(Function, ENTRY_COUNT(1), SIG_INDEX(1)),
      SECTION(Export, ENTRY_COUNT(1), ADD_COUNT('m'), kExternalFunction,
              FUNC_INDEX(0)),
      SECTION(Code, ENTRY_COUNT(1),
              ADD_COUNT(0, WASM_GC_OP(kExprStructNewDefault), 0,
                        WASM_GC_OP(kExprExternConvertAny), WASM_END))};
  Local<WasmModuleObject> module =
      WasmModuleObject::Compile(isolate(), kModuleBytes).ToLocalChecked();
  context->Global()->Set(context, NewString("mod"), module).Check();
  Local<Object> wasm_obj =
      RunJS("new WebAssembly.Instance(mod).exports.m()").As<Object>();
  Local<Object> sealed_js_obj =
      RunJS("Object.seal({__proto__: null})").As<Object>();
  Local<Object> other_proto = Object::New(isolate());

  {
    i::FlagScope<bool> flag_descriptors(&i::v8_flags.wasm_custom_descriptors,
                                        false);
    EXPECT_TRUE(wasm_obj->SetPrototype(context, Null(isolate())).IsNothing());
    EXPECT_TRUE(wasm_obj->SetPrototype(context, other_proto).IsNothing());
  }

  {
    i::FlagScope<bool> flag_descriptors(&i::v8_flags.wasm_custom_descriptors,
                                        true);
    for (Local<Object> obj : {wasm_obj, sealed_js_obj}) {
      EXPECT_EQ(Just(true), obj->SetPrototype(context, Null(isolate())));
      EXPECT_EQ(Just(true),
                obj->SetPrototype(context, Integer::New(isolate(), 42)));
      EXPECT_TRUE(obj->SetPrototype(context, other_proto).IsNothing());
    }
  }
}

TEST_F(ApiWasmTest, WasmStreamingAbortsWhenExecutionTerminating) {
  using namespace internal::wasm;  // NOLINT(build/namespaces)
  static std::shared_ptr<WasmStreaming> g_streaming;
  static bool g_start_called = false;
  auto capture_streaming = [](const FunctionCallbackInfo<Value>& info) {
    g_streaming = WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  };
  isolate()->SetWasmStreamingCallback(capture_streaming);

  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  // Module with an imported start function and no code section (so Finish()
  // transitions deterministically to FinishCompilation without background
  // compilation threads).
  constexpr uint8_t kNoCodeStartModuleBytes[] = {
      WASM_MODULE_HEADER, SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('f'),
              kExternalFunction, 0),
      SECTION(Start, 0)};

  // Module with an imported start function and a code section (to exercise the
  // async background compilation / prefix-cache foreground task path).
  constexpr uint8_t kCodeStartModuleBytes[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('f'),
              kExternalFunction, 0),
      SECTION(Function, ENTRY_COUNT(1), 0),
      SECTION(Start, 0),
      SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0, WASM_END))};

  // Pre-compile and root the module so GC cannot evict its weak entry from
  // NativeModuleCache before subtest 3.
  Global<WasmModuleObject> cached_module(
      isolate(), WasmModuleObject::Compile(isolate(), kCodeStartModuleBytes)
                     .ToLocalChecked());
  ASSERT_FALSE(cached_module.IsEmpty());

  Local<Object> imports = Object::New(isolate());
  Local<Object> mod = Object::New(isolate());
  Local<Function> on_start =
      Function::New(context, [](const FunctionCallbackInfo<Value>&) {
        g_start_called = true;
      }).ToLocalChecked();
  mod->Set(context, NewString("f"), on_start).Check();
  imports->Set(context, NewString("m"), mod).Check();
  context->Global()->Set(context, NewString("imports"), imports).Check();

  // 1. Deferred FinishCompilation foreground task pumped while execution is
  // terminating must abort cleanly without hitting
  // DCHECK(!isolate_->has_exception()) in InstanceBuilder::Build_Phase1 or
  // executing the Wasm start function.
  {
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->OnBytesReceived(kNoCodeStartModuleBytes,
                               arraysize(kNoCodeStartModuleBytes));
    {
      // Disallow JS so AsyncCompileJob::DoImmediatelyOrDefer posts a deferred
      // FinishCompilation foreground task.
      Isolate::DisallowJavascriptExecutionScope no_js(
          isolate(),
          Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
      streaming->Finish({});
    }
    EXPECT_TRUE(isolate()->HasPendingBackgroundTasks());

    {
      TryCatch try_catch(isolate());
      i_isolate()->TerminateExecution();
      ASSERT_TRUE(i_isolate()->is_execution_terminating());
      EmptyMessageQueues();
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  // 2. AsyncStreamingProcessor::OnFinishedStream called while execution is
  // already terminating must abort cleanly without finishing compilation or
  // running start.
  {
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->OnBytesReceived(kNoCodeStartModuleBytes,
                               arraysize(kNoCodeStartModuleBytes));
    {
      TryCatch try_catch(isolate());
      i_isolate()->TerminateExecution();
      ASSERT_TRUE(i_isolate()->is_execution_terminating());
      streaming->Finish({});
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  // 3a. Worker-thread-posted FinishCompilation (prefix-cache hit spawning
  // PrepareNativeModule on a background thread) pumped while execution is
  // terminating must abort cleanly in AsyncCompileJob::CompileStep::Run.
  {
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->OnBytesReceived(kCodeStartModuleBytes,
                               arraysize(kCodeStartModuleBytes));
    {
      // Disallow JS so even under --no-wasm-native-module-cache, a fast
      // background compile cannot run FinishCompilation synchronously before
      // TerminateExecution().
      Isolate::DisallowJavascriptExecutionScope no_js(
          isolate(),
          Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
      streaming->Finish({});
    }
    EXPECT_TRUE(isolate()->HasPendingBackgroundTasks());

    {
      TryCatch try_catch(isolate());
      i_isolate()->TerminateExecution();
      ASSERT_TRUE(i_isolate()->is_execution_terminating());
      while (isolate()->HasPendingBackgroundTasks()) {
        v8::platform::PumpMessageLoop(
            internal::V8::GetCurrentPlatform(), isolate(),
            platform::MessageLoopBehavior::kWaitForWork);
      }
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  // 3b. AsyncStreamingProcessor::OnFinishedStream called while execution is
  // already terminating on an uncached code-section module must abort via
  // OnAbort(), canceling active background validation/compilation and releasing
  // the NativeModuleCache entry without leaving pending tasks behind.
  {
    constexpr uint8_t kUncachedCodeStartModuleBytes[] = {
        WASM_MODULE_HEADER,
        SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
        SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('f'),
                kExternalFunction, 0),
        SECTION(Function, ENTRY_COUNT(1), 0),
        SECTION(Start, 0),
        SECTION(Code, ENTRY_COUNT(1),
                ADD_COUNT(0, WASM_NOP, WASM_NOP, WASM_END))};
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->OnBytesReceived(kUncachedCodeStartModuleBytes,
                               arraysize(kUncachedCodeStartModuleBytes));
    {
      TryCatch try_catch(isolate());
      i_isolate()->TerminateExecution();
      ASSERT_TRUE(i_isolate()->is_execution_terminating());
      streaming->Finish({});
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
    // Compiling the same wire bytes afterwards must succeed (proving
    // owns_cache_entry_ was released cleanly by OnAbort()).
    EXPECT_FALSE(
        WasmModuleObject::Compile(isolate(), kUncachedCodeStartModuleBytes)
            .IsEmpty());
  }

  // 4. Deferred Fail foreground task pumped while execution is terminating must
  // abort cleanly without rejecting the promise or clearing termination; also
  // verify OnFinishedStream abort when decoding already failed
  // (after_error == true).
  {
    constexpr uint8_t kInvalidModuleBytes[] = {0x00, 0x61, 0x73, 0x6d,
                                               0x99, 0x00, 0x00, 0x00};
    {
      g_streaming.reset();
      Local<v8::Promise> promise =
          Local<v8::Promise>::Cast(RunJS("WebAssembly.compileStreaming(null)"));
      std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
      ASSERT_TRUE(streaming);

      {
        Isolate::DisallowJavascriptExecutionScope no_js(
            isolate(),
            Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
        streaming->OnBytesReceived(kInvalidModuleBytes,
                                   arraysize(kInvalidModuleBytes));
        streaming->Finish({});
      }
      EXPECT_TRUE(isolate()->HasPendingBackgroundTasks());

      {
        TryCatch try_catch(isolate());
        i_isolate()->TerminateExecution();
        ASSERT_TRUE(i_isolate()->is_execution_terminating());
        EmptyMessageQueues();
        EXPECT_EQ(v8::Promise::kPending, promise->State());
        EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
        EXPECT_TRUE(i_isolate()->is_execution_terminating());
        EXPECT_TRUE(try_catch.HasTerminated());
      }
      EXPECT_FALSE(i_isolate()->has_exception());
    }

    {
      g_streaming.reset();
      Local<v8::Promise> promise =
          Local<v8::Promise>::Cast(RunJS("WebAssembly.compileStreaming(null)"));
      std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
      ASSERT_TRUE(streaming);

      streaming->OnBytesReceived(kInvalidModuleBytes,
                                 arraysize(kInvalidModuleBytes));
      {
        TryCatch try_catch(isolate());
        i_isolate()->TerminateExecution();
        ASSERT_TRUE(i_isolate()->is_execution_terminating());
        streaming->Finish({});
        EXPECT_EQ(v8::Promise::kPending, promise->State());
        EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
        EXPECT_TRUE(i_isolate()->is_execution_terminating());
        EXPECT_TRUE(try_catch.HasTerminated());
      }
      EXPECT_FALSE(i_isolate()->has_exception());
    }
  }

  // 5. WasmStreaming::Abort with a non-empty exception while execution is
  // terminating must leave the promise pending without hitting
  // DCHECK(!is_execution_terminating()) in Promise::Resolver::Reject.
  {
    g_streaming.reset();
    Local<v8::Promise> promise =
        Local<v8::Promise>::Cast(RunJS("WebAssembly.compileStreaming(null)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);
    Local<Value> abort_reason = Object::New(isolate());

    {
      TryCatch try_catch(isolate());
      i_isolate()->TerminateExecution();
      ASSERT_TRUE(i_isolate()->is_execution_terminating());
      streaming->Abort(abort_reason);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  // 6. Cached streaming compilation with termination on a cache miss (no
  // SetCachedCompiledModuleBytes call): falling through to OnBytesReceived() +
  // OnFinishedStream() must abort cleanly in
  // AsyncStreamingProcessor::OnFinishedStream without spawning
  // PrepareNativeModule or running start.
  {
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->SetHasCompiledModuleBytes();
    streaming->OnBytesReceived(kCodeStartModuleBytes,
                               arraysize(kCodeStartModuleBytes));
    {
      TryCatch try_catch(isolate());
      streaming->Finish([&](WasmStreaming::ModuleCachingInterface&) {
        i_isolate()->TerminateExecution();
        ASSERT_TRUE(i_isolate()->is_execution_terminating());
      });
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  g_streaming.reset();
}

#ifdef V8_ENABLE_TURBOFAN
// Separate TEST_F so FLAG_VALUE_SCOPE modifies v8_flags before any Wasm
// compilation starts in this process (avoiding TSan data races with detached
// background compile jobs from earlier subtests).
TEST_F(ApiWasmTest, WasmStreamingDeserializeAbortsWhenExecutionTerminating) {
  if (i::v8_flags.wasm_jitless || i::v8_flags.jitless ||
      i::v8_flags.liftoff_only) {
    return;
  }
  FLAG_VALUE_SCOPE(liftoff, false);
  FLAG_VALUE_SCOPE(wasm_lazy_compilation, false);

  using namespace internal::wasm;  // NOLINT(build/namespaces)
  static std::shared_ptr<WasmStreaming> g_streaming;
  static bool g_start_called = false;
  auto capture_streaming = [](const FunctionCallbackInfo<Value>& info) {
    g_streaming = WasmStreaming::Unpack(info.GetIsolate(), info.Data());
  };
  isolate()->SetWasmStreamingCallback(capture_streaming);

  HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);

  constexpr uint8_t kCachedStartModuleBytes[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1), SIG_ENTRY_v_v),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('f'),
              kExternalFunction, 0),
      SECTION(Function, ENTRY_COUNT(1), 0),
      SECTION(Start, 0),
      SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0, WASM_NOP, WASM_END))};
  OwnedBuffer serialized =
      WasmModuleObject::Compile(isolate(), kCachedStartModuleBytes)
          .ToLocalChecked()
          ->GetCompiledModule()
          .Serialize();
  ASSERT_FALSE(serialized.empty());

  Local<Object> imports = Object::New(isolate());
  Local<Object> mod = Object::New(isolate());
  Local<Function> on_start =
      Function::New(context, [](const FunctionCallbackInfo<Value>&) {
        g_start_called = true;
      }).ToLocalChecked();
  mod->Set(context, NewString("f"), on_start).Check();
  imports->Set(context, NewString("m"), mod).Check();
  context->Global()->Set(context, NewString("imports"), imports).Check();

  // Test both already-terminating before Finish() and termination triggered
  // inside caching_callback before SetCachedCompiledModuleBytes().
  for (bool terminate_before_finish : {true, false}) {
    g_start_called = false;
    g_streaming.reset();
    Local<v8::Promise> promise = Local<v8::Promise>::Cast(
        RunJS("WebAssembly.instantiateStreaming(null, imports)"));
    std::shared_ptr<WasmStreaming> streaming = std::move(g_streaming);
    ASSERT_TRUE(streaming);

    streaming->SetHasCompiledModuleBytes();
    streaming->OnBytesReceived(kCachedStartModuleBytes,
                               arraysize(kCachedStartModuleBytes));
    {
      TryCatch try_catch(isolate());
      if (terminate_before_finish) {
        i_isolate()->TerminateExecution();
        ASSERT_TRUE(i_isolate()->is_execution_terminating());
      }
      static bool g_codegen_callback_called = false;
      g_codegen_callback_called = false;
      isolate()->SetAllowWasmCodeGenerationCallback(
          [](Local<Context>, Local<String>) {
            g_codegen_callback_called = true;
            return true;
          });
      bool set_cached_ok = false;
      streaming->Finish(
          [&](WasmStreaming::ModuleCachingInterface& caching_interface) {
            if (!terminate_before_finish) {
              i_isolate()->TerminateExecution();
              ASSERT_TRUE(i_isolate()->is_execution_terminating());
            }
            set_cached_ok = caching_interface.SetCachedCompiledModuleBytes(
                serialized.as_span());
          });
      isolate()->SetAllowWasmCodeGenerationCallback(nullptr);
      EXPECT_FALSE(g_codegen_callback_called);
      EXPECT_TRUE(set_cached_ok);
      EXPECT_FALSE(g_start_called);
      EXPECT_EQ(v8::Promise::kPending, promise->State());
      EXPECT_FALSE(isolate()->HasPendingBackgroundTasks());
      EXPECT_TRUE(i_isolate()->is_execution_terminating());
      EXPECT_TRUE(try_catch.HasTerminated());
    }
    EXPECT_FALSE(i_isolate()->has_exception());
  }

  g_streaming.reset();
}
#endif  // V8_ENABLE_TURBOFAN

}  // namespace v8
