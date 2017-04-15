// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/utils.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

#define WASM_CODE_FUZZER_HASH_SEED 83
#define MAX_NUM_FUNCTIONS 3
#define MAX_NUM_PARAMS 3

using namespace v8::internal::wasm;

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
    v8::internal::Handle<v8::internal::Object>* compiled_args, int* argc,
    const uint8_t** data, size_t* size, bool* ok) {
  if (!(*ok)) return;
  switch (type) {
    case kWasmF32: {
      float value = read_value<float>(data, size, ok);
      interpreter_args[*argc] = WasmVal(value);
      compiled_args[*argc] =
          isolate->factory()->NewNumber(static_cast<double>(value));
      break;
    }
    case kWasmF64: {
      double value = read_value<double>(data, size, ok);
      interpreter_args[*argc] = WasmVal(value);
      compiled_args[*argc] = isolate->factory()->NewNumber(value);
      break;
    }
    case kWasmI32: {
      int32_t value = read_value<int32_t>(data, size, ok);
      interpreter_args[*argc] = WasmVal(value);
      compiled_args[*argc] =
          isolate->factory()->NewNumber(static_cast<double>(value));
      break;
    }
    default:
      UNREACHABLE();
  }
  (*argc)++;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  v8::internal::AccountingAllocator allocator;
  v8::internal::Zone zone(&allocator, ZONE_NAME);

  bool ok = true;
  uint8_t num_functions =
      (read_value<uint8_t>(&data, &size, &ok) % MAX_NUM_FUNCTIONS) + 1;

  ValueType types[] = {kWasmF32, kWasmF64, kWasmI32, kWasmI64};
  WasmVal interpreter_args[3];
  v8::internal::Handle<v8::internal::Object> compiled_args[3];
  int argc = 0;

  WasmModuleBuilder builder(&zone);
  for (int fun = 0; fun < num_functions; fun++) {
    size_t num_params = static_cast<size_t>(
        (read_value<uint8_t>(&data, &size, &ok) % MAX_NUM_PARAMS) + 1);
    FunctionSig::Builder sig_builder(&zone, 1, num_params);
    sig_builder.AddReturn(kWasmI32);
    for (size_t param = 0; param < num_params; param++) {
      // The main function cannot handle int64 parameters.
      ValueType param_type = types[(read_value<uint8_t>(&data, &size, &ok) %
                                    (arraysize(types) - (fun == 0 ? 1 : 0)))];
      sig_builder.AddParam(param_type);
      if (fun == 0) {
        add_argument(i_isolate, param_type, interpreter_args, compiled_args,
                     &argc, &data, &size, &ok);
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
      f->ExportAs(v8::internal::CStrVector("main"));
    }
  }

  ZoneBuffer buffer(&zone);
  builder.WriteTo(buffer);

  if (!ok) {
    // The input data was too short.
    return 0;
  }

  v8::internal::wasm::testing::SetupIsolateForWasmModule(i_isolate);

  v8::internal::HandleScope scope(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  std::unique_ptr<const WasmModule> module(testing::DecodeWasmModuleForTesting(
      i_isolate, &interpreter_thrower, buffer.begin(), buffer.end(),
      v8::internal::wasm::ModuleOrigin::kWasmOrigin, true));

  if (module == nullptr) {
    return 0;
  }
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());
  int32_t result_interpreted;
  bool possible_nondeterminism = false;
  {
    result_interpreted = testing::InterpretWasmModule(
        i_isolate, &interpreter_thrower, module.get(), wire_bytes, 0,
        interpreter_args, &possible_nondeterminism);
  }

  ErrorThrower compiler_thrower(i_isolate, "Compiler");
  v8::internal::Handle<v8::internal::JSObject> instance =
      testing::InstantiateModuleForTesting(i_isolate, &compiler_thrower,
                                           module.get(), wire_bytes);

  if (!interpreter_thrower.error()) {
    CHECK(!instance.is_null());
  } else {
    return 0;
  }
  int32_t result_compiled;
  {
    result_compiled = testing::CallWasmFunctionForTesting(
        i_isolate, instance, &compiler_thrower, "main", argc, compiled_args,
        v8::internal::wasm::ModuleOrigin::kWasmOrigin);
  }
  if (result_interpreted == bit_cast<int32_t>(0xdeadbeef)) {
    CHECK(i_isolate->has_pending_exception());
    i_isolate->clear_pending_exception();
  } else {
    // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
    // This sign bit may cause result_interpreted to be different than
    // result_compiled. Therefore we do not check the equality of the results
    // if the execution may have produced a NaN at some point.
    if (!possible_nondeterminism && (result_interpreted != result_compiled)) {
      V8_Fatal(__FILE__, __LINE__, "WasmCodeFuzzerHash=%x",
               v8::internal::StringHasher::HashSequentialString(
                   data, static_cast<int>(size), WASM_CODE_FUZZER_HASH_SEED));
    }
  }
  return 0;
}
