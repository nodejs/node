// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_FAST_INTERPRETER_FUZZER_COMMON_H_
#define WASM_FAST_INTERPRETER_FUZZER_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <random>

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module-builder.h"

namespace v8::internal::wasm::fuzzing {

std::mt19937_64 MersenneTwister(const char* data, size_t size);

// Instantiates and interprets exported functions within module_object if
// possible
int FastInterpretAndExecuteModule(Isolate* isolate,
                                  DirectHandle<WasmModuleObject> module_object,
                                  std::mt19937_64 g);

void InitializeDrumbrakeForFuzzing();

typedef std::function<bool(Isolate*, Zone*, v8::base::Vector<const uint8_t>,
                           ZoneBuffer*)>
    GenerateModuleFunc;

int LLVMFuzzerTestOneInputCommon(const uint8_t* data, size_t size,
                                 GenerateModuleFunc generate_module);

}  // namespace v8::internal::wasm::fuzzing

#endif  // WASM_FAST_INTERPRETER_FUZZER_COMMON_H_
