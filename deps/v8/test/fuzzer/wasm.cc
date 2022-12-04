// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "src/execution/isolate-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace i = v8::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // We reduce the maximum memory size and table size of WebAssembly instances
  // to avoid OOMs in the fuzzer.
  i::v8_flags.wasm_max_mem_pages = 32;
  i::v8_flags.wasm_max_table_size = 100;

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  i::wasm::fuzzer::OneTimeEnableStagedWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  i::wasm::testing::SetupIsolateForWasmModule(i_isolate);
  i::wasm::ModuleWireBytes wire_bytes(data, data + size);

  i::HandleScope scope(i_isolate);
  i::wasm::ErrorThrower thrower(i_isolate, "wasm fuzzer");
  i::Handle<i::WasmModuleObject> module_object;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  bool compiles =
      i::wasm::GetWasmEngine()
          ->SyncCompile(i_isolate, enabled_features, &thrower, wire_bytes)
          .ToHandle(&module_object);

  if (i::v8_flags.wasm_fuzzer_gen_test) {
    i::wasm::fuzzer::GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  if (compiles) {
    i::wasm::fuzzer::InterpretAndExecuteModule(i_isolate, module_object);
  }

  // Pump the message loop and run micro tasks, e.g. GC finalization tasks.
  support->PumpMessageLoop(v8::platform::MessageLoopBehavior::kDoNotWait);
  isolate->PerformMicrotaskCheckpoint();
  return 0;
}
