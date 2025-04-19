// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/fuzzer/wasm/interpreter/interpreter-fuzzer-common.h"

#include <ctime>

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-metrics.h"
#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/utils/ostreams.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/interpreter/wasm-interpreter-runtime.h"
#include "src/wasm/interpreter/wasm-interpreter.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-feature-flags.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"
#include "test/fuzzer/wasm/fuzzer-common.h"

namespace v8::internal::wasm::fuzzing {

std::mt19937_64 MersenneTwister(const char* data, size_t size) {
  std::mt19937_64 rng;
  std::string str = std::string(data, size);
  std::size_t hash = std::hash<std::string>()(str);
  rng.seed(hash);
  return rng;
}

class WasmInterpretationResult {
 public:
  static WasmInterpretationResult Failed() { return {kFailed, 0, false}; }
  static WasmInterpretationResult Trapped(bool possible_nondeterminism) {
    return {kTrapped, 0, possible_nondeterminism};
  }
  static WasmInterpretationResult Finished(int32_t result,
                                           bool possible_nondeterminism) {
    return {kFinished, result, possible_nondeterminism};
  }

  // {failed()} captures different reasons: The module was invalid, no function
  // to call was found in the module, the function did not terminate within a
  // limited number of steps, or a stack overflow happened.
  bool failed() const { return status_ == kFailed; }
  bool trapped() const { return status_ == kTrapped; }
  bool finished() const { return status_ == kFinished; }

  int32_t result() const {
    DCHECK_EQ(status_, kFinished);
    return result_;
  }

  bool possible_nondeterminism() const { return possible_nondeterminism_; }

 private:
  enum Status { kFinished, kTrapped, kFailed };

  const Status status_;
  const int32_t result_;
  const bool possible_nondeterminism_;

  WasmInterpretationResult(Status status, int32_t result,
                           bool possible_nondeterminism)
      : status_(status),
        result_(result),
        possible_nondeterminism_(possible_nondeterminism) {}
};

// Interprets the given module, starting at the function specified by
// {function_index}. The return type of the function has to be int32. The module
// should not have any imports or exports.
WasmInterpretationResult FastInterpretWasmModule(
    Isolate* isolate, DirectHandle<WasmInstanceObject> instance,
    int32_t function_index, const std::vector<WasmValue>& args,
    std::vector<WasmValue>& rets) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  v8::internal::HandleScope scope(isolate);
  const WasmFunction* func = &instance->module()->functions[function_index];

  CHECK(func->exported);
  // This would normally be handled by export wrappers.
  if (!IsJSCompatibleSignature(
          reinterpret_cast<const wasm::CanonicalSig*>(func->sig))) {
    return WasmInterpretationResult::Trapped(false);
  }

  CHECK(v8_flags.wasm_jitless);
  isolate->clear_exception();

  wasm::WasmInterpreterThread* thread =
      wasm::WasmInterpreterThread::GetCurrentInterpreterThread(isolate);

  DirectHandle<Tuple2> interpreter_object =
      v8::internal::WasmTrustedInstanceData::GetOrCreateInterpreterObject(
          instance);

  int* thread_in_wasm_code = trap_handler::GetThreadInWasmThreadLocalAddress();
  *thread_in_wasm_code = true;
  // Assume an instance can run in only one thread.
  wasm::InterpreterHandle* handle =
      wasm::GetOrCreateInterpreterHandle(isolate, interpreter_object);

  for (const WasmValue& arg : args) {
    if (arg.type().is_reference()) {
      // We should pass WasmNull as null argument, not a JS null value.
      CHECK(!IsNull(*arg.to_ref(), isolate));
    }
  }
  bool success = handle->wasm::InterpreterHandle::Execute(
      thread, 0, static_cast<uint32_t>(function_index), args, rets);
  *thread_in_wasm_code = false;

  // Returned values should not be the hole value.
  if (success) {
    for (auto& wasm_value : rets) {
      if (wasm_value.type().is_reference())
        CHECK(!IsTheHole(*wasm_value.to_ref()));
    }
  }

  return success ? WasmInterpretationResult::Finished(0, false)
                 : WasmInterpretationResult::Failed();
}

