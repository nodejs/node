// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/utils.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm-fuzzer-common.h"

#define MAX_NUM_FUNCTIONS 3
#define MAX_NUM_PARAMS 3

using namespace v8::internal;
using namespace v8::internal::wasm;
using namespace v8::internal::wasm::fuzzer;

class WasmCallFuzzer : public WasmExecutionFuzzer {
  template <typename V>
  static inline V read_value(const uint8_t** data, size_t* size, bool* ok) {
    // The status flag {ok} checks that the decoding up until now was okay, and
    // that a value of type V can be read without problems.
    *ok &= (*size > sizeof(V));
    if (!(*ok)) return 0;
    V result = v8::internal::ReadLittleEndianValue<V>(*data);
    *data += sizeof(V);
    *size -= sizeof(V);
    return result;
  }

  static void add_argument(
      v8::internal::Isolate* isolate, ValueType type, WasmVal* interpreter_args,
      v8::internal::Handle<v8::internal::Object>* compiler_args, int* argc,
      const uint8_t** data, size_t* size, bool* ok) {
    if (!(*ok)) return;
    switch (type) {
      case kWasmF32: {
        float value = read_value<float>(data, size, ok);
        interpreter_args[*argc] = WasmVal(value);
        compiler_args[*argc] =
            isolate->factory()->NewNumber(static_cast<double>(value));
        break;
      }
      case kWasmF64: {
        double value = read_value<double>(data, size, ok);
        interpreter_args[*argc] = WasmVal(value);
        compiler_args[*argc] = isolate->factory()->NewNumber(value);
        break;
      }
      case kWasmI32: {
        int32_t value = read_value<int32_t>(data, size, ok);
        interpreter_args[*argc] = WasmVal(value);
        compiler_args[*argc] =
            isolate->factory()->NewNumber(static_cast<double>(value));
        break;
      }
      default:
        UNREACHABLE();
    }
    (*argc)++;
  }

  virtual bool GenerateModule(
      Isolate* isolate, Zone* zone, const uint8_t* data, size_t size,
      ZoneBuffer& buffer, int32_t& num_args,
      std::unique_ptr<WasmVal[]>& interpreter_args,
      std::unique_ptr<Handle<Object>[]>& compiler_args) override {
    bool ok = true;
    uint8_t num_functions =
        (read_value<uint8_t>(&data, &size, &ok) % MAX_NUM_FUNCTIONS) + 1;

    ValueType types[] = {kWasmF32, kWasmF64, kWasmI32, kWasmI64};

    interpreter_args.reset(new WasmVal[3]);
    compiler_args.reset(new Handle<Object>[3]);

    WasmModuleBuilder builder(zone);
    for (int fun = 0; fun < num_functions; fun++) {
      size_t num_params = static_cast<size_t>(
          (read_value<uint8_t>(&data, &size, &ok) % MAX_NUM_PARAMS) + 1);
      FunctionSig::Builder sig_builder(zone, 1, num_params);
      sig_builder.AddReturn(kWasmI32);
      for (size_t param = 0; param < num_params; param++) {
        // The main function cannot handle int64 parameters.
        ValueType param_type = types[(read_value<uint8_t>(&data, &size, &ok) %
                                      (arraysize(types) - (fun == 0 ? 1 : 0)))];
        sig_builder.AddParam(param_type);
        if (fun == 0) {
          add_argument(isolate, param_type, interpreter_args.get(),
                       compiler_args.get(), &num_args, &data, &size, &ok);
        }
      }
      v8::internal::wasm::WasmFunctionBuilder* f =
          builder.AddFunction(sig_builder.Build());
      uint32_t code_size = static_cast<uint32_t>(size / num_functions);
      f->EmitCode(data, code_size);
      uint8_t end_opcode = kExprEnd;
      f->EmitCode(&end_opcode, 1);
      data += code_size;
      size -= code_size;
      if (fun == 0) {
        builder.AddExport(v8::internal::CStrVector("main"), f);
      }
    }

    builder.WriteTo(buffer);

    if (!ok) {
      // The input data was too short.
      return 0;
    }
    return true;
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return WasmCallFuzzer().FuzzWasmModule(data, size);
}
