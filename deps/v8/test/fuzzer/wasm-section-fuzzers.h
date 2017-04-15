// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_SECTION_FUZZERS_H_
#define WASM_SECTION_FUZZERS_H_

#include <stddef.h>
#include <stdint.h>

#include "src/wasm/wasm-module.h"

int fuzz_wasm_section(v8::internal::wasm::WasmSectionCode section,
                      const uint8_t* data, size_t size);

#endif  // WASM_SECTION_FUZZERS_H_
