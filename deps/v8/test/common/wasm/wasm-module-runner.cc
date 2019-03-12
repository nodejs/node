// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/wasm/wasm-module-runner.h"

#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/property-descriptor.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace testing {

uint32_t GetInitialMemSize(const WasmModule* module) {
  return kWasmPageSize * module->initial_pages;
}

MaybeHandle<WasmInstanceObject> CompileAndInstantiateForTesting(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes) {
  auto enabled_features = WasmFeaturesFromIsolate(isolate);
  MaybeHandle<WasmModuleObject> module = isolate->wasm_engine()->SyncCompile(
      isolate, enabled_features, thrower, bytes);
  DCHECK_EQ(thrower->error(), module.is_null());
  if (module.is_null()) return {};

  return isolate->wasm_engine()->SyncInstantiate(
      isolate, thrower, module.ToHandleChecked(), {}, {});
}

std::shared_ptr<WasmModule> DecodeWasmModuleForTesting(
    Isolate* isolate, ErrorThrower* thrower, const byte* module_start,
    const byte* module_end, ModuleOrigin origin, bool verify_functions) {
  // Decode the module, but don't verify function bodies, since we'll
  // be compiling them anyway.
  auto enabled_features = WasmFeaturesFromIsolate(isolate);
  ModuleResult decoding_result = DecodeWasmModule(
      enabled_features, module_start, module_end, verify_functions, origin,
      isolate->counters(), isolate->allocator());

  if (decoding_result.failed()) {
    // Module verification failed. throw.
    thrower->CompileError("DecodeWasmModule failed: %s",
                          decoding_result.error().message().c_str());
  }

  return std::move(decoding_result).value();
}

bool InterpretWasmModuleForTesting(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   const char* name, size_t argc,
                                   WasmValue* args) {
  MaybeHandle<WasmExportedFunction> maybe_function =
      GetExportedFunction(isolate, instance, "main");
  Handle<WasmExportedFunction> function;
  if (!maybe_function.ToHandle(&function)) {
    return false;
  }
  int function_index = function->function_index();
  FunctionSig* signature = instance->module()->functions[function_index].sig;
  size_t param_count = signature->parameter_count();
  std::unique_ptr<WasmValue[]> arguments(new WasmValue[param_count]);

  memcpy(arguments.get(), args, std::min(param_count, argc));

  // Fill the parameters up with default values.
  for (size_t i = argc; i < param_count; ++i) {
    switch (signature->GetParam(i)) {
      case kWasmI32:
        arguments[i] = WasmValue(int32_t{0});
        break;
      case kWasmI64:
        arguments[i] = WasmValue(int64_t{0});
        break;
      case kWasmF32:
        arguments[i] = WasmValue(0.0f);
        break;
      case kWasmF64:
        arguments[i] = WasmValue(0.0);
        break;
      default:
        UNREACHABLE();
    }
  }

  // Don't execute more than 16k steps.
  constexpr int kMaxNumSteps = 16 * 1024;

  Zone zone(isolate->allocator(), ZONE_NAME);

  WasmInterpreter* interpreter = WasmDebugInfo::SetupForTesting(instance);
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);
  thread->Reset();

  // Start an activation so that we can deal with stack overflows. We do not
  // finish the activation. An activation is just part of the state of the
  // interpreter, and we do not reuse the interpreter anyways. In addition,
  // finishing the activation is not correct in all cases, e.g. when the
  // execution of the interpreter did not finish after kMaxNumSteps.
  thread->StartActivation();
  thread->InitFrame(&instance->module()->functions[function_index],
                    arguments.get());
  WasmInterpreter::State interpreter_result = thread->Run(kMaxNumSteps);

  isolate->clear_pending_exception();

  return interpreter_result != WasmInterpreter::PAUSED;
}

int32_t RunWasmModuleForTesting(Isolate* isolate,
                                Handle<WasmInstanceObject> instance, int argc,
                                Handle<Object> argv[]) {
  ErrorThrower thrower(isolate, "RunWasmModule");
  return CallWasmFunctionForTesting(isolate, instance, &thrower, "main", argc,
                                    argv);
}

int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end) {
  HandleScope scope(isolate);
  ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
  MaybeHandle<WasmInstanceObject> instance = CompileAndInstantiateForTesting(
      isolate, &thrower, ModuleWireBytes(module_start, module_end));
  if (instance.is_null()) {
    return -1;
  }
  return RunWasmModuleForTesting(isolate, instance.ToHandleChecked(), 0,
                                 nullptr);
}

