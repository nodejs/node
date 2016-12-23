// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-section-fuzzers.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return fuzz_wasm_section(v8::internal::wasm::kTypeSectionCode, data, size);
}
