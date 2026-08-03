// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/wasm/fuzzer-common.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-metrics.h"
#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/string-builder-multiline.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-tracing.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "tools/wasm/mjsunit-module-disassembler-impl.h"

#if V8_ENABLE_DRUMBRAKE
#include "src/wasm/interpreter/wasm-interpreter.h"
#endif  // V8_ENABLE_DRUMBRAKE

namespace v8::internal::wasm::fuzzing {

namespace {

V8_WARN_UNUSED_RESULT
bool CompileAllFunctionsForReferenceExecution(NativeModule* native_module,
                                              int32_t* max_steps) {
  const WasmModule* module = native_module->module();
  WasmCodeRefScope code_ref_scope;
  CompilationEnv env = CompilationEnv::ForModule(native_module);
  ModuleWireBytes wire_bytes_accessor{native_module->wire_bytes()};
  for (size_t i = module->num_imported_functions; i < module->functions.size();
       ++i) {
    auto& func = module->functions[i];
    base::Vector<const uint8_t> func_code =
        wire_bytes_accessor.GetFunctionBytes(&func);
    constexpr bool kIsShared = false;
    FunctionBody func_body(func.sig, func.code.offset(), func_code.begin(),
                           func_code.end(), kIsShared);
    auto result =
        ExecuteLiftoffCompilation(&env, func_body,
                                  LiftoffOptions{}
                                      .set_func_index(func.func_index)
                                      .set_for_debugging(kForDebugging)
                                      .set_max_steps(max_steps)
                                      // TODO(clemensb): Fully remove
                                      // nondeterminism detection.
                                      .set_detect_nondeterminism(false));
    if (!result.succeeded()) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
      // Liftoff compilation can bailout on x64 / ia32 if SSE4.1 is unavailable.
      if (!CpuFeatures::IsSupported(SSE4_1)) return false;
#endif
      FATAL(
          "Liftoff compilation failed on a valid module. Run with "
          "--trace-wasm-decoder (in a debug build) to see why.");
    }
    native_module->PublishCode(native_module->AddCompiledCode(result));
  }
  return true;
}

}  // namespace

bool ValuesEquivalent(const WasmValue& init_lhs, const WasmValue& init_rhs,
                      Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  // Stack of elements to be checked.
  std::vector<std::pair<WasmValue, WasmValue>> cmp = {{init_lhs, init_rhs}};
  using TaggedT = decltype(Tagged<Object>().ptr());
  // Map of lhs objects we have already seen to their rhs object on the first
  // visit. This is needed to ensure a reasonable runtime for the check.
  // Example:
  //   (array.new $myArray 10 (array.new_default $myArray 10))
  // This creates a nested array where each outer array element is the same
  // inner array. Without memorizing the inner array, we'd end up performing
  // 100+ comparisons.
  std::unordered_map<TaggedT, TaggedT> lhs_map;

  auto CheckArray = [&cmp, &lhs_map](Tagged<WasmArray> lhs,
                                     Tagged<WasmArray> rhs) -> bool {
    auto [iter, inserted] = lhs_map.insert({lhs.ptr(), rhs.ptr()});
    if (!inserted) {
      return iter->second == rhs.ptr();
    }
    if (lhs->map()->wasm_type_info()->type() !=
        rhs->map()->wasm_type_info()->type()) {
      return false;
    }
    if (lhs->length() != rhs->length()) {
      return false;
    }
    cmp.reserve(cmp.size() + lhs->length());
    for (uint32_t i = 0; i < lhs->length(); ++i) {
      cmp.emplace_back(lhs->GetElement(i), rhs->GetElement(i));
    }
    return true;
  };

  auto CheckStruct = [&cmp, &lhs_map](Tagged<WasmStruct> lhs,
                                      Tagged<WasmStruct> rhs) -> bool {
    auto [iter, inserted] = lhs_map.insert({lhs.ptr(), rhs.ptr()});
    if (!inserted) {
      return iter->second == rhs.ptr();
    }
    if (lhs->map()->wasm_type_info()->type() !=
        rhs->map()->wasm_type_info()->type()) {
      return false;
    }
    const CanonicalStructType* type = GetTypeCanonicalizer()->LookupStruct(
        lhs->map()->wasm_type_info()->type_index());
    uint32_t field_count = type->field_count();
    for (uint32_t i = 0; i < field_count; ++i) {
      cmp.emplace_back(lhs->GetFieldValue(i), rhs->GetFieldValue(i));
    }
    return true;
  };

  while (!cmp.empty()) {
    const auto [lhs, rhs] = cmp.back();
    cmp.pop_back();

    if (lhs.type() != rhs.type()) return false;
    CHECK(!lhs.type().is_sentinel());  // top, bottom, void can't get here.

    if (lhs.type().is_numeric()) {
      if (lhs != rhs) return false;
      continue;
    }

    CHECK(lhs.type().is_ref());
    Tagged<Object> lhs_ref = *lhs.to_ref();
    Tagged<Object> rhs_ref = *rhs.to_ref();

    // Nulls and Smis can be compared by pointer equality.
    if (IsNull(lhs_ref) || IsWasmNull(lhs_ref) || IsSmi(lhs_ref)) {
      if (rhs_ref != lhs_ref) return false;
    } else if (IsWasmFuncRef(lhs_ref)) {
      if (!IsWasmFuncRef(rhs_ref)) return false;
      if (Cast<WasmFuncRef>(lhs_ref)->internal(isolate)->function_index() !=
          Cast<WasmFuncRef>(rhs_ref)->internal(isolate)->function_index()) {
        return false;
      }
    } else if (IsWasmStruct(lhs_ref)) {
      if (!IsWasmStruct(rhs_ref)) return false;
      if (!CheckStruct(Cast<WasmStruct>(lhs_ref), Cast<WasmStruct>(rhs_ref))) {
        return false;
      }
    } else if (IsWasmArray(lhs_ref)) {
      if (!IsWasmArray(rhs_ref)) return false;
      if (!CheckArray(Cast<WasmArray>(lhs_ref), Cast<WasmArray>(rhs_ref))) {
        return false;
      }
    } else if (IsString(lhs_ref)) {
      if (!IsString(rhs_ref)) return false;
      if (!Cast<String>(lhs_ref)->Equals(Cast<String>(rhs_ref))) return false;
    } else {
      PrintF(
          "WARNING: equivalence checking for instance type %d is not "
          "implemented, results may be unreliable\n",
          Cast<HeapObject>(lhs_ref)->map()->instance_type());
      // TODO(nikolaskaipel): Consider putting `UNIMPLEMENTED()` or
      // `return false` here.
    }
  }

  return true;
}

