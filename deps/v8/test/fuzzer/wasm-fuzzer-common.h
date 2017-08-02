// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_SECTION_FUZZERS_H_
#define WASM_SECTION_FUZZERS_H_

#include <stddef.h>
#include <stdint.h>

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

int FuzzWasmSection(v8::internal::wasm::SectionCode section,
                    const uint8_t* data, size_t size);

class WasmExecutionFuzzer {
 public:
  virtual ~WasmExecutionFuzzer() {}
  int FuzzWasmModule(const uint8_t* data, size_t size);

 protected:
  virtual bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmVal[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) = 0;
};

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif  // WASM_SECTION_FUZZERS_H_
