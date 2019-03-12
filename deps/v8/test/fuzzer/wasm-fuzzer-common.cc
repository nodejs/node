// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-fuzzer-common.h"

#include <ctime>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace fuzzer {

void InterpretAndExecuteModule(i::Isolate* isolate,
                               Handle<WasmModuleObject> module_object) {
  // We do not instantiate the module if there is a start function, because a
  // start function can contain an infinite loop which we cannot handle.
  if (module_object->module()->start_function_index >= 0) return;

  ErrorThrower thrower(isolate, "WebAssembly Instantiation");
  MaybeHandle<WasmInstanceObject> maybe_instance;
  Handle<WasmInstanceObject> instance;

  // Try to instantiate and interpret the module_object.
  maybe_instance = isolate->wasm_engine()->SyncInstantiate(
      isolate, &thrower, module_object,
      Handle<JSReceiver>::null(),     // imports
      MaybeHandle<JSArrayBuffer>());  // memory
  if (!maybe_instance.ToHandle(&instance)) {
    isolate->clear_pending_exception();
    thrower.Reset();  // Ignore errors.
    return;
  }
  if (!testing::InterpretWasmModuleForTesting(isolate, instance, "main", 0,
                                              nullptr)) {
    isolate->clear_pending_exception();
    return;
  }

  // Try to instantiate and execute the module_object.
  maybe_instance = isolate->wasm_engine()->SyncInstantiate(
      isolate, &thrower, module_object,
      Handle<JSReceiver>::null(),     // imports
      MaybeHandle<JSArrayBuffer>());  // memory
  if (!maybe_instance.ToHandle(&instance)) {
    isolate->clear_pending_exception();
    thrower.Reset();  // Ignore errors.
    return;
  }
  if (testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr) < 0) {
    isolate->clear_pending_exception();
    return;
  }
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

struct PrintName {
  WasmName name;
  PrintName(ModuleWireBytes wire_bytes, WireBytesRef ref)
      : name(wire_bytes.GetNameOrNull(ref)) {}
};
std::ostream& operator<<(std::ostream& os, const PrintName& name) {
  return os.write(name.name.start(), name.name.size());
}
}  // namespace

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles) {
  constexpr bool kVerifyFunctions = false;
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(isolate);
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(), kVerifyFunctions,
      ModuleOrigin::kWasmOrigin, isolate->counters(), isolate->allocator());
  CHECK(module_res.ok());
  WasmModule* module = module_res.value().get();
  CHECK_NOT_NULL(module);

  StdoutStream os;

  tzset();
  time_t current_time = time(nullptr);
  struct tm current_localtime;
#ifdef V8_OS_WIN
  localtime_s(&current_localtime, &current_time);
#else
  localtime_r(&current_time, &current_localtime);
#endif
  int year = 1900 + current_localtime.tm_year;

  os << "// Copyright " << year
     << " the V8 project authors. All rights reserved.\n"
        "// Use of this source code is governed by a BSD-style license that "
        "can be\n"
        "// found in the LICENSE file.\n"
        "\n"
        "load('test/mjsunit/wasm/wasm-constants.js');\n"
        "load('test/mjsunit/wasm/wasm-module-builder.js');\n"
        "\n"
        "(function() {\n"
        "  const builder = new WasmModuleBuilder();\n";

  if (module->has_memory) {
    os << "  builder.addMemory(" << module->initial_pages;
    if (module->has_maximum_pages) {
      os << ", " << module->maximum_pages;
    } else {
      os << ", undefined";
    }
    os << ", " << (module->mem_export ? "true" : "false");
    if (module->has_shared_memory) {
      os << ", true";
    }
    os << ");\n";
  }

  for (WasmGlobal& glob : module->globals) {
    os << "  builder.addGlobal(" << ValueTypeToConstantName(glob.type) << ", "
       << glob.mutability << ");\n";
  }

  for (const FunctionSig* sig : module->signatures) {
    os << "  builder.addType(makeSig(" << PrintParameters(sig) << ", "
       << PrintReturns(sig) << "));\n";
  }

  Zone tmp_zone(isolate->allocator(), ZONE_NAME);

  // There currently cannot be more than one table.
  DCHECK_GE(1, module->tables.size());
  for (const WasmTable& table : module->tables) {
    os << "  builder.setTableBounds(" << table.initial_size << ", ";
    if (table.has_maximum_size) {
      os << table.maximum_size << ");\n";
    } else {
      os << "undefined);\n";
    }
  }
  for (const WasmElemSegment& elem_segment : module->elem_segments) {
    os << "  builder.addElementSegment(";
    switch (elem_segment.offset.kind) {
      case WasmInitExpr::kGlobalIndex:
        os << elem_segment.offset.val.global_index << ", true";
        break;
      case WasmInitExpr::kI32Const:
        os << elem_segment.offset.val.i32_const << ", false";
        break;
      default:
        UNREACHABLE();
    }
    os << ", " << PrintCollection(elem_segment.entries) << ");\n";
  }

  for (const WasmFunction& func : module->functions) {
    Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    os << "  // Generate function " << (func.func_index + 1) << " (out of "
       << module->functions.size() << ").\n";

    // Add function.
    os << "  builder.addFunction(undefined, " << func.sig_index
       << " /* sig */)\n";

    // Add locals.
    BodyLocalDecls decls(&tmp_zone);
    DecodeLocalDecls(enabled_features, &decls, func_code.start(),
                     func_code.end());
    if (!decls.type_list.empty()) {
      os << "    ";
      for (size_t pos = 0, count = 1, locals = decls.type_list.size();
           pos < locals; pos += count, count = 1) {
        ValueType type = decls.type_list[pos];
        while (pos + count < locals && decls.type_list[pos + count] == type)
          ++count;
        os << ".addLocals({" << ValueTypes::TypeName(type)
           << "_count: " << count << "})";
      }
      os << "\n";
    }

    // Add body.
    os << "    .addBodyWithEnd([\n";

    FunctionBody func_body(func.sig, func.code.offset(), func_code.start(),
                           func_code.end());
    PrintRawWasmCode(isolate->allocator(), func_body, module, kOmitLocals);
    os << "            ]);\n";
  }

  for (WasmExport& exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    os << "  builder.addExport('" << PrintName(wire_bytes, exp.name) << "', "
       << exp.index << ");\n";
  }

  if (compiles) {
    os << "  const instance = builder.instantiate();\n"
          "  print(instance.exports.main(1, 2, 3));\n";
  } else {
    os << "  assertThrows(function() { builder.instantiate(); }, "
          "WebAssembly.CompileError);\n";
  }
  os << "})();\n";
}