void PrintValue(std::ostream& os, const WasmValue& value) {
  enum class PrintSymbol {
    kStructClose,
    kArrayClose,
    kComma,
  };

  using PrintTask = std::variant<WasmValue, PrintSymbol>;
  using ObjectPtr = decltype(Tagged<Object>().ptr());

  std::vector<PrintTask> print_stack = {value};
  std::unordered_set<ObjectPtr> seen_objects;

  while (!print_stack.empty()) {
    PrintTask task = print_stack.back();
    print_stack.pop_back();

    if (PrintSymbol* symbol = std::get_if<PrintSymbol>(&task)) {
      switch (*symbol) {
        case PrintSymbol::kStructClose:
          os << "}";
          break;
        case PrintSymbol::kArrayClose:
          os << "]";
          break;
        case PrintSymbol::kComma:
          os << ", ";
          break;
      }
    } else {
      WasmValue current_val = std::get<WasmValue>(task);
      if (current_val.type().is_numeric()) {
        os << current_val.to_string();
      } else if (current_val.type().is_ref()) {
        Tagged<Object> ref = *current_val.to_ref();
        if (IsNull(ref) || IsWasmNull(ref)) {
          os << "null";
          continue;
        }

        if (IsSmi(ref) || IsWasmFuncRef(ref)) {
          os << "<" << current_val.to_string() << ">";
          continue;
        }

        if (seen_objects.count(ref.ptr())) {
          os << "<repeat>";
          continue;
        }
        seen_objects.insert(ref.ptr());

        if (IsWasmStruct(ref)) {
          Tagged<WasmStruct> struct_ref = Cast<WasmStruct>(ref);
          const auto* type = GetTypeCanonicalizer()->LookupStruct(
              struct_ref->map()->wasm_type_info()->type_index());
          uint32_t count = type->field_count();

          print_stack.push_back(PrintSymbol::kStructClose);
          for (uint32_t i = count; i-- > 0;) {
            print_stack.push_back(struct_ref->GetFieldValue(i));
            if (i > 0) {
              print_stack.push_back(PrintSymbol::kComma);
            }
          }
          os << '{';
        } else if (IsWasmArray(ref)) {
          Tagged<WasmArray> array_ref = Cast<WasmArray>(ref);
          uint32_t len = array_ref->length();

          print_stack.push_back(PrintSymbol::kArrayClose);
          for (uint32_t i = len; i-- > 0;) {
            print_stack.push_back(array_ref->GetElement(i));
            if (i > 0) {
              print_stack.push_back(PrintSymbol::kComma);
            }
          }
          os << '[';
        } else {
          os << "<" << current_val.to_string() << ">";
        }
      } else {
        os << "?";
      }
    }
  }
}

template <typename TraceEntry, typename PrintEntryFunc>
void CompareAndPrintTraces(const std::vector<TraceEntry>& trace,
                           const std::vector<TraceEntry>& ref_trace,
                           NativeModule* native_module, std::ostream& outs,
                           PrintEntryFunc print_entry) {
  CHECK_EQ(trace.size(), ref_trace.size());

  static constexpr size_t kMaxTracesToPrint = 50;
  static constexpr size_t kFirstMismatchContextSize = 10;

  // If the trace is small enough, just print everything.
  if (trace.size() <= kMaxTracesToPrint) {
    uint32_t mismatches = 0;
    for (size_t i = 0; i < trace.size(); ++i) {
      if (trace[i] != ref_trace[i]) {
        outs << "Mismatch at line " << (i + 1) << "\n";
        outs << "  Reference: ";
        print_entry(ref_trace[i], native_module, outs);
        outs << "\n  Actual:    ";
        print_entry(trace[i], native_module, outs);
        mismatches++;
      } else {
        print_entry(trace[i], native_module, outs);
      }
      outs << "\n";
    }
    if (mismatches == 0) {
      outs << "Traces are identical (" << trace.size() << " entries).\n";
    } else {
      outs << "Found " << mismatches << " mismatches in " << trace.size()
           << " entries.\n";
    }
    return;
  }

  // Get the indices of each mismatch.
  std::vector<size_t> mismatch_indices;
  for (size_t i = 0; i < trace.size(); ++i) {
    if (trace[i] != ref_trace[i]) {
      mismatch_indices.push_back(i);
    }
  }

  // If there are no mismatches, print the start and end of the trace.
  if (mismatch_indices.empty()) {
    size_t half = kMaxTracesToPrint / 2;
    for (size_t i = 0; i < half; ++i) {
      print_entry(trace[i], native_module, outs);
      outs << "\n";
    }
    outs << "[...]\n";
    for (size_t i = trace.size() - half; i < trace.size(); ++i) {
      print_entry(trace[i], native_module, outs);
      outs << "\n";
    }
    outs << "Traces are identical (" << trace.size() << " entries).\n";
    return;
  }

  // If there are mismatches, print the first mismatch with context and then the
  // rest of the mismatches.
  outs << "Found " << mismatch_indices.size() << " mismatches in "
       << trace.size() << " entries.\n";

  size_t first_mismatch_idx = mismatch_indices[0];
  outs << "\nContext around the first mismatch at line "
       << (first_mismatch_idx + 1) << ":\n";
  size_t start_context = (first_mismatch_idx > kFirstMismatchContextSize)
                             ? (first_mismatch_idx - kFirstMismatchContextSize)
                             : 0;
  size_t end_context = std::min(
      trace.size(), first_mismatch_idx + kFirstMismatchContextSize + 1);

  for (size_t i = start_context; i < end_context; ++i) {
    if (trace[i] != ref_trace[i]) {
      outs << "Mismatch at line " << (i + 1) << "\n";
      outs << "  Reference: ";
      print_entry(ref_trace[i], native_module, outs);
      outs << "\n  Actual:    ";
      print_entry(trace[i], native_module, outs);
      outs << "\n";
    } else {
      outs << "Match    at line " << (i + 1) << ": ";
      print_entry(trace[i], native_module, outs);
      outs << "\n";
    }
  }

  // Print the rest of mismatches.
  size_t last_printed_idx_in_context = end_context - 1;

  size_t mismatches_to_print =
      std::min(mismatch_indices.size(), kMaxTracesToPrint);
  for (size_t i = 1; i < mismatches_to_print; ++i) {
    size_t current_mismatch_idx = mismatch_indices[i];

    // Skip if this mismatch was already shown in the initial context.
    if (current_mismatch_idx <= last_printed_idx_in_context) {
      continue;
    }

    // If there's a gap between this mismatch and the previous one, print
    // "[...]".
    if (current_mismatch_idx > mismatch_indices[i - 1] + 1) {
      outs << "[...]\n";
    }

    outs << "Mismatch at line " << (current_mismatch_idx + 1) << "\n";
    outs << "  Reference: ";
    print_entry(ref_trace[current_mismatch_idx], native_module, outs);
    outs << "\n  Actual:    ";
    print_entry(trace[current_mismatch_idx], native_module, outs);
    outs << "\n";
  }
  if (mismatch_indices.size() > kMaxTracesToPrint) {
    outs << "[... " << (mismatch_indices.size() - kMaxTracesToPrint)
         << " more mismatches not shown]\n";
  }
}