Handle<JSObject> GetOrCreateModuleNamespaceObject(Isolate* isolate,
                                                  Handle<JSObject> ffi_object,
                                                  Handle<String> module_name) {
  PropertyDescriptor desc;
  Maybe<bool> property_found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, ffi_object, module_name, &desc);
  if (property_found.FromMaybe(false)) {
    return Cast<JSObject>(desc.value());
  }

  Handle<JSObject> module_namespace =
      isolate->factory()->NewJSObject(isolate->object_function());
  JSObject::DefinePropertyOrElementIgnoreAttributes(ffi_object, module_name,
                                                    module_namespace)
      .Assert();
  return module_namespace;
}

Handle<JSFunction> GenerateJSFunction(Isolate* isolate) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::String> fn_body =
      v8::String::NewFromUtf8(v8_isolate, "return arguments[1] + 0x414141;")
          .ToLocalChecked();
  v8::ScriptCompiler::Source script_source(fn_body);
  v8::Local<v8::Function> function =
      v8::ScriptCompiler::CompileFunction(v8_isolate->GetCurrentContext(),
                                          &script_source)
          .ToLocalChecked();

  return Cast<JSFunction>(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(function)));
}

MaybeDirectHandle<WasmTableObject> GenerateWasmTable(
    Isolate* isolate, DirectHandle<WasmModuleObject> module_object,
    uint32_t table_index) {
  const WasmTable& table = module_object->module()->tables[table_index];

  uint32_t table_initial = 10;
  uint32_t table_maximum = 30;
  bool has_maximum = true;

  if (table.type == wasm::kWasmAnyRef) {
    DirectHandle<WasmTableObject> table_obj = WasmTableObject::New(
        isolate, Handle<WasmTrustedInstanceData>(), wasm::kWasmAnyRef,
        wasm::CanonicalValueType{wasm::kWasmAnyRef}, table_initial, has_maximum,
        table_maximum, isolate->factory()->undefined_value(),
        wasm::AddressType::kI32);
    // Anyref (externref) Table
    Handle<FixedArray> externref_backing_store{table_obj->entries(), isolate};
    externref_backing_store->set(0, Smi::FromInt(0x414141));
    externref_backing_store->set(
        1, *isolate->factory()->NewJSObject(isolate->object_function()));
    return table_obj;
  } else if (table.type == wasm::kWasmFuncRef) {
    return WasmTableObject::New(
        isolate, Handle<WasmTrustedInstanceData>(), wasm::kWasmFuncRef,
        wasm::CanonicalValueType{wasm::kWasmFuncRef}, table_initial,
        has_maximum, table_maximum, isolate->factory()->null_value(),
        wasm::AddressType::kI32);
  }
  // Not supported type
  return {};
}

Handle<String> ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, base::Vector<const uint8_t> wire_bytes,
    wasm::WireBytesRef ref) {
  base::Vector<const uint8_t> name_vec =
      wire_bytes.SubVector(ref.offset(), ref.end_offset());
  return isolate->factory()->InternalizeUtf8String(
      base::Vector<const char>::cast(name_vec));
}

