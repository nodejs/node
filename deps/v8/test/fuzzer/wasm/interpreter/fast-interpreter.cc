// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-isolate.h"
#include "src/execution/isolate-inl.h"
#include "src/wasm/fuzzing/random-module-generation.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/zone/zone.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm/fuzzer-common.h"
#include "test/fuzzer/wasm/interpreter/interpreter-fuzzer-common.h"

namespace v8::internal::wasm::fuzzing {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto rnd_gen = MersenneTwister(reinterpret_cast<const char*>(data), size);

  InitializeDrumbrakeForFuzzing();

  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  v8::internal::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

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
  ErrorThrower thrower(i_isolate, "wasm fuzzer");
  DirectHandle<i::WasmModuleObject> module_object;
  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  bool compiles =
      GetWasmEngine()
          ->SyncCompile(i_isolate, enabled_features, CompileTimeImports{},
                        &thrower,
                        v8::base::OwnedCopyOf(wire_bytes.module_bytes()))
          .ToHandle(&module_object);
  // TODO(338326645): Add similar GenerateTestCase code for wasm fast
  // interpreter.
  // if (v8_flags.wasm_fuzzer_gen_test) {
  //   GenerateTestCase(i_isolate, wire_bytes, compiles);
  // }
  if (compiles) {
    FastInterpretAndExecuteModule(i_isolate, module_object, rnd_gen);
  }
  thrower.Reset();

  // Pump the message loop and run micro tasks, e.g. GC finalization tasks.
  support->PumpMessageLoop(v8::platform::MessageLoopBehavior::kDoNotWait);
  isolate->PerformMicrotaskCheckpoint();
  return 0;
}

}  // namespace v8::internal::wasm::fuzzing