void CompareAndPrintMemoryTraces(wasm::MemoryTrace& memory_trace,
                                 wasm::MemoryTrace& ref_memory_trace,
                                 NativeModule* native_module,
                                 std::ostream& outs) {
  CompareAndPrintTraces(memory_trace, ref_memory_trace, native_module, outs,
                        PrintMemoryTraceString);
}

void CompareAndPrintGlobalTraces(wasm::GlobalTrace& global_trace,
                                 wasm::GlobalTrace& ref_global_trace,
                                 NativeModule* native_module,
                                 std::ostream& outs) {
  CompareAndPrintTraces(global_trace, ref_global_trace, native_module, outs,
                        PrintGlobalTraceString);
}

CompileTimeImports CompileTimeImportsForFuzzing() {
  CompileTimeImports result;
  result.Add(CompileTimeImport::kJsString);
  result.Add(CompileTimeImport::kTextDecoder);
  result.Add(CompileTimeImport::kTextEncoder);
  return result;
}

// Compile a baseline module. We pass a pointer to a max step counter and a
// nondeterminsm flag that are updated during execution by Liftoff.
MaybeDirectHandle<WasmModuleObject> CompileReferenceModule(
    Isolate* isolate, base::Vector<const uint8_t> wire_bytes,
    int32_t* max_steps) {
  // Create the native module.
  std::shared_ptr<NativeModule> native_module;
  constexpr bool kNoVerifyFunctions = false;
  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
  WasmDetectedFeatures detected_features;
  ModuleResult module_res =
      DecodeWasmModule(enabled_features, wire_bytes, kNoVerifyFunctions,
                       ModuleOrigin::kWasmOrigin, &detected_features);
  CHECK(module_res.ok());
  std::shared_ptr<WasmModule> module = std::move(module_res).value();
  CHECK_NOT_NULL(module);
  CompileTimeImports compile_imports = CompileTimeImportsForFuzzing();
  WasmError imports_error = ValidateAndSetBuiltinImports(
      module.get(), wire_bytes, compile_imports, &detected_features);
  CHECK(!imports_error.has_error());  // The module was compiled before.
  const size_t code_size_estimate =
      WasmCodeManager::EstimateNativeModuleCodeSize(module.get());
  native_module = GetWasmEngine()->NewNativeModule(
      isolate, enabled_features, detected_features,
      CompileTimeImportsForFuzzing(), module, code_size_estimate);
  native_module->SetWireBytes(base::OwnedCopyOf(wire_bytes));
  // The module is known to be valid as this point (it was compiled by the
  // caller before).
  module->set_all_functions_validated();

  // The value is -3 so that it is different than the compilation ID of actual
  // compilations, different than the sentinel value of the CompilationState
  // (-1) and the value used by native module deserialization (-2).
  const int dummy_fuzzing_compilation_id = -3;
  native_module->compilation_state()->set_compilation_id(
      dummy_fuzzing_compilation_id);
  InitializeCompilationForTesting(native_module.get());

  // Compile all functions with Liftoff.
  // This can fail on missing CPU features.
  if (!CompileAllFunctionsForReferenceExecution(native_module.get(),
                                                max_steps)) {
    return {};
  }

  // Create the module object.
  constexpr base::Vector<const char> kNoSourceUrl;
  DirectHandle<Script> script =
      GetWasmEngine()->GetOrCreateScript(isolate, native_module, kNoSourceUrl);
  TypeCanonicalizer::PrepareForCanonicalTypeId(isolate,
                                               module->MaxCanonicalTypeIndex());
  return WasmModuleObject::New(isolate, std::move(native_module), script);
}

#if V8_ENABLE_DRUMBRAKE
void ClearJsToWasmWrappersForTesting(Isolate* isolate) {
  isolate->heap()->SetJSToWasmWrappers(
      ReadOnlyRoots(isolate).empty_weak_fixed_array());
}
#endif  // V8_ENABLE_DRUMBRAKE

struct ExecutionResult {
  DirectHandle<WasmInstanceObject> instance;
  std::unique_ptr<const char[]> exception;
  int32_t result = -1;
  bool should_execute_non_reference = false;
};

