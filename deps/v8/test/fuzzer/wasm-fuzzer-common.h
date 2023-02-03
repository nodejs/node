// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_FUZZER_COMMON_H_
#define WASM_FUZZER_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module-builder.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

// First instantiates and interprets the "main" function within module_object if
// possible. If the interpretation finishes within kMaxSteps steps,
// module_object is instantiated again and the compiled "main" function is
// executed.
void InterpretAndExecuteModule(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    Handle<WasmModuleObject> module_ref = Handle<WasmModuleObject>::null(),
    int32_t* max_steps = nullptr, int32_t* nondeterminism = nullptr);

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles);

// On the first call, enables all staged wasm features. All subsequent calls are
// no-ops. This avoids race conditions with threads reading the flags. Fuzzers
// are executed in their own process anyway, so this should not interfere with
// anything.
void OneTimeEnableStagedWasmFeatures(v8::Isolate* isolate);

class WasmExecutionFuzzer {
 public:
  virtual ~WasmExecutionFuzzer() = default;
  void FuzzWasmModule(base::Vector<const uint8_t> data,
                      bool require_valid = false);

  virtual size_t max_input_size() const { return 512; }

 protected:
  virtual bool GenerateModule(Isolate* isolate, Zone* zone,
                              base::Vector<const uint8_t> data,
                              ZoneBuffer* buffer,
                              bool liftoff_as_reference) = 0;
};

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif  // WASM_FUZZER_COMMON_H_
