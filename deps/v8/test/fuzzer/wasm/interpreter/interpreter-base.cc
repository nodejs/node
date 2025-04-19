// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/fuzzing/random-module-generation.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/zone/zone.h"
#include "test/fuzzer/wasm/interpreter/interpreter-fuzzer-common.h"

namespace v8::internal::wasm::fuzzing {

bool GenerateModule(i::Isolate* isolate, i::Zone* zone,
                    v8::base::Vector<const uint8_t> data,
                    i::wasm::ZoneBuffer* buffer) {
  v8::base::Vector<const uint8_t> wire_bytes =
      fuzzing::GenerateRandomWasmModule(
          zone, fuzzing::WasmModuleGenerationOptions::MVP(), data);
  if (wire_bytes.empty()) return false;
  buffer->write(wire_bytes.data(), wire_bytes.size());
  return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return LLVMFuzzerTestOneInputCommon(data, size, GenerateModule);
}

}  // namespace v8::internal::wasm::fuzzing