ExecutionResult ExecuteReferenceRun(Isolate* isolate,
                                    base::Vector<const uint8_t> wire_bytes,
                                    int exported_main_function_index,
                                    int32_t max_executed_instructions) {
  // The reference module uses a special compilation mode of Liftoff for
  // termination and nondeterminism detected, and that would be undone by
  // flushing that code.
  FlagScope<bool> no_liftoff_code_flushing(&v8_flags.flush_liftoff_code, false);

  int32_t max_steps = max_executed_instructions;

  Zone reference_module_zone(isolate->allocator(), "wasm reference module");
  DirectHandle<WasmModuleObject> module_ref;
  if (!CompileReferenceModule(isolate, wire_bytes, &max_steps)
           .ToHandle(&module_ref)) {
    return {};
  }
  DirectHandle<WasmInstanceObject> instance_ref;

  // Before execution, there should be no dangling nondeterminism registered on
  // the engine, no pending exception, and no termination request.
  DCHECK(!WasmEngine::had_nondeterminism());
  DCHECK(!isolate->has_exception());
  DCHECK(!isolate->stack_guard()->CheckTerminateExecution());

  // Try to instantiate the reference instance, return if it fails.
  {
    ErrorThrower thrower(isolate, "ExecuteAgainstReference");
    if (!GetWasmEngine()
             ->SyncInstantiate(isolate, &thrower, module_ref, {},
                               {})  // no imports & memory
             .ToHandle(&instance_ref)) {
      isolate->clear_exception();
      thrower.Reset();  // Ignore errors.
      return {};
    }
  }

  // Get the "main" exported function. We checked before that this exists.
  DirectHandle<WasmExportedFunction> main_function;
  CHECK(testing::GetExportedFunction(isolate, instance_ref, "main")
            .ToHandle(&main_function));

  struct OomCallbackData {
    Isolate* isolate;
    bool heap_limit_reached{false};
    size_t initial_limit{0};
  };
  OomCallbackData oom_callback_data{isolate};
  auto heap_limit_callback = [](void* raw_data, size_t current_limit,
                                size_t initial_limit) -> size_t {
    OomCallbackData* data = reinterpret_cast<OomCallbackData*>(raw_data);
    if (data->heap_limit_reached) return initial_limit;
    data->heap_limit_reached = true;
    // We can not throw an exception directly at this point, so request
    // termination on the next stack check.
    data->isolate->stack_guard()->RequestTerminateExecution();
    data->initial_limit = initial_limit;
    // Return a generously raised limit to maximize the chance to make it to the
    // next interrupt check point, where execution will terminate.
    return initial_limit * 4;
  };
  isolate->heap()->AddNearHeapLimitCallback(heap_limit_callback,
                                            &oom_callback_data);

  Tagged<WasmExportedFunctionData> func_data =
      main_function->shared()->wasm_exported_function_data();
  DCHECK_EQ(exported_main_function_index, func_data->function_index());
  const FunctionSig* sig = func_data->instance_data()
                               ->module()
                               ->functions[func_data->function_index()]
                               .sig;
  DirectHandleVector<Object> compiled_args =
      testing::MakeDefaultArguments(isolate, sig);
  std::unique_ptr<const char[]> exception;
  int32_t result_ref = testing::CallWasmFunctionForTesting(
      isolate, instance_ref, "main", base::VectorOf(compiled_args), &exception);
  bool execute = true;
  // Reached max steps, do not try to execute the test module as it might
  // never terminate.
  if (max_steps < 0) execute = false;
  // If there is nondeterminism, we cannot guarantee the behavior of the test
  // module, and in particular it may not terminate.
  if (WasmEngine::clear_nondeterminism()) execute = false;
  // Similar to max steps reached, also discard modules that need too much
  // memory.
  isolate->heap()->RemoveNearHeapLimitCallback(heap_limit_callback,
                                               oom_callback_data.initial_limit);
  if (oom_callback_data.heap_limit_reached) {
    execute = false;
    isolate->stack_guard()->ClearTerminateExecution();
  }

  if (exception) {
    if (strcmp(exception.get(),
               "RangeError: Maximum call stack size exceeded") == 0) {
      // There was a stack overflow, which may happen nondeterministically. We
      // cannot guarantee the behavior of the test module, and in particular it
      // may not terminate.
      execute = false;
    }
  }

  if (!execute) {
    // Before discarding the module, see if Turbofan runs into any DCHECKs.
    TierUpAllForTesting(isolate, instance_ref->trusted_data(isolate));
    return {};
  }

  return {instance_ref, std::move(exception), result_ref, true};
}

// This function instantiates and runs the module for the actual, non-reference
// run.
static std::optional<ExecutionResult> ExecuteNonReferenceRun(
    Isolate* isolate, const WasmModule* module,
    DirectHandle<WasmModuleObject> module_object, int exported_main) {
  DirectHandle<WasmInstanceObject> instance;
  {
    ErrorThrower thrower(isolate, "ExecuteNonReferenceRun");
    if (!GetWasmEngine()
             ->SyncInstantiate(isolate, &thrower, module_object, {},
                               {})  // no imports & memory
             .ToHandle(&instance)) {
      CHECK(thrower.error());
      // The only reason to fail this instantiation should be OOM, because
      // there is a previous instantiation in the reference run.
      if (strstr(thrower.error_msg(), "Out of memory")) {
        // The initial memory size might be too large for instantiation
        // (especially on 32 bit systems), therefore do not treat it as a fuzzer
        // failure.
        return {};
      }
      FATAL("Instantiation failed unexpectedly: %s", thrower.error_msg());
    }
    CHECK(!thrower.error());
  }

  std::unique_ptr<const char[]> exception;
  const FunctionSig* sig = module->functions[exported_main].sig;
  DirectHandleVector<Object> compiled_args =
      testing::MakeDefaultArguments(isolate, sig);
  int32_t result = testing::CallWasmFunctionForTesting(
      isolate, instance, "main", base::VectorOf(compiled_args), &exception);

  return {{instance, std::move(exception), result, false}};
}

int FindExportedMainFunction(const WasmModule* module,
                             base::Vector<const uint8_t> wire_bytes) {
  constexpr base::Vector<const char> kMainName = base::StaticCharVector("main");
  for (const WasmExport& exp : module->export_table) {
    if (exp.kind != ImportExportKindCode::kExternalFunction) continue;
    base::Vector<const uint8_t> name =
        wire_bytes.SubVector(exp.name.offset(), exp.name.end_offset());
    if (base::Vector<const char>::cast(name) != kMainName) continue;
    return exp.index;
  }
  return -1;
}

