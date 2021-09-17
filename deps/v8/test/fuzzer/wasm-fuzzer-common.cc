// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm-fuzzer-common.h"

#include <ctime>

#include "include/v8.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
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

// Compile a baseline module. We pass a pointer to a max step counter and a
// nondeterminsm flag that are updated during execution by Liftoff.
Handle<WasmModuleObject> CompileReferenceModule(Zone* zone, Isolate* isolate,
                                                ModuleWireBytes wire_bytes,
                                                ErrorThrower* thrower,
                                                int32_t* max_steps,
                                                int32_t* nondeterminism) {
  // Create the native module.
  std::shared_ptr<NativeModule> native_module;
  constexpr bool kNoVerifyFunctions = false;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(),
      kNoVerifyFunctions, ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->metrics_recorder(), v8::metrics::Recorder::ContextId::Empty(),
      DecodingMethod::kSync, GetWasmEngine()->allocator());
  CHECK(module_res.ok());
  std::shared_ptr<WasmModule> module = module_res.value();
  CHECK_NOT_NULL(module);
  native_module =
      GetWasmEngine()->NewNativeModule(isolate, enabled_features, module, 0);
  native_module->SetWireBytes(
      base::OwnedVector<uint8_t>::Of(wire_bytes.module_bytes()));

  // Compile all functions with Liftoff.
  WasmCodeRefScope code_ref_scope;
  auto env = native_module->CreateCompilationEnv();
  for (size_t i = module->num_imported_functions; i < module->functions.size();
       ++i) {
    auto& func = module->functions[i];
    base::Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    FunctionBody func_body(func.sig, func.code.offset(), func_code.begin(),
                           func_code.end());
    auto result = ExecuteLiftoffCompilation(
        &env, func_body, func.func_index, kForDebugging,
        LiftoffOptions{}.set_max_steps(max_steps).set_nondeterminism(
            nondeterminism));
    native_module->PublishCode(
        native_module->AddCompiledCode(std::move(result)));
  }

  // Create the module object.
  constexpr base::Vector<const char> kNoSourceUrl;
  Handle<Script> script =
      GetWasmEngine()->GetOrCreateScript(isolate, native_module, kNoSourceUrl);
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(
      static_cast<int>(module->num_exported_functions));
  return WasmModuleObject::New(isolate, std::move(native_module), script,
                               export_wrappers);
}

