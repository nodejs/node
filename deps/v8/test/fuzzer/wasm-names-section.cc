// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

using namespace v8::internal::wasm::fuzzer;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // TODO(titzer): Names section requires a preceding function section.
  return FuzzWasmSection(v8::internal::wasm::kNameSectionCode, data, size);
}