bool GlobalsMatch(Isolate* isolate, const WasmModule* module,
                  Tagged<WasmTrustedInstanceData> instance_data,
                  Tagged<WasmTrustedInstanceData> ref_instance_data,
                  bool print_difference) {
  size_t globals_count = module->globals.size();

  int global_mismatches = 0;
  for (size_t i = 0; i < globals_count; ++i) {
    const WasmGlobal& global = module->globals[i];
    WasmValue value = instance_data->GetGlobalValue(isolate, global);
    WasmValue ref_value = ref_instance_data->GetGlobalValue(isolate, global);

    if (!ValuesEquivalent(value, ref_value, isolate)) {
      if (print_difference) {
        std::ostringstream ss;
        ss << "Error: Global variables at index " << i
           << " have different values!\n";
        ss << "  - Reference: ";
        PrintValue(ss, ref_value);
        ss << "\n  - Actual:    ";
        PrintValue(ss, value);
        base::OS::PrintError("%s\n", ss.str().c_str());
      }
      global_mismatches++;
    }
  }

  return global_mismatches == 0;
}

bool MemoriesMatch(Isolate* isolate, const WasmModule* module,
                   Tagged<WasmTrustedInstanceData> instance_data,
                   Tagged<WasmTrustedInstanceData> ref_instance_data,
                   bool print_difference) {
  int memory_mismatches = 0;
  size_t memories_count = module->memories.size();
  for (size_t i = 0; i < memories_count; ++i) {
    int memory_index = module->memories[i].index;
    Tagged<WasmMemoryObject> memory =
        instance_data->memory_object(memory_index);
    Tagged<WasmMemoryObject> ref_memory =
        ref_instance_data->memory_object(memory_index);

    auto buffer = memory->array_buffer();
    auto ref_buffer = ref_memory->array_buffer();

    size_t memory_size = buffer->byte_length();
    size_t ref_memory_size = ref_buffer->byte_length();

    if (ref_memory_size != memory_size) {
      memory_mismatches++;

      if (print_difference) {
        std::ostringstream ss;
        ss << "Error: Memories at index " << i << " have different sizes!\n";
        ss << "  - Reference: " << ref_memory_size << " bytes\n;";
        ss << "  - Actual:    " << memory_size << " bytes" << std::endl;
        base::OS::PrintError("%s", ss.str().c_str());
      }
      continue;
    }

    uint8_t* data = static_cast<uint8_t*>(buffer->backing_store());
    uint8_t* ref_data = static_cast<uint8_t*>(ref_buffer->backing_store());

    if (memcmp(ref_data, data, memory_size) == 0) {
      continue;
    }

    memory_mismatches++;

    if (!print_difference) {
      continue;
    }

    constexpr int block_size = 16;

    auto print_mem_block = [](std::ostream& os, const uint8_t* buffer) {
      for (size_t k = 0; k < block_size; ++k) {
        os << std::setw(2) << std::setfill('0')
           << static_cast<uint32_t>(buffer[k]) << " ";
        if ((k + 1) % 4 == 0) {
          os << " ";
        }
      }
    };

    if (memory_size != 0) {
      CHECK_NOT_NULL(data);
      CHECK_NOT_NULL(ref_data);

      for (size_t j = 0; j < memory_size; j += block_size) {
        if (memcmp(ref_data + j, data + j, block_size) != 0) {
          std::ostringstream ss;
          ss << "Error: Memory difference found in range 0x" << std::hex << j
             << "-0x" << (j + block_size) << " (" << std::dec << j << "-"
             << (j + block_size) << ")!\n"
             << std::hex;

          ss << "  - Reference: ";
          print_mem_block(ss, ref_data + j);

          ss << "\n  - Actual:    ";
          print_mem_block(ss, data + j);

          ss << "\n               ";
          for (size_t k = 0; k < block_size; ++k) {
            if (ref_data[j + k] != data[j + k]) {
              ss << "^^ ";
            } else {
              ss << "   ";
            }
            if ((k + 1) % 4 == 0) {
              ss << " ";
            }
          }
          base::OS::PrintError("%s\n", ss.str().c_str());
        }
      }
    }
  }

  return memory_mismatches == 0;
}

bool TablesMatch(Isolate* isolate, const WasmModule* module,
                 Tagged<WasmTrustedInstanceData> instance_data,
                 Tagged<WasmTrustedInstanceData> ref_instance_data,
                 bool print_difference) {
  size_t tables_count = module->tables.size();

  Tagged<FixedArray> tables = instance_data->tables();
  Tagged<FixedArray> ref_tables = ref_instance_data->tables();

  int table_mismatches = 0;
  for (size_t i = 0; i < tables_count; ++i) {
    int table_index = static_cast<int>(i);
    Tagged<WasmTableObject> table =
        Cast<WasmTableObject>(tables->get(table_index));
    Tagged<WasmTableObject> ref_table =
        Cast<WasmTableObject>(ref_tables->get(table_index));

    int length = table->current_length();
    int ref_length = ref_table->current_length();

    if (ref_length != length) {
      table_mismatches++;
      if (print_difference) {
        std::ostringstream ss;
        ss << "Error: Tables at index " << i << " have different lengths!\n";
        ss << "  - Reference: " << ref_length << "\n";
        ss << "  - Actual:    " << length << std::endl;
        base::OS::PrintError("%s", ss.str().c_str());
      }
      continue;
    }

    Tagged<FixedArray> table_entries = table->entries();
    Tagged<FixedArray> ref_table_entries = ref_table->entries();

    for (int j = 0; j < length; ++j) {
      DirectHandle<Object> entry(table_entries->get(j), isolate);
      DirectHandle<Object> ref_entry(ref_table_entries->get(j), isolate);

      WasmValue value = WasmValue(entry, table->canonical_type(module));
      WasmValue ref_value =
          WasmValue(ref_entry, ref_table->canonical_type(module));

      if (!ValuesEquivalent(value, ref_value, isolate)) {
        table_mismatches++;

        if (print_difference) {
          std::ostringstream ss;
          ss << "Error: Table entries at index " << j << " of table " << i
             << " have different values!\n";
          ss << "  - Reference: ";
          PrintValue(ss, ref_value);
          ss << "\n  - Actual:    ";
          PrintValue(ss, value);
          base::OS::PrintError("%s\n", ss.str().c_str());
        }
      }
    }
  }

  return table_mismatches == 0;
}