void WasmExecutionFuzzer::FuzzWasmModule(Vector<const uint8_t> data,
                                         bool require_valid) {
  // Strictly enforce the input size limit. Note that setting "max_len" on the
  // fuzzer target is not enough, since different fuzzers are used and not all
  // respect that limit.
  if (data.size() > max_input_size()) return;

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
  // The first byte builds the bitmask to control which function will be
  // compiled with Turbofan and which one with Liftoff.
  uint8_t tier_mask = data.is_empty() ? 0 : data[0];
  if (!data.is_empty()) data += 1;
  if (!GenerateModule(i_isolate, &zone, data, buffer, num_args,
                      interpreter_args, compiler_args)) {
    return;
  }

  testing::SetupIsolateForWasmModule(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  // Compile with Turbofan here. Liftoff will be tested later.
  auto enabled_features = i::wasm::WasmFeaturesFromIsolate(i_isolate);
  MaybeHandle<WasmModuleObject> compiled_module;
  {
    // Explicitly enable Liftoff, disable tiering and set the tier_mask. This
    // way, we deterministically test a combination of Liftoff and Turbofan.
    FlagScope<bool> liftoff(&FLAG_liftoff, true);
    FlagScope<bool> no_tier_up(&FLAG_wasm_tier_up, false);
    FlagScope<int> tier_mask_scope(&FLAG_wasm_tier_mask_for_testing, tier_mask);
    compiled_module = i_isolate->wasm_engine()->SyncCompile(
        i_isolate, enabled_features, &interpreter_thrower, wire_bytes);
  }
  bool compiles = !compiled_module.is_null();

  if (FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  bool validates = i_isolate->wasm_engine()->SyncValidate(
      i_isolate, enabled_features, wire_bytes);

  CHECK_EQ(compiles, validates);
  CHECK_IMPLIES(require_valid, validates);

  if (!compiles) return;

  MaybeHandle<WasmInstanceObject> interpreter_instance =
      i_isolate->wasm_engine()->SyncInstantiate(
          i_isolate, &interpreter_thrower, compiled_module.ToHandleChecked(),
          MaybeHandle<JSReceiver>(), MaybeHandle<JSArrayBuffer>());

  // Ignore instantiation failure.
  if (interpreter_thrower.error()) return;

  testing::WasmInterpretationResult interpreter_result =
      testing::InterpretWasmModule(i_isolate,
                                   interpreter_instance.ToHandleChecked(), 0,
                                   interpreter_args.get());

  // Do not execute the generated code if the interpreter did not finished after
  // a bounded number of steps.
  if (interpreter_result.stopped()) return;

  // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
  // This sign bit can make the difference between an infinite loop and
  // terminating code. With possible non-determinism we cannot guarantee that
  // the generated code will not go into an infinite loop and cause a timeout in
  // Clusterfuzz. Therefore we do not execute the generated code if the result
  // may be non-deterministic.
  if (interpreter_result.possible_nondeterminism()) return;

  int32_t result_compiled;
  {
    ErrorThrower compiler_thrower(i_isolate, "Compile");
    MaybeHandle<WasmInstanceObject> compiled_instance =
        i_isolate->wasm_engine()->SyncInstantiate(
            i_isolate, &compiler_thrower, compiled_module.ToHandleChecked(),
            MaybeHandle<JSReceiver>(), MaybeHandle<JSArrayBuffer>());

    DCHECK(!compiler_thrower.error());
    result_compiled = testing::CallWasmFunctionForTesting(
        i_isolate, compiled_instance.ToHandleChecked(), &compiler_thrower,
        "main", num_args, compiler_args.get());
  }

  if (interpreter_result.trapped() != i_isolate->has_pending_exception()) {
    const char* exception_text[] = {"no exception", "exception"};
    FATAL("interpreter: %s; compiled: %s",
          exception_text[interpreter_result.trapped()],
          exception_text[i_isolate->has_pending_exception()]);
  }

  if (!interpreter_result.trapped()) {
    CHECK_EQ(interpreter_result.result(), result_compiled);
  }

  // Cleanup any pending exception.
  i_isolate->clear_pending_exception();
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