void InterpretAndExecuteModule(i::Isolate* isolate,
                               Handle<WasmModuleObject> module_object,
                               Handle<WasmModuleObject> module_ref,
                               int32_t* max_steps, int32_t* nondeterminism) {
  // We do not instantiate the module if there is a start function, because a
  // start function can contain an infinite loop which we cannot handle.
  if (module_object->module()->start_function_index >= 0) return;

  HandleScope handle_scope(isolate);  // Avoid leaking handles.
  Handle<WasmInstanceObject> instance;

  // Try to instantiate, return if it fails.
  {
    ErrorThrower thrower(isolate, "WebAssembly Instantiation");
    if (!GetWasmEngine()
             ->SyncInstantiate(isolate, &thrower, module_object, {},
                               {})  // no imports & memory
             .ToHandle(&instance)) {
      isolate->clear_pending_exception();
      thrower.Reset();  // Ignore errors.
      return;
    }
  }

  // Get the "main" exported function. Do nothing if it does not exist.
  Handle<WasmExportedFunction> main_function;
  if (!testing::GetExportedFunction(isolate, instance, "main")
           .ToHandle(&main_function)) {
    return;
  }

  base::OwnedVector<Handle<Object>> compiled_args =
      testing::MakeDefaultArguments(isolate, main_function->sig());
  bool exception_ref = false;
  bool exception = false;
  int32_t result_ref = 0;
  int32_t result = 0;

  auto interpreter_result = testing::WasmInterpretationResult::Failed();
  if (module_ref.is_null()) {
    base::OwnedVector<WasmValue> arguments =
        testing::MakeDefaultInterpreterArguments(isolate, main_function->sig());

    // Now interpret.
    testing::WasmInterpretationResult interpreter_result =
        testing::InterpretWasmModule(isolate, instance,
                                     main_function->function_index(),
                                     arguments.begin());
    if (interpreter_result.failed()) return;

    // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
    // This sign bit can make the difference between an infinite loop and
    // terminating code. With possible non-determinism we cannot guarantee that
    // the generated code will not go into an infinite loop and cause a timeout
    // in Clusterfuzz. Therefore we do not execute the generated code if the
    // result may be non-deterministic.
    if (interpreter_result.possible_nondeterminism()) return;
    if (interpreter_result.finished()) {
      result_ref = interpreter_result.result();
    } else {
      DCHECK(interpreter_result.trapped());
      exception_ref = true;
    }
    // Reset the instance before the test run.
    {
      ErrorThrower thrower(isolate, "Second Instantiation");
      // We instantiated before, so the second instantiation must also succeed:
      CHECK(GetWasmEngine()
                ->SyncInstantiate(isolate, &thrower, module_object, {},
                                  {})  // no imports & memory
                .ToHandle(&instance));
    }
  } else {
    Handle<WasmInstanceObject> instance_ref;
    {
      ErrorThrower thrower(isolate, "WebAssembly Instantiation");
      // We instantiated before, so the second instantiation must also succeed:
      CHECK(GetWasmEngine()
                ->SyncInstantiate(isolate, &thrower, module_ref, {},
                                  {})  // no imports & memory
                .ToHandle(&instance_ref));
    }
    result_ref = testing::CallWasmFunctionForTesting(
        isolate, instance_ref, "main", static_cast<int>(compiled_args.size()),
        compiled_args.begin(), &exception_ref);
    // Reached max steps, do not try to execute the test module as it might
    // never terminate.
    if (*max_steps == 0) return;
    // If there is nondeterminism, we cannot guarantee the behavior of the test
    // module, and in particular it may not terminate.
    if (*nondeterminism != 0) return;
  }

  result = testing::CallWasmFunctionForTesting(
      isolate, instance, "main", static_cast<int>(compiled_args.size()),
      compiled_args.begin(), &exception);

  if (exception_ref != exception) {
    const char* exception_text[] = {"no exception", "exception"};
    FATAL("expected: %s; got: %s", exception_text[interpreter_result.trapped()],
          exception_text[exception]);
  }

  if (!exception) {
    CHECK_EQ(result_ref, result);
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
std::string ValueTypeToConstantName(ValueType type) {
  switch (type.kind()) {
    case kI32:
      return "kWasmI32";
    case kI64:
      return "kWasmI64";
    case kF32:
      return "kWasmF32";
    case kF64:
      return "kWasmF64";
    case kS128:
      return "kWasmS128";
    case kOptRef:
      switch (type.heap_representation()) {
        case HeapType::kExtern:
          return "kWasmExternRef";
        case HeapType::kFunc:
          return "kWasmFuncRef";
        case HeapType::kEq:
          return "kWasmEqRef";
        case HeapType::kAny:
          return "kWasmAnyRef";
        case HeapType::kData:
          return "wasmOptRefType(kWasmDataRef)";
        case HeapType::kI31:
          return "wasmOptRefType(kWasmI31Ref)";
        case HeapType::kBottom:
        default:
          return "wasmOptRefType(" + std::to_string(type.ref_index()) + ")";
      }
    default:
      UNREACHABLE();
  }
}

std::string HeapTypeToConstantName(HeapType heap_type) {
  switch (heap_type.representation()) {
    case HeapType::kFunc:
      return "kWasmFuncRef";
    case HeapType::kExtern:
      return "kWasmExternRef";
    case HeapType::kEq:
      return "kWasmEqRef";
    case HeapType::kI31:
      return "kWasmI31Ref";
    case HeapType::kData:
      return "kWasmDataRef";
    case HeapType::kAny:
      return "kWasmAnyRef";
    case HeapType::kBottom:
      UNREACHABLE();
    default:
      return std::to_string(heap_type.ref_index());
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
  return os.write(name.name.begin(), name.name.size());
}

std::ostream& operator<<(std::ostream& os, WasmElemSegment::Entry entry) {
  os << "WasmInitExpr.";
  switch (entry.kind) {
    case WasmElemSegment::Entry::kGlobalGetEntry:
      os << "GlobalGet(" << entry.index;
      break;
    case WasmElemSegment::Entry::kRefFuncEntry:
      os << "RefFunc(" << entry.index;
      break;
    case WasmElemSegment::Entry::kRefNullEntry:
      os << "RefNull(" << HeapType(entry.index).name().c_str();
      break;
  }
  return os << ")";
}

// Appends an initializer expression encoded in {wire_bytes}, in the offset
// contained in {expr}.
// TODO(7748): Find a way to implement other expressions here.
void AppendInitExpr(std::ostream& os, ModuleWireBytes wire_bytes,
                    WireBytesRef expr) {
  Decoder decoder(wire_bytes.module_bytes());
  const byte* pc = wire_bytes.module_bytes().begin() + expr.offset();
  uint32_t length;
  os << "WasmInitExpr.";
  switch (static_cast<WasmOpcode>(pc[0])) {
    case kExprGlobalGet:
      os << "GlobalGet("
         << decoder.read_u32v<Decoder::kNoValidation>(pc + 1, &length);
      break;
    case kExprI32Const:
      os << "I32Const("
         << decoder.read_i32v<Decoder::kNoValidation>(pc + 1, &length);
      break;
    case kExprI64Const:
      os << "I64Const("
         << decoder.read_i64v<Decoder::kNoValidation>(pc + 1, &length);
      break;
    case kExprF32Const: {
      uint32_t result = decoder.read_u32<Decoder::kNoValidation>(pc + 1);
      os << "F32Const(" << bit_cast<float>(result);
      break;
    }
    case kExprF64Const: {
      uint64_t result = decoder.read_u64<Decoder::kNoValidation>(pc + 1);
      os << "F64Const(" << bit_cast<double>(result);
      break;
    }
    case kSimdPrefix: {
      DCHECK_LE(2 + kSimd128Size, expr.length());
      DCHECK_EQ(static_cast<WasmOpcode>(pc[1]), kExprS128Const & 0xff);
      os << "S128Const([";
      for (int i = 0; i < kSimd128Size; i++) {
        os << int(decoder.read_u8<Decoder::kNoValidation>(pc + 2 + i));
        if (i + 1 < kSimd128Size) os << ", ";
      }
      os << "]";
      break;
    }
    case kExprRefFunc:
      os << "RefFunc("
         << decoder.read_u32v<Decoder::kNoValidation>(pc + 1, &length);
      break;
    case kExprRefNull: {
      HeapType heap_type =
          value_type_reader::read_heap_type<Decoder::kNoValidation>(
              &decoder, pc + 1, &length, nullptr, WasmFeatures::All());
      os << "RefNull(" << HeapTypeToConstantName(heap_type);
      break;
    }
    default:
      UNREACHABLE();
  }
  os << ")";
}
}  // namespace

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles) {
  constexpr bool kVerifyFunctions = false;
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(), kVerifyFunctions,
      ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->metrics_recorder(), v8::metrics::Recorder::ContextId::Empty(),
      DecodingMethod::kSync, GetWasmEngine()->allocator());
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
        "// Flags: --wasm-staging --experimental-wasm-gc\n"
        "\n"
        "load('test/mjsunit/wasm/wasm-module-builder.js');\n"
        "\n"
        "const builder = new WasmModuleBuilder();\n";

  if (module->has_memory) {
    os << "builder.addMemory(" << module->initial_pages;
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
    os << "builder.addGlobal(" << ValueTypeToConstantName(glob.type) << ", "
       << glob.mutability << ", ";
    AppendInitExpr(os, wire_bytes, glob.init);
    os << ");\n";
  }

#if DEBUG
  for (uint8_t kind : module->type_kinds) {
    DCHECK(kWasmArrayTypeCode == kind || kWasmStructTypeCode == kind ||
           kWasmFunctionTypeCode == kind);
  }
#endif

  for (int i = 0; i < static_cast<int>(module->types.size()); i++) {
    if (module->has_struct(i)) {
      const StructType* struct_type = module->types[i].struct_type;
      os << "builder.addStruct([";
      int field_count = struct_type->field_count();
      for (int index = 0; index < field_count; index++) {
        os << "makeField(" << ValueTypeToConstantName(struct_type->field(index))
           << ", " << (struct_type->mutability(index) ? "true" : "false")
           << ")";
        if (index + 1 < field_count)
          os << ", ";
        else
          os << "]);\n";
      }
    } else if (module->has_array(i)) {
      const ArrayType* array_type = module->types[i].array_type;
      os << "builder.addArray("
         << ValueTypeToConstantName(array_type->element_type()) << ","
         << (array_type->mutability() ? "true" : "false") << ");\n";
    } else {
      DCHECK(module->has_signature(i));
      const FunctionSig* sig = module->types[i].function_sig;
      os << "builder.addType(makeSig(" << PrintParameters(sig) << ", "
         << PrintReturns(sig) << "));\n";
    }
  }

  Zone tmp_zone(isolate->allocator(), ZONE_NAME);

  // There currently cannot be more than one table.
  // TODO(manoskouk): Add support for more tables.
  // TODO(9495): Add support for talbes with explicit initializers.
  DCHECK_GE(1, module->tables.size());
  for (const WasmTable& table : module->tables) {
    os << "builder.setTableBounds(" << table.initial_size << ", ";
    if (table.has_maximum_size) {
      os << table.maximum_size << ");\n";
    } else {
      os << "undefined);\n";
    }
  }
  for (const WasmElemSegment& elem_segment : module->elem_segments) {
    const char* status_str =
        elem_segment.status == WasmElemSegment::kStatusActive
            ? "Active"
            : elem_segment.status == WasmElemSegment::kStatusPassive
                  ? "Passive"
                  : "Declarative";
    os << "builder.add" << status_str << "ElementSegment(";
    if (elem_segment.status == WasmElemSegment::kStatusActive) {
      os << elem_segment.table_index << ", ";
      AppendInitExpr(os, wire_bytes, elem_segment.offset);
      os << ", ";
    }
    os << "[";
    for (uint32_t i = 0; i < elem_segment.entries.size(); i++) {
      os << elem_segment.entries[i];
      if (i < elem_segment.entries.size() - 1) os << ", ";
    }
    os << "], " << ValueTypeToConstantName(elem_segment.type) << ");\n";
  }

  for (const WasmFunction& func : module->functions) {
    base::Vector<const uint8_t> func_code = wire_bytes.GetFunctionBytes(&func);
    os << "// Generate function " << (func.func_index + 1) << " (out of "
       << module->functions.size() << ").\n";

    // Add function.
    os << "builder.addFunction(undefined, " << func.sig_index
       << " /* sig */)\n";

    // Add locals.
    BodyLocalDecls decls(&tmp_zone);
    DecodeLocalDecls(enabled_features, &decls, module, func_code.begin(),
                     func_code.end());
    if (!decls.type_list.empty()) {
      os << "  ";
      for (size_t pos = 0, count = 1, locals = decls.type_list.size();
           pos < locals; pos += count, count = 1) {
        ValueType type = decls.type_list[pos];
        while (pos + count < locals && decls.type_list[pos + count] == type) {
          ++count;
        }
        os << ".addLocals(" << ValueTypeToConstantName(type) << ", " << count
           << ")";
      }
      os << "\n";
    }

    // Add body.
    os << "  .addBodyWithEnd([\n";

    FunctionBody func_body(func.sig, func.code.offset(), func_code.begin(),
                           func_code.end());
    PrintRawWasmCode(isolate->allocator(), func_body, module, kOmitLocals);
    os << "]);\n";
  }

  for (WasmExport& exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    os << "builder.addExport('" << PrintName(wire_bytes, exp.name) << "', "
       << exp.index << ");\n";
  }

  if (compiles) {
    os << "const instance = builder.instantiate();\n"
          "print(instance.exports.main(1, 2, 3));\n";
  } else {
    os << "assertThrows(function() { builder.instantiate(); }, "
          "WebAssembly.CompileError);\n";
  }
}

