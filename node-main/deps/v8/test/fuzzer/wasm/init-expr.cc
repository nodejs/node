// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "src/base/vector.h"
#include "src/execution/isolate.h"
#include "src/objects/property-descriptor.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/fuzzing/random-module-generation.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

// This fuzzer fuzzes initializer expressions used e.g. in globals.
// The fuzzer creates a set of globals with initializer expressions and a set of
// functions containing the same body as these initializer expressions.
// The global value should be equal to the result of running the corresponding
// function.

namespace v8::internal::wasm::fuzzing {

#define CHECK_FLOAT_EQ(expected, actual)                    \
  if (std::isnan(expected)) {                               \
    CHECK(std::isnan(actual));                              \
  } else {                                                  \
    CHECK_EQ(expected, actual);                             \
    CHECK_EQ(std::signbit(expected), std::signbit(actual)); \
  }

namespace {
bool IsNullOrWasmNull(Tagged<Object> obj) {
  return IsNull(obj) || IsWasmNull(obj);
}

DirectHandle<Object> GetExport(Isolate* isolate,
                               DirectHandle<WasmInstanceObject> instance,
                               const char* name) {
  DirectHandle<JSObject> exports_object;
  DirectHandle<Name> exports =
      isolate->factory()->InternalizeUtf8String("exports");
  exports_object = Cast<JSObject>(
      JSObject::GetProperty(isolate, instance, exports).ToHandleChecked());

  DirectHandle<Name> main_name =
      isolate->factory()->NewStringFromAsciiChecked(name);
  PropertyDescriptor desc;
  Maybe<bool> property_found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, exports_object, main_name, &desc);
  CHECK(property_found.FromMaybe(false));
  return desc.value();
}

void FuzzIt(base::Vector<const uint8_t> data) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  v8::Isolate::Scope isolate_scope(isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // Disable the optimizing compiler. The init expressions can be huge and might
  // produce long compilation times. The function is only used as a reference
  // and only run once, so use liftoff only as it allows much faster fuzzing.
  v8_flags.liftoff_only = true;

  // We explicitly enable staged WebAssembly features here to increase fuzzer
  // coverage. For libfuzzer fuzzers it is not possible that the fuzzer enables
  // the flag by itself.
  EnableExperimentalWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  HandleScope scope(i_isolate);

  // Clear recursive groups: The fuzzer creates random types in every run. These
  // are saved as recursive groups as part of the type canonicalizer, but types
  // from previous runs just waste memory.
  ResetTypeCanonicalizer(isolate);

  size_t expression_count = 0;
  base::Vector<const uint8_t> bytes =
      GenerateWasmModuleForInitExpressions(&zone, data, &expression_count);

  ModuleWireBytes wire_bytes(bytes.begin(), bytes.end());
  auto enabled_features = WasmEnabledFeatures::FromIsolate(i_isolate);
  bool valid = GetWasmEngine()->SyncValidate(i_isolate, enabled_features,
                                             CompileTimeImportsForFuzzing(),
                                             wire_bytes.module_bytes());

  if (v8_flags.wasm_fuzzer_gen_test) {
    GenerateTestCase(i_isolate, wire_bytes, valid);
  }

  CHECK(valid);
  FlagScope<bool> eager_compile(&v8_flags.wasm_lazy_compilation, false);
  ErrorThrower thrower(i_isolate, "WasmFuzzerSyncCompile");
  MaybeDirectHandle<WasmModuleObject> compiled_module =
      GetWasmEngine()->SyncCompile(i_isolate, enabled_features,
                                   CompileTimeImportsForFuzzing(), &thrower,
                                   base::OwnedCopyOf(bytes));
  CHECK(!compiled_module.is_null());
  CHECK(!thrower.error());
  thrower.Reset();
  CHECK(!i_isolate->has_exception());

  DirectHandle<WasmModuleObject> module_object =
      compiled_module.ToHandleChecked();
  const WasmModule* module = module_object->native_module()->module();
  DirectHandle<WasmInstanceObject> instance =
      GetWasmEngine()
          ->SyncInstantiate(i_isolate, &thrower, module_object, {}, {})
          .ToHandleChecked();
  CHECK_EQ(expression_count, module->num_declared_functions);

  for (size_t i = 0; i < expression_count; ++i) {
    char buffer[22];
    snprintf(buffer, sizeof buffer, "f%zu", i);
    // Execute corresponding function.
    auto function =
        Cast<WasmExportedFunction>(GetExport(i_isolate, instance, buffer));
    DirectHandle<Object> undefined = i_isolate->factory()->undefined_value();
    DirectHandle<Object> function_result =
        Execution::Call(i_isolate, function, undefined, {}).ToHandleChecked();
    // Get global value.
    snprintf(buffer, sizeof buffer, "g%zu", i);
    auto global =
        Cast<WasmGlobalObject>(GetExport(i_isolate, instance, buffer));
    switch (global->type().kind()) {
      case ValueKind::kF32: {
        float global_val = global->GetF32();
        float func_val;
        if (IsSmi(*function_result)) {
          func_val = Smi::ToInt(*function_result);
        } else {
          CHECK(IsHeapNumber(*function_result));
          func_val = Cast<HeapNumber>(*function_result)->value();
        }
        CHECK_FLOAT_EQ(func_val, global_val);
        break;
      }
      case ValueKind::kF64: {
        double global_val = global->GetF64();
        double func_val;
        if (IsSmi(*function_result)) {
          func_val = Smi::ToInt(*function_result);
        } else {
          CHECK(IsHeapNumber(*function_result));
          func_val = Cast<HeapNumber>(*function_result)->value();
        }
        CHECK_FLOAT_EQ(func_val, global_val);
        break;
      }
      case ValueKind::kI32: {
        int32_t global_val = global->GetI32();
        int32_t func_val;
        if (IsSmi(*function_result)) {
          func_val = Smi::ToInt(*function_result);
        } else {
          CHECK(IsHeapNumber(*function_result));
          func_val = Cast<HeapNumber>(*function_result)->value();
        }
        CHECK_EQ(func_val, global_val);
        break;
      }
      case ValueKind::kI64: {
        int64_t global_val = global->GetI64();
        int64_t func_val;
        if (IsSmi(*function_result)) {
          func_val = Smi::ToInt(*function_result);
        } else {
          CHECK(IsBigInt(*function_result));
          bool lossless;
          func_val = Cast<BigInt>(*function_result)->AsInt64(&lossless);
          CHECK(lossless);
        }
        CHECK_EQ(func_val, global_val);
        break;
      }
      case ValueKind::kRef:
      case ValueKind::kRefNull: {
        // For reference types the expectations are more limited.
        // Any struct.new would create a new object, so reference equality
        // comparisons will not work.
        DirectHandle<Object> global_val = global->GetRef();
        CHECK_EQ(IsUndefined(*global_val), IsUndefined(*function_result));
        CHECK_EQ(IsNullOrWasmNull(*global_val),
                 IsNullOrWasmNull(*function_result));
        if (!IsNullOrWasmNull(*global_val)) {
          if (IsSubtypeOf(global->type(), kWasmFuncRef, module)) {
            // For any function the global should be an internal function
            // whose external function equals the call result. (The call goes
            // through JS conversions while the global is accessed directly.)
            CHECK(IsWasmFuncRef(*global_val));
            CHECK(
                WasmExportedFunction::IsWasmExportedFunction(*function_result));
            CHECK(*WasmInternalFunction::GetOrCreateExternal(direct_handle(
                      Cast<WasmFuncRef>(*global_val)->internal(i_isolate),
                      i_isolate)) == *function_result);
          } else {
            // On arrays and structs, perform a deep comparison.
            DisallowGarbageCollection no_gc;
            WasmValue global_value =
                instance->trusted_data(i_isolate)->GetGlobalValue(
                    i_isolate, instance->module()->globals[i]);
            WasmValue func_value(function_result, global_value.type());
            if (!ValuesEquivalent(global_value, func_value, i_isolate)) {
              std::stringstream str;
              str << "Equality check failed for global #" << i
                  << ". Global value: ";
              PrintValue(str, global_value);
              str << ", Function value:   ";
              PrintValue(str, func_value);

              FATAL("%s\n", str.str().c_str());
            }
          }
        }
        break;
      }
      default:
        UNIMPLEMENTED();
    }
  }
}

}  // anonymous namespace

V8_SYMBOL_USED extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  v8_fuzzer::FuzzerSupport::InitializeFuzzerSupport(argc, argv);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzIt({data, size});
  return 0;
}

}  // namespace v8::internal::wasm::fuzzing