int ExecuteAgainstReference(Isolate* isolate,
                            DirectHandle<WasmModuleObject> module_object,
                            int32_t max_executed_instructions
#if V8_ENABLE_DRUMBRAKE
                            ,
                            bool is_wasm_jitless
#endif  // V8_ENABLE_DRUMBRAKE
) {
  HandleScope handle_scope(isolate);

  NativeModule* native_module = module_object->native_module();
  const WasmModule* module = native_module->module();
  const base::Vector<const uint8_t> wire_bytes = native_module->wire_bytes();
  int exported_main = FindExportedMainFunction(module, wire_bytes);
  if (exported_main < 0) return -1;

  // We do not instantiate the module if there is a start function, because a
  // start function can contain an infinite loop which we cannot handle.
  if (module->start_function_index >= 0) return -1;

  ExecutionResult ref_result = ExecuteReferenceRun(
      isolate, wire_bytes, exported_main, max_executed_instructions);
  if (!ref_result.should_execute_non_reference) return -1;

#if V8_ENABLE_DRUMBRAKE
  if (is_wasm_jitless) {
    v8::internal::v8_flags.jitless = true;
    v8::internal::v8_flags.wasm_jitless = true;

    FlagList::EnforceFlagImplications();
    v8::internal::wasm::WasmInterpreterThread::Initialize();
    ClearJsToWasmWrappersForTesting(isolate);

    // Compiled WasmCode objects should be cleared before running drumbrake.
    isolate->heap()->CollectAllGarbage(GCFlag::kNoFlags,
                                       i::GarbageCollectionReason::kTesting);

    // The module should be validated when compiled for jitless mode.
    // But, we already compiled the module without jitless for the reference
    // instance. So, we run the validation here before running drumbrake.
    auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
    WasmDetectedFeatures unused_detected_features;
    ModuleDecoderImpl decoder(
        enabled_features, module_object->native_module()->wire_bytes(),
        ModuleOrigin::kWasmOrigin, &unused_detected_features);
    if (decoder.DecodeModule(/*validate_functions=*/true).failed()) return -1;
  }
#endif  // V8_ENABLE_DRUMBRAKE

  std::optional<ExecutionResult> result_opt =
      ExecuteNonReferenceRun(isolate, module, module_object, exported_main);
  if (!result_opt) {
    // The execution of non-reference run can fail if it runs OOM during
    // instantiation.
    return -1;
  }
  const ExecutionResult& result = *result_opt;

  // Also the second run can hit nondeterminism which was not hit before (when
  // growing memory). In that case, do not compare results.
  // TODO(384781857): Due to nondeterminism, the second run could even not
  // terminate. If this happens often enough we should do something about this.
  if (WasmEngine::clear_nondeterminism()) return -1;

  if ((ref_result.exception != nullptr) != (result.exception != nullptr) ||
      (ref_result.exception &&
       strcmp(ref_result.exception.get(), result.exception.get()) != 0)) {
    FATAL("Exception mismatch! Expected: <%s>; got: <%s>",
          ref_result.exception ? ref_result.exception.get() : "<no exception>",
          result.exception ? result.exception.get() : "<no exception>");
  }

  if (!result.exception) {
    CHECK_EQ(ref_result.result, result.result);
  }

  bool globals_match;
  bool memories_match;
  bool tables_match;
  {
    DisallowGarbageCollection no_gc;

    Tagged<WasmTrustedInstanceData> instance_data =
        result.instance->trusted_data(isolate);
    Tagged<WasmTrustedInstanceData> ref_instance_data =
        ref_result.instance->trusted_data(isolate);

    globals_match =
        GlobalsMatch(isolate, module, instance_data, ref_instance_data, true);
    memories_match =
        MemoriesMatch(isolate, module, instance_data, ref_instance_data, true);
    tables_match =
        TablesMatch(isolate, module, instance_data, ref_instance_data, true);
    if (globals_match && memories_match && tables_match) {
      return 0;
    }

    if (!tables_match && (globals_match && memories_match)) {
      FATAL(
          "\nMismatch detected in tables! Tracing for tables is not "
          "implemented "
          "yet.");
    }

    base::OS::PrintError(
        "\nMismatch detected in global variables, memories or tables - "
        "rerunning "
        "with tracing enabled.\n");
  }

  bool should_trace_globals = !globals_match;
  bool should_trace_memory = !memories_match;

  // Disable module cache so that the module is actually recompiled and the
  // runtime calls for tracing are generated.
  FlagScope<bool> no_native_module_cache(&v8_flags.wasm_native_module_cache,
                                         false);
  FlagScope<bool> trace_globals_scope(&v8_flags.trace_wasm_globals,
                                      should_trace_globals);
  FlagScope<bool> trace_memory_scope(&v8_flags.trace_wasm_memory,
                                     should_trace_memory);

  wasm::WasmTracesForTesting& traces = GetWasmTracesForTesting();
  // This flag makes sure the runtime functions store the traces instead of
  // printing them to stdout.
  traces.should_store_trace = true;

  if (isolate->has_exception()) isolate->clear_exception();
  if (WasmEngine::clear_nondeterminism()) return -1;

  ExecutionResult ref_result_traced = ExecuteReferenceRun(
      isolate, wire_bytes, exported_main, max_executed_instructions);
  CHECK(ref_result_traced.should_execute_non_reference);

  // Copy the traces from the reference run to preserve them.
  wasm::MemoryTrace ref_memory_trace = std::move(traces.memory_trace);
  wasm::GlobalTrace ref_global_trace = std::move(traces.global_trace);
  // Reset the vectors to store the traces for the actual run.
  traces.memory_trace.clear();
  traces.global_trace.clear();

  DirectHandle<WasmModuleObject> module_object_traced;

  {
    ErrorThrower thrower(isolate, "ExecuteAgainstReference (traced compile)");
    auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
    MaybeDirectHandle<WasmModuleObject> maybe_module =
        GetWasmEngine()->SyncCompile(isolate, enabled_features,
                                     CompileTimeImportsForFuzzing(), &thrower,
                                     base::OwnedCopyOf(wire_bytes));
    module_object_traced = maybe_module.ToHandleChecked();
    CHECK(!thrower.error());
  }

  std::optional<ExecutionResult> result_traced_opt = ExecuteNonReferenceRun(
      isolate, module, module_object_traced, exported_main);
  CHECK(result_traced_opt);
  const ExecutionResult& result_traced = *result_traced_opt;

  Tagged<WasmTrustedInstanceData> instance_data_traced =
      result_traced.instance->trusted_data(isolate);
  Tagged<WasmTrustedInstanceData> ref_instance_data_traced =
      ref_result_traced.instance->trusted_data(isolate);

  bool globals_match_traced = GlobalsMatch(
      isolate, module, instance_data_traced, ref_instance_data_traced, false);
  bool memories_match_traced = MemoriesMatch(
      isolate, module, instance_data_traced, ref_instance_data_traced, false);

  if (globals_match_traced && memories_match_traced) {
    FATAL("Mismatch disappeared when re-running with tracing enabled.");
  }

  wasm::MemoryTrace memory_trace = std::move(traces.memory_trace);
  wasm::GlobalTrace global_trace = std::move(traces.global_trace);

  if (should_trace_memory) {
    std::ostringstream ss;
    ss << "\nMemory trace.\n";
    CompareAndPrintMemoryTraces(memory_trace, ref_memory_trace, native_module,
                                ss);
    base::OS::PrintError("%s", ss.str().c_str());
  }

  if (should_trace_globals) {
    std::ostringstream ss;
    ss << "\nGlobal trace.\n";
    CompareAndPrintGlobalTraces(global_trace, ref_global_trace, native_module,
                                ss);
    base::OS::PrintError("%s", ss.str().c_str());
  }

  if (!tables_match) {
    base::OS::PrintError("\nTracing for tables is not implemented yet.");
  }

  FATAL("Execution mismatch. Tracing has been printed.");
}