void OneTimeEnableStagedWasmFeatures(v8::Isolate* isolate) {
  struct EnableStagedWasmFeatures {
    explicit EnableStagedWasmFeatures(v8::Isolate* isolate) {
#define ENABLE_STAGED_FEATURES(feat, desc, val) \
  FLAG_experimental_wasm_##feat = true;
      FOREACH_WASM_STAGING_FEATURE_FLAG(ENABLE_STAGED_FEATURES)
#undef ENABLE_STAGED_FEATURES
      isolate->InstallConditionalFeatures(isolate->GetCurrentContext());
    }
  };
  // The compiler will properly synchronize the constructor call.
  static EnableStagedWasmFeatures one_time_enable_staged_features(isolate);
}

void WasmExecutionFuzzer::FuzzWasmModule(base::Vector<const uint8_t> data,
                                         bool require_valid) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // Strictly enforce the input size limit. Note that setting "max_len" on the
  // fuzzer target is not enough, since different fuzzers are used and not all
  // respect that limit.
  if (data.size() > max_input_size()) return;

  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  i_isolate->clear_pending_exception();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  OneTimeEnableStagedWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  // The first byte builds the bitmask to control which function will be
  // compiled with Turbofan and which one with Liftoff.
  uint8_t tier_mask = data.empty() ? 0 : data[0];
  if (!data.empty()) data += 1;
  // Build the bitmask to control which functions should be compiled for
  // debugging.
  uint8_t debug_mask = data.empty() ? 0 : data[0];
  if (!data.empty()) data += 1;
    // Control whether Liftoff or the interpreter will be used as the reference
    // tier.
    // TODO(thibaudm): Port nondeterminism detection to arm.
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_X86)
  bool liftoff_as_reference = data.empty() ? false : data[0] % 2;
