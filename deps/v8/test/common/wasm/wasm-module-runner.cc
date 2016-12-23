// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/wasm/wasm-module-runner.h"

#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace testing {

uint32_t GetMinModuleMemSize(const WasmModule* module) {
  return WasmModule::kPageSize * module->min_mem_pages;
}

const WasmModule* DecodeWasmModuleForTesting(Isolate* isolate, Zone* zone,
                                             ErrorThrower* thrower,
                                             const byte* module_start,
                                             const byte* module_end,
                                             ModuleOrigin origin) {
  // Decode the module, but don't verify function bodies, since we'll
  // be compiling them anyway.
  ModuleResult decoding_result =
      DecodeWasmModule(isolate, zone, module_start, module_end, false, origin);

  std::unique_ptr<const WasmModule> module(decoding_result.val);
  if (decoding_result.failed()) {
    // Module verification failed. throw.
    thrower->Error("WASM.compileRun() failed: %s",
                   decoding_result.error_msg.get());
    return nullptr;
  }

  if (thrower->error()) return nullptr;
  return module.release();
}

const Handle<JSObject> InstantiateModuleForTesting(Isolate* isolate,
                                                   ErrorThrower* thrower,
                                                   const WasmModule* module) {
  CHECK(module != nullptr);

  if (module->import_table.size() > 0) {
    thrower->Error("Not supported: module has imports.");
  }
  if (module->export_table.size() == 0) {
    thrower->Error("Not supported: module has no exports.");
  }
  if (thrower->error()) return Handle<JSObject>::null();

  // Although we decoded the module for some pre-validation, run the bytes
  // again through the normal pipeline.
  MaybeHandle<JSObject> module_object = CreateModuleObjectFromBytes(
      isolate, module->module_start, module->module_end, thrower,
      ModuleOrigin::kWasmOrigin);
  if (module_object.is_null()) {
    thrower->Error("Module pre-validation failed.");
    return Handle<JSObject>::null();
  }
  MaybeHandle<JSObject> maybe_instance = WasmModule::Instantiate(
      isolate, thrower, module_object.ToHandleChecked(),
      Handle<JSReceiver>::null(), Handle<JSArrayBuffer>::null());
  Handle<JSObject> instance;
  if (!maybe_instance.ToHandle(&instance)) {
    return Handle<JSObject>::null();
  }
  return instance;
}

const Handle<JSObject> CompileInstantiateWasmModuleForTesting(
    Isolate* isolate, Zone* zone, const byte* module_start,
    const byte* module_end, ModuleOrigin origin) {
  ErrorThrower thrower(isolate, "CompileInstantiateWasmModule");
  std::unique_ptr<const WasmModule> module(DecodeWasmModuleForTesting(
      isolate, zone, &thrower, module_start, module_end, origin));

  if (module == nullptr) {
    thrower.Error("Wasm module decode failed");
    return Handle<JSObject>::null();
  }
  return InstantiateModuleForTesting(isolate, &thrower, module.get());
}

int32_t RunWasmModuleForTesting(Isolate* isolate, Handle<JSObject> instance,
                                int argc, Handle<Object> argv[],
                                ModuleOrigin origin) {
  ErrorThrower thrower(isolate, "RunWasmModule");
  const char* f_name = origin == ModuleOrigin::kAsmJsOrigin ? "caller" : "main";
  return CallWasmFunctionForTesting(isolate, instance, &thrower, f_name, argc,
                                    argv, origin);
}

int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end, ModuleOrigin origin) {
  HandleScope scope(isolate);
  Zone zone(isolate->allocator());

  Handle<JSObject> instance = CompileInstantiateWasmModuleForTesting(
      isolate, &zone, module_start, module_end, origin);
  if (instance.is_null()) {
    return -1;
  }
  return RunWasmModuleForTesting(isolate, instance, 0, nullptr, origin);
}

