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

using namespace v8::internal;
using namespace v8::internal::wasm;
using namespace v8::internal::wasm::fuzzer;

class WasmCodeFuzzer : public WasmExecutionFuzzer {
  virtual bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmVal[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    TestSignatures sigs;
    WasmModuleBuilder builder(zone);
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    f->EmitCode(data, static_cast<uint32_t>(size));
    uint8_t end_opcode = kExprEnd;
    f->EmitCode(&end_opcode, 1);
    builder.AddExport(CStrVector("main"), f);

    builder.WriteTo(buffer);
    num_args = 3;
    interpreter_args.reset(new WasmVal[3]{WasmVal(1), WasmVal(2), WasmVal(3)});

    compiler_args.reset(new Handle<Object>[3]{
        handle(Smi::FromInt(1), isolate), handle(Smi::FromInt(1), isolate),
        handle(Smi::FromInt(1), isolate)});
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return WasmCodeFuzzer().FuzzWasmModule(data, size);
}
