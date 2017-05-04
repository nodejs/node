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
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  unsigned int max_mem_flag_value = v8::internal::FLAG_wasm_max_mem_pages;
  unsigned int max_table_flag_value = v8::internal::FLAG_wasm_max_table_size;
  v8::internal::FLAG_wasm_max_mem_pages = 32;
  v8::internal::FLAG_wasm_max_table_size = 100;
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);
  v8::internal::wasm::testing::SetupIsolateForWasmModule(i_isolate);
  v8::internal::wasm::testing::CompileAndRunWasmModule(
      i_isolate, data, data + size, v8::internal::wasm::kWasmOrigin);
  v8::internal::FLAG_wasm_max_mem_pages = max_mem_flag_value;
  v8::internal::FLAG_wasm_max_table_size = max_table_flag_value;
  return 0;
}
