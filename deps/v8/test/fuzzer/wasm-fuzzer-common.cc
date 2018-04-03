// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-fuzzer-common.h"

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-api.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

static constexpr const char* kNameString = "name";
static constexpr size_t kNameStringLength = 4;

int FuzzWasmSection(SectionCode section, const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  i_isolate->clear_pending_exception();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  buffer.write_u32(kWasmMagic);
  buffer.write_u32(kWasmVersion);
  if (section == kNameSectionCode) {
    buffer.write_u8(kUnknownSectionCode);
    buffer.write_size(size + kNameStringLength + 1);
    buffer.write_u8(kNameStringLength);
    buffer.write(reinterpret_cast<const uint8_t*>(kNameString),
                 kNameStringLength);
    buffer.write(data, size);
  } else {
    buffer.write_u8(section);
    buffer.write_size(size);
    buffer.write(data, size);
  }

  ErrorThrower thrower(i_isolate, "decoder");

  std::unique_ptr<const WasmModule> module(testing::DecodeWasmModuleForTesting(
      i_isolate, &thrower, buffer.begin(), buffer.end(), kWasmOrigin));

  return 0;
}

void InterpretAndExecuteModule(i::Isolate* isolate,
                               Handle<WasmModuleObject> module_object) {
  ScheduledErrorThrower thrower(isolate, "WebAssembly Instantiation");
  // Try to instantiate and interpret the module_object.
  MaybeHandle<WasmInstanceObject> maybe_instance =
      SyncInstantiate(isolate, &thrower, module_object,
                      Handle<JSReceiver>::null(),     // imports
                      MaybeHandle<JSArrayBuffer>());  // memory
  Handle<WasmInstanceObject> instance;
  if (!maybe_instance.ToHandle(&instance)) return;
  if (!testing::InterpretWasmModuleForTesting(isolate, instance, "main", 0,
                                              nullptr)) {
    return;
  }

  // Instantiate and execute the module_object.
  maybe_instance = SyncInstantiate(isolate, &thrower, module_object,
                                   Handle<JSReceiver>::null(),     // imports
                                   MaybeHandle<JSArrayBuffer>());  // memory
  if (!maybe_instance.ToHandle(&instance)) return;

  testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr);
}

namespace {
struct PrintSig {
  const size_t num;
  const std::function<ValueType(size_t)> getter;
};
PrintSig PrintParameters(const FunctionSig* sig) {
  return {sig->parameter_count(), [=](size_t i) { return sig->GetParam(i); }};
}
PrintSig PrintReturns(const FunctionSig* sig) {
  return {sig->return_count(), [=](size_t i) { return sig->GetReturn(i); }};
}
const char* ValueTypeToConstantName(ValueType type) {
  switch (type) {
    case kWasmI32:
      return "kWasmI32";
    case kWasmI64:
      return "kWasmI64";
    case kWasmF32:
      return "kWasmF32";
    case kWasmF64:
      return "kWasmF64";
    default:
      UNREACHABLE();
  }
}
std::ostream& operator<<(std::ostream& os, const PrintSig& print) {
  os << "[";
  for (size_t i = 0; i < print.num; ++i) {
    os << (i == 0 ? "" : ", ") << ValueTypeToConstantName(print.getter(i));
  }
  return os << "]";
}

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles) {
  constexpr bool kVerifyFunctions = false;
  ModuleResult module_res =
      SyncDecodeWasmModule(isolate, wire_bytes.start(), wire_bytes.end(),
                           kVerifyFunctions, ModuleOrigin::kWasmOrigin);
  CHECK(module_res.ok());
  WasmModule* module = module_res.val.get();
  CHECK_NOT_NULL(module);

  OFStream os(stdout);

  os << "// Copyright 2018 the V8 project authors. All rights reserved.\n"
        "// Use of this source code is governed by a BSD-style license that "
        "can be\n"
        "// found in the LICENSE file.\n"
        "\n"
        "load('test/mjsunit/wasm/wasm-constants.js');\n"
        "load('test/mjsunit/wasm/wasm-module-builder.js');\n"
        "\n"
        "(function() {\n"
        "  var builder = new WasmModuleBuilder();\n";

  if (module->has_memory) {
    os << "  builder.addMemory(" << module->initial_pages;
    if (module->has_maximum_pages) {
      os << ", " << module->maximum_pages << ");\n";
    } else {
      os << ");\n";
    }
  }

  Zone tmp_zone(isolate->allocator(), ZONE_NAME);

  for (const WasmFunction& func : module->functions) {
    Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    os << "  // Generate function " << func.func_index + 1 << " of "
       << module->functions.size() << ".\n";
    // Generate signature.
    os << "  sig" << func.func_index << " = makeSig("
       << PrintParameters(func.sig) << ", " << PrintReturns(func.sig) << ");\n";

    // Add function.
    os << "  builder.addFunction(undefined, sig" << func.func_index << ")\n";

    // Add locals.
    BodyLocalDecls decls(&tmp_zone);
    DecodeLocalDecls(&decls, func_code.start(), func_code.end());
    if (!decls.type_list.empty()) {
      os << "    ";
      for (size_t pos = 0, count = 1, locals = decls.type_list.size();
           pos < locals; pos += count, count = 1) {
        ValueType type = decls.type_list[pos];
        while (pos + count < locals && decls.type_list[pos + count] == type)
          ++count;
        os << ".addLocals({" << WasmOpcodes::TypeName(type)
           << "_count: " << count << "})";
      }
      os << "\n";
    }

    // Add body.
    os << "    .addBodyWithEnd([\n";

    FunctionBody func_body(func.sig, func.code.offset(), func_code.start(),
                           func_code.end());
    PrintRawWasmCode(isolate->allocator(), func_body, module, kOmitLocals);
    os << "            ])";
    if (func.func_index == 0) os << "\n            .exportAs('main')";
    os << ";\n ";
  }

  if (compiles) {
    os << "  var module = builder.instantiate();\n"
          "  module.exports.main(1, 2, 3);\n";
  } else {
    os << "  assertThrows(function() { builder.instantiate(); });\n";
  }
  os << "})();\n";
}
}  // namespace