#else
  bool liftoff_as_reference = false;
#endif
  if (!data.empty()) data += 1;
  if (!GenerateModule(i_isolate, &zone, data, &buffer, liftoff_as_reference)) {
    return;
  }

  testing::SetupIsolateForWasmModule(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  if (require_valid && FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, true);
  }

  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
  MaybeHandle<WasmModuleObject> compiled_module;
  {
    // Explicitly enable Liftoff, disable tiering and set the tier_mask. This
    // way, we deterministically test a combination of Liftoff and Turbofan.
    FlagScope<bool> liftoff(&FLAG_liftoff, true);
    FlagScope<bool> no_tier_up(&FLAG_wasm_tier_up, false);
    FlagScope<int> tier_mask_scope(&FLAG_wasm_tier_mask_for_testing, tier_mask);
    FlagScope<int> debug_mask_scope(&FLAG_wasm_debug_mask_for_testing,
                                    debug_mask);
    compiled_module = GetWasmEngine()->SyncCompile(
        i_isolate, enabled_features, &interpreter_thrower, wire_bytes);
  }
  bool compiles = !compiled_module.is_null();
  if (!require_valid && FLAG_wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, compiles);
  }

  bool validates =
      GetWasmEngine()->SyncValidate(i_isolate, enabled_features, wire_bytes);

  CHECK_EQ(compiles, validates);
  CHECK_IMPLIES(require_valid, validates);

  if (!compiles) return;

  int32_t max_steps = 16 * 1024;
  int32_t nondeterminism = false;
  Handle<WasmModuleObject> module_ref;
  if (liftoff_as_reference) {
    module_ref = CompileReferenceModule(&zone, i_isolate, wire_bytes,
                                        &interpreter_thrower, &max_steps,
                                        &nondeterminism);
  }
  InterpretAndExecuteModule(i_isolate, compiled_module.ToHandleChecked(),
                            module_ref, &max_steps, &nondeterminism);
}

}  // namespace fuzzer
}  // namespace wasm
}  // namespace internal
}  // namespace v8