DirectHandle<JSObject> CreateImportObject(
    Isolate* isolate, DirectHandle<WasmModuleObject> module_object) {
  Handle<JSObject> ffi_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  base::Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  for (size_t index = 0; index < module_object->module()->import_table.size();
       ++index) {
    const WasmImport& import = module_object->module()->import_table[index];

    Handle<String> module_name = ExtractUtf8StringFromModuleBytes(
        isolate, wire_bytes, import.module_name);

    Handle<String> field_name = ExtractUtf8StringFromModuleBytes(
        isolate, wire_bytes, import.field_name);

    Handle<JSObject> module_namespace =
        GetOrCreateModuleNamespaceObject(isolate, ffi_object, module_name);

    switch (import.kind) {
      case kExternalFunction: {
        // TODO: Support other types of external functions such as wasm?
        // It currently supports only external JS function.
        Handle<JSFunction> fn_obj = GenerateJSFunction(isolate);
        JSObject::DefinePropertyOrElementIgnoreAttributes(module_namespace,
                                                          field_name, fn_obj)
            .Assert();
        break;
      }
      case kExternalTable: {
        uint32_t table_index = import.index;
        DirectHandle<WasmTableObject> table_obj;
        if (GenerateWasmTable(isolate, module_object, table_index)
                .ToHandle(&table_obj)) {
          JSObject::DefinePropertyOrElementIgnoreAttributes(
              module_namespace, field_name, table_obj)
              .Assert();
        }
        break;
      }
      case kExternalMemory: {
        // Memory
        SharedFlag shared = SharedFlag::kNotShared;
        int memory_initial = 1;
        int memory_maximum = 32;
        DirectHandle<WasmMemoryObject> memory_obj;
        if (WasmMemoryObject::New(isolate, memory_initial, memory_maximum,
                                  shared, wasm::AddressType::kI32)
                .ToHandle(&memory_obj)) {
          JSObject::DefinePropertyOrElementIgnoreAttributes(
              module_namespace, field_name, memory_obj)
              .Assert();
        }
        break;
      }
      case kExternalGlobal: {
        // Global
        const uint32_t offset = 0;
        const WasmGlobal& global =
            module_object->module()->globals[import.index];
        DirectHandle<WasmTrustedInstanceData> trusted_data =
            WasmTrustedInstanceData::New(isolate, module_object, false);
        MaybeDirectHandle<WasmGlobalObject> maybe_global_obj =
            WasmGlobalObject::New(isolate, trusted_data,
                                  MaybeHandle<JSArrayBuffer>(),
                                  MaybeHandle<FixedArray>(), global.type,
                                  offset, global.mutability);
        DirectHandle<WasmGlobalObject> global_obj;
        if (maybe_global_obj.ToHandle(&global_obj)) {
          JSObject::DefinePropertyOrElementIgnoreAttributes(
              module_namespace, field_name, global_obj)
              .Assert();
        }
        break;
      }
      case kExternalTag: {
        // Not supported yet.
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  return ffi_object;
}

bool InstantiateModule(Isolate* isolate,
                       DirectHandle<WasmModuleObject> module_object,
                       DirectHandle<internal::JSObject> imports_obj,
                       DirectHandle<WasmInstanceObject>* instance) {
  ErrorThrower thrower(isolate, "WebAssembly Instantiation");

  if (!GetWasmEngine()
           ->SyncInstantiate(isolate, &thrower, module_object, imports_obj, {})
           .ToHandle(instance)) {
    isolate->clear_exception();
    thrower.Reset();  // Ignore errors.
    return false;
  }
  return true;
}

// Generate an array of default arguments for the given signature, to be used in
// the interpreter.
std::vector<WasmValue> FastMakeDefaultInterpreterArguments(
    Isolate* isolate, const WasmModule* module, const FunctionSig* sig,
    std::mt19937_64 mt_generator_64) {
  size_t param_count = sig->parameter_count();
  std::vector<WasmValue> arguments(param_count);
  int64_t rand_num = 0;

  for (size_t i = 0; i < param_count; ++i) {
    rand_num = mt_generator_64();
    switch (sig->GetParam(i).kind()) {
      case kI32:
        arguments[i] = WasmValue(static_cast<int32_t>(rand_num));
        break;
      case kI64:
        arguments[i] = WasmValue(rand_num);
        break;
      case kF32:
        arguments[i] = WasmValue(static_cast<float>(rand_num));
        break;
      case kF64:
        arguments[i] = WasmValue(static_cast<double>(rand_num));
        break;
      case kRefNull: {
        ValueType type = sig->GetParam(i);
        wasm::CanonicalValueType canonical_type = wasm::kWasmBottom;
        if (type.has_index()) {
          canonical_type =
              type.Canonicalize(module->canonical_type_id(type.ref_index()));
        } else {
          canonical_type = CanonicalValueType{type};
        }

        if (type.heap_representation() == HeapType::kExtern) {
          arguments[i] = WasmValue(
              Cast<Object>(isolate->factory()->NewHeapNumber(rand_num + 0.125)),
              canonical_type);
        } else {
          arguments[i] =
              WasmValue(isolate->factory()->wasm_null(), canonical_type);
        }
        break;
      }
      case kS128: {
        int64_t rand_num2 = mt_generator_64();
        const int64_t simd_value[2] = {rand_num, rand_num2};
        arguments[i] = WasmValue(reinterpret_cast<const uint8_t*>(simd_value),
                                 (CanonicalValueType)sig->GetParam(i));
        break;
      }
      case kRef:
      case kI8:
      case kI16:
      case kF16:
      case kVoid:
      case kTop:
      case kBottom:
        UNREACHABLE();
    }
  }

  return arguments;
}

void RunInstance(Isolate* isolate, std::mt19937_64 rand_generator,
                 DirectHandle<WasmModuleObject> module,
                 DirectHandle<WasmInstanceObject> instance) {
  size_t num_exports = instance->module()->export_table.size();
  for (size_t i = 0; i < num_exports; ++i) {
    WasmExport exp = instance->module()->export_table[i];

    if (exp.kind != kExternalFunction) continue;
    WasmFunction wfn = instance->module()->functions[exp.index];

    std::vector<WasmValue> arguments = FastMakeDefaultInterpreterArguments(
        isolate, instance->module(), wfn.sig, rand_generator);
    std::vector<WasmValue> retvals(wfn.sig->return_count());

    FastInterpretWasmModule(isolate, instance, wfn.func_index, arguments,
                            retvals);

    isolate->clear_exception();
  }  // end handle loop
}

int FastInterpretAndExecuteModule(Isolate* isolate,
                                  DirectHandle<WasmModuleObject> module_object,
                                  std::mt19937_64 rand_generator) {
  // We do not instantiate the module if there is a start function, because a
  // start function can contain an infinite loop which we cannot handle.
  if (module_object->module()->start_function_index >= 0) return -1;

  HandleScope handle_scope(isolate);  // Avoid leaking handles.

  DirectHandle<JSObject> imports_obj =
      CreateImportObject(isolate, module_object);

  DirectHandle<WasmInstanceObject> other_instance;
  if (!InstantiateModule(isolate, module_object, imports_obj, &other_instance))
    return -1;

  DirectHandle<WasmInstanceObject> instance;
  if (!InstantiateModule(isolate, module_object, imports_obj, &instance))
    return -1;

  RunInstance(isolate, rand_generator, module_object, other_instance);
  RunInstance(isolate, rand_generator, module_object, instance);
  return 0;
}

void InitializeDrumbrakeForFuzzing() {
  // We should always pass jitless for wasm-jitless.
  v8_flags.jitless = true;
  v8_flags.wasm_jitless = true;

  // This configuration will make fuzzing slower, but helps to reproduce GC bugs
  // more reliably.
  v8_flags.stress_compaction = true;
  v8_flags.gc_interval = 500;

  // Avoid restarting the fuzzer process due to timeout when there is an
  // infinite loop.
  v8_flags.drumbrake_fuzzer_timeout = true;

  // We reduce the maximum memory size and table size of WebAssembly instances
  // to avoid OOMs in the fuzzer.
  v8_flags.wasm_max_mem_pages = 32;
  v8_flags.wasm_max_table_size = 100;

  FlagList::EnforceFlagImplications();

  WasmInterpreterThread::Initialize();
}

int LLVMFuzzerTestOneInputCommon(const uint8_t* data, size_t size,
                                 GenerateModuleFunc generate_module) {
  auto rnd_gen = MersenneTwister(reinterpret_cast<const char*>(data), size);

  wasm::fuzzing::InitializeDrumbrakeForFuzzing();

  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();

  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_exception()) {
    i_isolate->clear_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());

  // Drumbrake may not support some experimental features yet.
  // wasm::fuzzing::EnableExperimentalWasmFeatures(isolate);

  v8::TryCatch try_catch(isolate);
  testing::SetupIsolateForWasmModule(i_isolate);

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  wasm::ZoneBuffer buffer(&zone);

  if (!generate_module(i_isolate, &zone, {data, size}, &buffer)) {
    return 0;
  }

  wasm::ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());

  HandleScope scope(i_isolate);
  wasm::ErrorThrower thrower(i_isolate, "wasm fuzzer");
  DirectHandle<WasmModuleObject> module_object;
  auto enabled_features = wasm::WasmEnabledFeatures::FromIsolate(i_isolate);
  bool compiles = wasm::GetWasmEngine()
                      ->SyncCompile(i_isolate, enabled_features,
                                    fuzzing::CompileTimeImportsForFuzzing(),
                                    &thrower, v8::base::OwnedCopyOf(buffer))
                      .ToHandle(&module_object);
  // TODO(338326645): add similar GenerateTestCase code for wasm fast
  // interpreter.
  // if (v8_flags.wasm_fuzzer_gen_test) {
  //   wasm::fuzzing::GenerateTestCase(i_isolate, wire_bytes, compiles);
  // }
  int module_result = -1;
  if (compiles) {
    module_result =
        FastInterpretAndExecuteModule(i_isolate, module_object, rnd_gen);
  }
  thrower.Reset();

  // Pump the message loop and run micro tasks, e.g. GC finalization tasks.
  support->PumpMessageLoop(v8::platform::MessageLoopBehavior::kDoNotWait);
  isolate->PerformMicrotaskCheckpoint();
  return module_result;
}

}  // namespace v8::internal::wasm::fuzzing
