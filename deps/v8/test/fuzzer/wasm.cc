// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace i = v8::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  i::FlagScope<uint32_t> max_mem_flag_scope(&i::FLAG_wasm_max_mem_pages, 32);
  i::FlagScope<uint32_t> max_table_size_scope(&i::FLAG_wasm_max_table_size,
                                              100);
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);
  i::wasm::testing::SetupIsolateForWasmModule(i_isolate);

  i::HandleScope scope(i_isolate);
  i::wasm::ErrorThrower thrower(i_isolate, "wasm fuzzer");
  i::MaybeHandle<i::WasmModuleObject> maybe_object = SyncCompile(
      i_isolate, &thrower, i::wasm::ModuleWireBytes(data, data + size));
  i::Handle<i::WasmModuleObject> module_object;
  if (maybe_object.ToHandle(&module_object)) {
    i::wasm::fuzzer::InterpretAndExecuteModule(i_isolate, module_object);
  }
  return 0;
}
