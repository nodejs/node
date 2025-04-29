// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/fuzzing/random-module-generation.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/zone/zone.h"
#include "test/fuzzer/wasm/interpreter/interpreter-fuzzer-common.h"

namespace v8::internal::wasm::fuzzing {

bool GenerateModule(Isolate* isolate, Zone* zone,
                    base::Vector<const uint8_t> data, ZoneBuffer* buffer) {
  ValueType kIntTypes4[4];
  FunctionSig sig_i_iii{1, 3, kIntTypes4};
  WasmModuleBuilder builder(zone);
  WasmFunctionBuilder* f = builder.AddFunction(&sig_i_iii);
  f->EmitCode(data.begin(), static_cast<uint32_t>(data.size()));
  f->Emit(kExprEnd);
  builder.AddExport(base::CStrVector("main"), f);

  builder.AddMemory(0, 32);
  builder.WriteTo(buffer);
  return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return LLVMFuzzerTestOneInputCommon(data, size, GenerateModule);
}

}  // namespace v8::internal::wasm::fuzzing