int32_t CompileAndRunAsmWasmModule(Isolate* isolate, const byte* module_start,
                                   const byte* module_end) {
  HandleScope scope(isolate);
  ErrorThrower thrower(isolate, "CompileAndRunAsmWasmModule");
  MaybeHandle<AsmWasmData> data =
      isolate->wasm_engine()->SyncCompileTranslatedAsmJs(
          isolate, &thrower, ModuleWireBytes(module_start, module_end),
          Vector<const byte>(), Handle<HeapNumber>());
  DCHECK_EQ(thrower.error(), data.is_null());
  if (data.is_null()) return -1;

  MaybeHandle<WasmModuleObject> module =
      isolate->wasm_engine()->FinalizeTranslatedAsmJs(
          isolate, data.ToHandleChecked(), Handle<Script>::null());

  MaybeHandle<WasmInstanceObject> instance =
      isolate->wasm_engine()->SyncInstantiate(
          isolate, &thrower, module.ToHandleChecked(),
          Handle<JSReceiver>::null(), Handle<JSArrayBuffer>::null());
  DCHECK_EQ(thrower.error(), instance.is_null());
  if (instance.is_null()) return -1;

  return RunWasmModuleForTesting(isolate, instance.ToHandleChecked(), 0,
                                 nullptr);
}
WasmInterpretationResult InterpretWasmModule(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    int32_t function_index, WasmValue* args) {
  // Don't execute more than 16k steps.
  constexpr int kMaxNumSteps = 16 * 1024;

  Zone zone(isolate->allocator(), ZONE_NAME);
  v8::internal::HandleScope scope(isolate);

  WasmInterpreter* interpreter = WasmDebugInfo::SetupForTesting(instance);
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);
  thread->Reset();

  // Start an activation so that we can deal with stack overflows. We do not
  // finish the activation. An activation is just part of the state of the
  // interpreter, and we do not reuse the interpreter anyways. In addition,
  // finishing the activation is not correct in all cases, e.g. when the
  // execution of the interpreter did not finish after kMaxNumSteps.
  thread->StartActivation();
  thread->InitFrame(&(instance->module()->functions[function_index]), args);
  WasmInterpreter::State interpreter_result = thread->Run(kMaxNumSteps);

  bool stack_overflow = isolate->has_pending_exception();
  isolate->clear_pending_exception();

  if (stack_overflow) return WasmInterpretationResult::Stopped();

  if (thread->state() == WasmInterpreter::TRAPPED) {
    return WasmInterpretationResult::Trapped(thread->PossibleNondeterminism());
  }

  if (interpreter_result == WasmInterpreter::FINISHED) {
    return WasmInterpretationResult::Finished(
        thread->GetReturnValue().to<int32_t>(),
        thread->PossibleNondeterminism());
  }

  return WasmInterpretationResult::Stopped();
}

MaybeHandle<WasmExportedFunction> GetExportedFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, const char* name) {
  Handle<JSObject> exports_object;
  Handle<Name> exports = isolate->factory()->InternalizeUtf8String("exports");
  exports_object = Handle<JSObject>::cast(
      JSObject::GetProperty(isolate, instance, exports).ToHandleChecked());

  Handle<Name> main_name = isolate->factory()->NewStringFromAsciiChecked(name);
  PropertyDescriptor desc;
  Maybe<bool> property_found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, exports_object, main_name, &desc);
  if (!property_found.FromMaybe(false)) return {};
  if (!desc.value()->IsJSFunction()) return {};

  return Handle<WasmExportedFunction>::cast(desc.value());
}

int32_t CallWasmFunctionForTesting(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   ErrorThrower* thrower, const char* name,
                                   int argc, Handle<Object> argv[]) {
  MaybeHandle<WasmExportedFunction> maybe_export =
      GetExportedFunction(isolate, instance, name);
  Handle<WasmExportedFunction> main_export;
  if (!maybe_export.ToHandle(&main_export)) {
    return -1;
  }

  // Call the JS function.
  Handle<Object> undefined = isolate->factory()->undefined_value();
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_export, undefined, argc, argv);

  // The result should be a number.
  if (retval.is_null()) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    thrower->RuntimeError("Calling exported wasm function failed.");
    return -1;
  }
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    return Smi::ToInt(*result);
  }
  if (result->IsHeapNumber()) {
    return static_cast<int32_t>(HeapNumber::cast(*result)->value());
  }
  thrower->RuntimeError(
      "Calling exported wasm function failed: Return value should be number");
  return -1;
}

void SetupIsolateForWasmModule(Isolate* isolate) {
  WasmJs::Install(isolate, true);
}

}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8
