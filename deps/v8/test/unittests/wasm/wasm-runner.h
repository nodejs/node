// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_WASM_WASM_RUNNER_H_
#define V8_UNITTESTS_WASM_WASM_RUNNER_H_

#include "src/api/api-inl.h"
#include "src/sandbox/hardware-support.h"
#include "src/trap-handler/trap-handler.h"
#include "test/common/wasm/wasm-run-utils.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm {

class WasmTrapHandlerInitializer {
 protected:
  WasmTrapHandlerInitializer() {
#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
    // SandboxHardwareSupport::InitializeBeforeThreadCreation() has already run
    // in main(); enable the trap handler once per process before any call to
    // IsTrapHandlerEnabled() (such as during Isolate::New).
    if (HardwareSandboxingDisabledOrSupportsSignalDeliveryInSandbox() &&
        trap_handler::g_can_enable_trap_handler.load(
            std::memory_order_relaxed)) {
      constexpr bool kUseDefaultTrapHandler = true;
      CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultTrapHandler));
    }
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
  }
};

template <typename TMixin>
class WithWasmTrapHandlerMixin : private WasmTrapHandlerInitializer,
                                 public TMixin {};

using WasmUninitializedTestBase = WithWasmTrapHandlerMixin<TestWithPlatform>;

class WasmExecutionTestBase : public WithIsolateScopeMixin<
                                  WithIsolateMixin<WasmUninitializedTestBase>> {
 public:
  ~WasmExecutionTestBase() override {
    while (v8_isolate()->InContext()) {
      v8_isolate()->GetCurrentContext()->Exit();
    }
  }

  static Isolate* InitIsolateOnce() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    CHECK_NOT_NULL(isolate);
    if (!isolate->InContext()) {
      v8::Context::New(isolate)->Enter();
    }
    return reinterpret_cast<Isolate*>(isolate);
  }

  static Isolate* current_i_isolate() {
    return reinterpret_cast<Isolate*>(v8::Isolate::GetCurrent());
  }

  static v8::Isolate* current_isolate() { return v8::Isolate::GetCurrent(); }

  static Heap* current_heap() { return current_i_isolate()->heap(); }

  static v8::base::RandomNumberGenerator* random_number_generator() {
    return InitIsolateOnce()->random_number_generator();
  }
};

inline v8::Local<v8::Value> CompileRun(const char* source) {
  WasmExecutionTestBase::InitIsolateOnce();
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
          .ToLocalChecked();
  v8::Local<v8::Value> result;
  if (script->Run(context).ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Value>();
}

template <typename ReturnType, typename... ParamTypes>
class WasmRunner : public CommonWasmRunner<ReturnType, ParamTypes...> {
 public:
  // If the caller does not provide an isolate, use the current test isolate.
  template <typename... Args>
    requires(... && !std::is_same_v<Args, Isolate*>)
  explicit WasmRunner(TestExecutionTier execution_tier, Args&&... args)
      : CommonWasmRunner<ReturnType, ParamTypes...>(
            WasmExecutionTestBase::InitIsolateOnce(), execution_tier,
            std::forward<Args&&>(args)...) {}

  // If an isolate is provided, use that.
  template <typename... Args>
    requires(... && !std::is_same_v<Args, Isolate*>)
  explicit WasmRunner(Isolate* isolate, Args&&... args)
      : CommonWasmRunner<ReturnType, ParamTypes...>(
            isolate, std::forward<Args&&>(args)...) {}
};

#if V8_ENABLE_DRUMBRAKE
#define TEST_IF_DRUMBRAKE(name)                             \
  TEST_F(WASM_EXEC_TEST_SUITE, RunWasmInterpreter_##name) { \
    FLAG_SCOPE(wasm_jitless);                               \
    WasmInterpreterThread::Initialize();                    \
    RunWasm_##name(TestExecutionTier::kInterpreter);        \
    WasmInterpreterThread::Terminate();                     \
  }
#else
#define TEST_IF_DRUMBRAKE(name)
#endif  // V8_ENABLE_DRUMBRAKE

#define WASM_EXEC_TEST(name)                             \
  void RunWasm_##name(TestExecutionTier execution_tier); \
  TEST_F(WASM_EXEC_TEST_SUITE, RunWasmTurbofan_##name) { \
    RunWasm_##name(TestExecutionTier::kTurbofan);        \
  }                                                      \
  TEST_F(WASM_EXEC_TEST_SUITE, RunWasmLiftoff_##name) {  \
    RunWasm_##name(TestExecutionTier::kLiftoff);         \
  }                                                      \
  TEST_IF_DRUMBRAKE(name)                                \
  void RunWasm_##name(TestExecutionTier execution_tier)

#define UNINITIALIZED_WASM_EXEC_TEST(name)                             \
  void RunWasm_##name(TestExecutionTier execution_tier);               \
  TEST_F(WASM_UNINITIALIZED_EXEC_TEST_SUITE, RunWasmTurbofan_##name) { \
    RunWasm_##name(TestExecutionTier::kTurbofan);                      \
  }                                                                    \
  TEST_F(WASM_UNINITIALIZED_EXEC_TEST_SUITE, RunWasmLiftoff_##name) {  \
    RunWasm_##name(TestExecutionTier::kLiftoff);                       \
  }                                                                    \
  void RunWasm_##name(TestExecutionTier execution_tier)

#define WASM_COMPILED_EXEC_TEST(name)                    \
  void RunWasm_##name(TestExecutionTier execution_tier); \
  TEST_F(WASM_EXEC_TEST_SUITE, RunWasmTurbofan_##name) { \
    RunWasm_##name(TestExecutionTier::kTurbofan);        \
  }                                                      \
  TEST_F(WASM_EXEC_TEST_SUITE, RunWasmLiftoff_##name) {  \
    RunWasm_##name(TestExecutionTier::kLiftoff);         \
  }                                                      \
  void RunWasm_##name(TestExecutionTier execution_tier)

}  // namespace v8::internal::wasm

#endif  // V8_UNITTESTS_WASM_WASM_RUNNER_H_
