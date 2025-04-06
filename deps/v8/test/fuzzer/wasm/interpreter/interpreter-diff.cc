// Copyright 2024 the V8 project authors. All rights reserved.
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
#include "test/fuzzer/wasm/fuzzer-common.h"
#include "test/fuzzer/wasm/interpreter/interpreter-fuzzer-common.h"

namespace v8::internal::wasm::fuzzing {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // We reduce the maximum memory size and table size of WebAssembly instances
  // to avoid OOMs in the fuzzer.
  v8_flags.wasm_max_mem_pages = 32;
  v8_flags.wasm_max_table_size = 100;

  // We need to reset jitless flags set by a previous run.
  v8::internal::v8_flags.jitless = false;
  v8::internal::v8_flags.wasm_jitless = false;
  FlagList::EnforceFlagImplications();

  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // We also need to clear JsToWasm wrappers from a previous run.
  ClearJsToWasmWrappersForTesting(i_isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_exception()) {
    i_isolate->clear_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // Drumbrake may not support some experimental features yet.
  // EnableExperimentalWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  testing::SetupIsolateForWasmModule(i_isolate);
  ModuleWireBytes wire_bytes(data, data + size);

  HandleScope scope(i_isolate);
  ErrorThrower thrower(i_isolate, "wasm interpreter diff fuzzer");
  DirectHandle<WasmModuleObject> module_object;
  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  bool compiles =
      GetWasmEngine()
          ->SyncCompile(i_isolate, enabled_features,
                        fuzzing::CompileTimeImportsForFuzzing(), &thrower,
                        v8::base::OwnedCopyOf(wire_bytes.module_bytes()))
          .ToHandle(&module_object);

  if (v8_flags.wasm_fuzzer_gen_test) {
    fuzzing::GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  int module_result = -1;
  if (compiles) {
    module_result = fuzzing::ExecuteAgainstReference(
        i_isolate, module_object,
        fuzzing::kDefaultMaxFuzzerExecutedInstructions,
        /* edge_is_wasm_jitless */ true);
  }

  // Pump the message loop and run micro tasks, e.g. GC finalization tasks.
  support->PumpMessageLoop(v8::platform::MessageLoopBehavior::kDoNotWait);
  isolate->PerformMicrotaskCheckpoint();
  return module_result;
}

}  // namespace v8::internal::wasm::fuzzing
