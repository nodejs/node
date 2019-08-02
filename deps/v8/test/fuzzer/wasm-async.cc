// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/api/api.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8 {
namespace internal {
class WasmModuleObject;

namespace wasm {
namespace fuzzer {

class AsyncFuzzerResolver : public i::wasm::CompilationResultResolver {
 public:
  AsyncFuzzerResolver(i::Isolate* isolate, bool* done)
      : isolate_(isolate), done_(done) {}

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> module) override {
    *done_ = true;
    InterpretAndExecuteModule(isolate_, module);
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    *done_ = true;
  }

 private:
  i::Isolate* isolate_;
  bool* done_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FlagScope<bool> turn_on_async_compile(
      &v8::internal::FLAG_wasm_async_compilation, true);
  FlagScope<uint32_t> max_mem_flag_scope(&v8::internal::FLAG_wasm_max_mem_pages,
                                         32);
  FlagScope<uint32_t> max_table_size_scope(
      &v8::internal::FLAG_wasm_max_table_size, 100);
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  i::HandleScope internal_scope(i_isolate);
  v8::Context::Scope context_scope(support->GetContext());
  TryCatch try_catch(isolate);
  testing::SetupIsolateForWasmModule(i_isolate);

  bool done = false;
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  constexpr const char* kAPIMethodName = "WasmAsyncFuzzer.compile";
  i_isolate->wasm_engine()->AsyncCompile(
      i_isolate, enabled_features,
      std::make_shared<AsyncFuzzerResolver>(i_isolate, &done),
      ModuleWireBytes(data, data + size), false, kAPIMethodName);

  // Wait for the promise to resolve.
  while (!done) {
    support->PumpMessageLoop(platform::MessageLoopBehavior::kWaitForWork);
    isolate->RunMicrotasks();
  }
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
