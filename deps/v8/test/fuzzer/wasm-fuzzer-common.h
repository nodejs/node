// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_FUZZER_COMMON_H_
#define WASM_FUZZER_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

int FuzzWasmSection(SectionCode section, const uint8_t* data, size_t size);

// First instantiates and interprets the "main" function within module_object if
// possible. If the interpretation finishes within kMaxSteps steps,
// module_object is instantiated again and the compiled "main" function is
// executed.
void InterpretAndExecuteModule(Isolate* isolate,
                               Handle<WasmModuleObject> module_object);

class WasmExecutionFuzzer {
 public:
  virtual ~WasmExecutionFuzzer() {}
  int FuzzWasmModule(const uint8_t* data, size_t size,
                     bool require_valid = false);

 protected:
  virtual bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmValue[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) = 0;
};

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif  // WASM_FUZZER_COMMON_H_