void GenerateTestCase(Isolate* isolate, ModuleWireBytes wire_bytes,
                      bool compiles) {
  StdoutStream os;
  GenerateTestCase(os, isolate, wire_bytes, compiles, false, "");
  os.flush();
}

void GenerateTestCase(StdoutStream& os, Isolate* isolate,
                      ModuleWireBytes wire_bytes, bool compiles,
                      bool emit_call_main, std::string_view extra_flags) {
  // Libfuzzer sometimes runs a test twice (for detecting memory leaks), and in
  // this case we do not want multiple outputs by this function.
  // Similarly if we explicitly execute the same test multiple times (via
  // `-runs=N`).
  static std::atomic<bool> did_output_before{false};
  if (did_output_before.exchange(true)) return;

  constexpr bool kVerifyFunctions = false;
  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
  WasmDetectedFeatures unused_detected_features;
  ModuleResult module_res = DecodeWasmModule(
      enabled_features, wire_bytes.module_bytes(), kVerifyFunctions,
      ModuleOrigin::kWasmOrigin, &unused_detected_features);
  CHECK_WITH_MSG(module_res.ok(), module_res.error().message().c_str());
  WasmModule* module = module_res.value().get();
  CHECK_NOT_NULL(module);

  AccountingAllocator allocator;
  Zone zone(&allocator, "constant expression zone");

  MultiLineStringBuilder out;
  NamesProvider names(module, wire_bytes.module_bytes());
  MjsunitModuleDis disassembler(out, module, &names, wire_bytes, &allocator,
                                !compiles);
  disassembler.PrintModule(extra_flags, emit_call_main);
  const bool offsets = false;  // Not supported by MjsunitModuleDis.
  out.WriteTo(os, offsets);
}

namespace {
static uint8_t kDummyModuleWireBytes[]{
    WASM_MODULE_HEADER,
    SECTION(Type, ENTRY_COUNT(2),
            // recgroup of 2 types
            WASM_REC_GROUP(ENTRY_COUNT(2),
                           // (array (field (mut f32)))
                           WASM_ARRAY_DEF(kF32Code, true),
                           // (struct (field i64) (field externref))
                           WASM_NONFINAL(WASM_STRUCT_DEF(
                               FIELD_COUNT(2), STRUCT_FIELD(kI64Code, false),
                               STRUCT_FIELD(kExternRefCode, false)))),
            // function type (void -> i32)
            SIG_ENTRY_x(kI32Code))};

}  // namespace

void AddDummyTypesToTypeCanonicalizer(Isolate* isolate) {
  const size_t type_count = GetTypeCanonicalizer()->GetCurrentNumberOfTypes();
  const bool is_valid = GetWasmEngine()->SyncValidate(
      isolate, WasmEnabledFeatures(), CompileTimeImportsForFuzzing(),
      base::VectorOf(kDummyModuleWireBytes));
  CHECK(is_valid);
  // As the types are reset on each run by the fuzzer, the validation should
  // have added new types to the TypeCanonicalizer.
  CHECK_GT(GetTypeCanonicalizer()->GetCurrentNumberOfTypes(), type_count);
}

void EnableExperimentalWasmFeatures(v8::Isolate* isolate) {
  struct EnableExperimentalWasmFeatures {
    explicit EnableExperimentalWasmFeatures(v8::Isolate* isolate)
        : isolate(isolate) {
      // Enable all staged features.
#define ENABLE_PRE_STAGED_AND_STAGED_FEATURES(feat, ...) \
  v8_flags.experimental_wasm_##feat = true;
      FOREACH_WASM_PRE_STAGING_FEATURE_FLAG(
          ENABLE_PRE_STAGED_AND_STAGED_FEATURES)
      FOREACH_WASM_STAGING_FEATURE_FLAG(ENABLE_PRE_STAGED_AND_STAGED_FEATURES)
#undef ENABLE_PRE_STAGED_AND_STAGED_FEATURES

      // Enable non-staged experimental features or other experimental flags
      // that we also want to fuzz, e.g., new optimizations.
      // Note: If you add a Wasm feature here, you will also have to add the
      // respective flag(s) to the mjsunit/wasm/generate-random-module.js test,
      // otherwise that fails on an unsupported feature.
      // You may also want to add the flag(s) to the JS file header in
      // `PrintModule()` of `mjsunit-module-disassembler-impl.h`, to make bugs
      // easier to reproduce with generated mjsunit test cases.

      // The "pure Wasm" part of this proposal is considered ready for
      // fuzzing, the JS-related part (prototypes etc) not yet.
      v8_flags.experimental_wasm_custom_descriptors = true;

#ifdef V8_ENABLE_WASM_SIMD256_REVEC
      // Fuzz revectorization, which is otherwise still considered experimental.
      v8_flags.experimental_wasm_revectorize = true;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC

      // Enforce implications from enabling features.
      FlagList::EnforceFlagImplications();

      // Last, install any conditional features. Implications are handled
      // implicitly.
      isolate->InstallConditionalFeatures(isolate->GetCurrentContext());
    }

    const v8::Isolate* const isolate;
  };

  // Call the constructor exactly once (per process!). The compiler will
  // properly synchronize this.
  static EnableExperimentalWasmFeatures one_time_enable_experimental_features(
      isolate);
  // Ensure that within the same process we always pass the same isolate. You
  // would get surprising results otherwise.
  CHECK_EQ(one_time_enable_experimental_features.isolate, isolate);
}

