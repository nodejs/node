// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "test/common/wasm/test-signatures.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

class WasmCodeFuzzer : public WasmExecutionFuzzer {
  bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmValue[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    TestSignatures sigs;
    WasmModuleBuilder builder(zone);
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    f->EmitCode(data, static_cast<uint32_t>(size));
    uint8_t end_opcode = kExprEnd;
    f->EmitCode(&end_opcode, 1);
    builder.AddExport(CStrVector("main"), f);

    builder.SetMaxMemorySize(32);
    builder.WriteTo(buffer);
    num_args = 3;
    interpreter_args.reset(
        new WasmValue[3]{WasmValue(1), WasmValue(2), WasmValue(3)});

    compiler_args.reset(new Handle<Object>[3]{
        handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(2), isolate),
        handle(Smi::FromInt(3), isolate)});
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return WasmCodeFuzzer().FuzzWasmModule(data, size);
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