int32_t InterpretWasmModule(Isolate* isolate, ErrorThrower* thrower,
                            const WasmModule* module, int function_index,
                            WasmVal* args) {
  CHECK(module != nullptr);

  Zone zone(isolate->allocator());
  v8::internal::HandleScope scope(isolate);

  if (module->import_table.size() > 0) {
    thrower->Error("Not supported: module has imports.");
  }
  if (module->export_table.size() == 0) {
    thrower->Error("Not supported: module has no exports.");
  }

  if (thrower->error()) return -1;

  ModuleEnv module_env;
  module_env.module = module;
  module_env.origin = module->origin;

  for (size_t i = 0; i < module->functions.size(); i++) {
    FunctionBody body = {
        &module_env, module->functions[i].sig, module->module_start,
        module->module_start + module->functions[i].code_start_offset,
        module->module_start + module->functions[i].code_end_offset};
    DecodeResult result = VerifyWasmCode(isolate->allocator(), body);
    if (result.failed()) {
      thrower->Error("Function did not verify");
      return -1;
    }
  }

  // The code verifies, we create an instance to run it in the interpreter.
  WasmModuleInstance instance(module);
  instance.context = isolate->native_context();
  instance.mem_size = GetMinModuleMemSize(module);
  // TODO(ahaas): Move memory allocation to wasm-module.cc for better
  // encapsulation.
  instance.mem_start =
      static_cast<byte*>(calloc(GetMinModuleMemSize(module), 1));
  instance.globals_start = nullptr;
  module_env.instance = &instance;

  WasmInterpreter interpreter(&instance, isolate->allocator());

  WasmInterpreter::Thread* thread = interpreter.GetThread(0);
  thread->Reset();
  thread->PushFrame(&(module->functions[function_index]), args);
  WasmInterpreter::State interpreter_result = thread->Run();
  if (instance.mem_start) {
    free(instance.mem_start);
  }
  if (interpreter_result == WasmInterpreter::FINISHED) {
    WasmVal val = thread->GetReturnValue();
    return val.to<int32_t>();
  } else if (thread->state() == WasmInterpreter::TRAPPED) {
    return 0xdeadbeef;
  } else {
    thrower->Error(
        "Interpreter did not finish execution within its step bound");
    return -1;
  }
}

int32_t CallWasmFunctionForTesting(Isolate* isolate, Handle<JSObject> instance,
                                   ErrorThrower* thrower, const char* name,
                                   int argc, Handle<Object> argv[],
                                   ModuleOrigin origin) {
  Handle<JSObject> exports_object;
  if (origin == ModuleOrigin::kAsmJsOrigin) {
    exports_object = instance;
  } else {
    Handle<Name> exports = isolate->factory()->InternalizeUtf8String("exports");
    exports_object = Handle<JSObject>::cast(
        JSObject::GetProperty(instance, exports).ToHandleChecked());
  }
  Handle<Name> main_name = isolate->factory()->NewStringFromAsciiChecked(name);
  PropertyDescriptor desc;
  Maybe<bool> property_found = JSReceiver::GetOwnPropertyDescriptor(
      isolate, exports_object, main_name, &desc);
  if (!property_found.FromMaybe(false)) return -1;

  Handle<JSFunction> main_export = Handle<JSFunction>::cast(desc.value());

  // Call the JS function.
  Handle<Object> undefined = isolate->factory()->undefined_value();
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_export, undefined, argc, argv);

  // The result should be a number.
  if (retval.is_null()) {
    thrower->Error("WASM.compileRun() failed: Invocation was null");
    return -1;
  }
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    return Smi::cast(*result)->value();
  }
  if (result->IsHeapNumber()) {
    return static_cast<int32_t>(HeapNumber::cast(*result)->value());
  }
  thrower->Error("WASM.compileRun() failed: Return value should be number");
  return -1;
}

void SetupIsolateForWasmModule(Isolate* isolate) {
  WasmJs::InstallWasmMapsIfNeeded(isolate, isolate->native_context());
  WasmJs::InstallWasmModuleSymbolIfNeeded(isolate, isolate->global_object(),
                                          isolate->native_context());
}

}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8
