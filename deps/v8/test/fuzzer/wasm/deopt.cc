// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "src/execution/isolate.h"
#include "test/common/wasm/deopt-fuzzer-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/fuzzer/fuzzer-support.h"

namespace v8::internal::wasm::fuzzing {

V8_SYMBOL_USED extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  // shared_heap and shared_strings are needed for
  // shared-everything-threads fuzzing.
  i::v8_flags.shared_heap.Set(true);
  i::v8_flags.shared_strings.Set(true);
  i::v8_flags.wasm_shared.Set(true);

  v8_fuzzer::FuzzerSupport::InitializeFuzzerSupport(argc, argv);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // Strictly enforce the input size limit as in fuzzer-common.h.
  if (size > kMaxFuzzerInputSize) return 0;

  v8::Isolate::Scope isolate_scope(isolate);

  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  return FuzzWasmDeopt({data, size}, isolate);
}

}  // namespace v8::internal::wasm::fuzzing