int WasmExecutionFuzzer::FuzzWasmModule(const uint8_t* data, size_t size,
                                        bool require_valid) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  i_isolate->clear_pending_exception();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  int32_t num_args = 0;
  std::unique_ptr<WasmValue[]> interpreter_args;
  std::unique_ptr<Handle<Object>[]> compiler_args;
  if (!GenerateModule(i_isolate, &zone, data, size, buffer, num_args,
                      interpreter_args, compiler_args)) {
    return 0;
  }

  testing::SetupIsolateForWasmModule(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  // Compile with Turbofan here. Liftoff will be tested later.
  MaybeHandle<WasmModuleObject> compiled_module;
  {
    FlagScope<bool> no_liftoff(&FLAG_liftoff, false);
    compiled_module = SyncCompile(i_isolate, &interpreter_thrower, wire_bytes);
  }
  bool compiles = !compiled_module.is_null();

  if (FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  bool validates =
      i_isolate->wasm_engine()->SyncValidate(i_isolate, wire_bytes);

  CHECK_EQ(compiles, validates);
  CHECK_IMPLIES(require_valid, validates);

  if (!compiles) return 0;

  int32_t result_interpreter;
  bool possible_nondeterminism = false;
  {
    MaybeHandle<WasmInstanceObject> interpreter_instance = SyncInstantiate(
        i_isolate, &interpreter_thrower, compiled_module.ToHandleChecked(),
        MaybeHandle<JSReceiver>(), MaybeHandle<JSArrayBuffer>());

    // Ignore instantiation failure.
    if (interpreter_thrower.error()) {
      return 0;
    }

    result_interpreter = testing::InterpretWasmModule(
        i_isolate, interpreter_instance.ToHandleChecked(), &interpreter_thrower,
        0, interpreter_args.get(), &possible_nondeterminism);
  }

  // Do not execute the generated code if the interpreter did not finished after
  // a bounded number of steps.
  if (interpreter_thrower.error()) {
    return 0;
  }

  bool expect_exception =
      result_interpreter == static_cast<int32_t>(0xDEADBEEF);

  int32_t result_turbofan;
  {
    ErrorThrower compiler_thrower(i_isolate, "Turbofan");
    MaybeHandle<WasmInstanceObject> compiled_instance = SyncInstantiate(
        i_isolate, &compiler_thrower, compiled_module.ToHandleChecked(),
        MaybeHandle<JSReceiver>(), MaybeHandle<JSArrayBuffer>());

    DCHECK(!compiler_thrower.error());
    result_turbofan = testing::CallWasmFunctionForTesting(
        i_isolate, compiled_instance.ToHandleChecked(), &compiler_thrower,
        "main", num_args, compiler_args.get());
  }

  // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
  // This sign bit may cause result_interpreter to be different than
  // result_turbofan. Therefore we do not check the equality of the results
  // if the execution may have produced a NaN at some point.
  if (!possible_nondeterminism) {
    CHECK_EQ(expect_exception, i_isolate->has_pending_exception());

    if (!expect_exception) CHECK_EQ(result_interpreter, result_turbofan);
  }

  // Clear any pending exceptions for the next run.
  i_isolate->clear_pending_exception();

  int32_t result_liftoff;
  {
    FlagScope<bool> liftoff(&FLAG_liftoff, true);
    ErrorThrower compiler_thrower(i_isolate, "Liftoff");
    // Re-compile with Liftoff.
    MaybeHandle<WasmInstanceObject> compiled_instance =
        SyncCompileAndInstantiate(i_isolate, &compiler_thrower, wire_bytes,
                                  MaybeHandle<JSReceiver>(),
                                  MaybeHandle<JSArrayBuffer>());
    DCHECK(!compiler_thrower.error());
    result_liftoff = testing::CallWasmFunctionForTesting(
        i_isolate, compiled_instance.ToHandleChecked(), &compiler_thrower,
        "main", num_args, compiler_args.get());
  }
  if (!possible_nondeterminism) {
    CHECK_EQ(expect_exception, i_isolate->has_pending_exception());

    if (!expect_exception) CHECK_EQ(result_interpreter, result_liftoff);
  }

  // Cleanup any pending exception.
  i_isolate->clear_pending_exception();
  return 0;
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