void ResetTypeCanonicalizer(v8::Isolate* isolate) {
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Make sure that there are no NativeModules left referencing the canonical
  // types. Collecting NativeModules can require two rounds of GC.
  for (int i = 0; i < 2 && GetWasmEngine()->NativeModuleCount() != 0; i++) {
    // We need to invoke GC without stack, otherwise the native module may
    // survive.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        i_isolate->heap());
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }
  GetTypeCanonicalizer()->EmptyStorageForTesting();
  TypeCanonicalizer::ClearWasmCanonicalTypesForTesting(i_isolate);
  AddDummyTypesToTypeCanonicalizer(i_isolate);
}

int WasmExecutionFuzzer::FuzzWasmModule(base::Vector<const uint8_t> data,
                                        bool require_valid) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  // Strictly enforce the input size limit. Note that setting "max_len" on the
  // fuzzer target is not enough, since different fuzzers are used and not all
  // respect that limit.
  if (data.size() > max_input_size()) return -1;

  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  EnableExperimentalWasmFeatures(isolate);

  // Allow mixed old and new EH instructions in the same module for fuzzing, to
  // help us test the interaction between the two EH proposals without requiring
  // multiple modules.
  v8_flags.wasm_allow_mixed_eh_for_testing = true;

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  // The first byte specifies some internal configuration, like which function
  // is compiled with which compiler, and other flags.
  uint8_t configuration_byte = data.empty() ? 0 : data[0];
  if (!data.empty()) data += 1;

  // Derive the compiler configuration for the first four functions from the
  // configuration byte, to choose for each function between:
  // 0: TurboFan
  // 1: Liftoff
  // 2: Liftoff for debugging
  uint8_t tier_mask = 0;
  uint8_t debug_mask = 0;
  for (int i = 0; i < 4; ++i, configuration_byte /= 3) {
    int compiler_config = configuration_byte % 3;
    tier_mask |= (compiler_config == 0) << i;
    debug_mask |= (compiler_config == 2) << i;
  }
  // The purpose of setting the tier mask (which affects the initial
  // compilation of each function) is to deterministically test a combination
  // of Liftoff and Turbofan.
  FlagScope<int> tier_mask_scope(&v8_flags.wasm_tier_mask_for_testing,
                                 tier_mask);
  FlagScope<int> debug_mask_scope(&v8_flags.wasm_debug_mask_for_testing,
                                  debug_mask);

  ZoneBuffer buffer(&zone);
  if (!GenerateModule(i_isolate, &zone, data, &buffer)) {
    return -1;
  }

  return SyncCompileAndExecuteAgainstReference(isolate, base::VectorOf(buffer),
                                               require_valid);
}

int SyncCompileAndExecuteAgainstReference(
    v8::Isolate* isolate, base::Vector<const uint8_t> wire_bytes,
    bool require_valid) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  // Clear recursive groups: The fuzzer creates random types in every run. These
  // are saved as recursive groups as part of the type canonicalizer, but types
  // from previous runs just waste memory.
  ResetTypeCanonicalizer(isolate);

  // Clear any exceptions from a prior run.
  if (i_isolate->has_exception()) i_isolate->clear_exception();

  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);

  bool valid = GetWasmEngine()->SyncValidate(
      i_isolate, enabled_features, CompileTimeImportsForFuzzing(), wire_bytes);

  if (v8_flags.wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, ModuleWireBytes{wire_bytes}, valid);
  }

  FlagScope<bool> eager_compile(&v8_flags.wasm_lazy_compilation, false);
  // We want to keep dynamic tiering enabled because that changes the code
  // Liftoff generates as well as optimizing compilers' behavior (especially
  // around inlining). We switch it to synchronous mode to avoid the
  // nondeterminism of background jobs finishing at random times.
  FlagScope<bool> sync_tier_up(&v8_flags.wasm_sync_tier_up, true);
  // Reference runs use extra compile settings (like non-determinism detection),
  // which could be replaced by new liftoff code without this option.
  FlagScope<bool> no_liftoff_code_flushing(&v8_flags.flush_liftoff_code, false);

  ErrorThrower thrower(i_isolate, "WasmFuzzerSyncCompile");
  MaybeDirectHandle<WasmModuleObject> compiled_module =
      GetWasmEngine()->SyncCompile(i_isolate, enabled_features,
                                   CompileTimeImportsForFuzzing(), &thrower,
                                   base::OwnedCopyOf(wire_bytes));
  CHECK_EQ(valid, !compiled_module.is_null());
  CHECK_EQ(!valid, thrower.error());
  if (require_valid && !valid) {
    FATAL("Generated module should validate, but got: %s", thrower.error_msg());
  }
  thrower.Reset();

  // Do not execute invalid modules, and return `-1` to avoid adding them to the
  // corpus. Even though invalid modules are also somewhat interesting to fuzz,
  // we will get them often enough via mutations, so we do not add them to the
  // corpus.
  if (!valid) return -1;

  return ExecuteAgainstReference(i_isolate, compiled_module.ToHandleChecked(),
                                 kDefaultMaxFuzzerExecutedInstructions);
}

}  // namespace v8::internal::wasm::fuzzing
