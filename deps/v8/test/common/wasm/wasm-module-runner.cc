// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/wasm/wasm-module-runner.h"

#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/wasm/module-decoder.h"
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
  return WasmModule::kPageSize * module->initial_pages;
}

std::unique_ptr<WasmModule> DecodeWasmModuleForTesting(
    Isolate* isolate, ErrorThrower* thrower, const byte* module_start,
    const byte* module_end, ModuleOrigin origin, bool verify_functions) {
  // Decode the module, but don't verify function bodies, since we'll
  // be compiling them anyway.
  ModuleResult decoding_result = SyncDecodeWasmModule(
      isolate, module_start, module_end, verify_functions, origin);

  if (decoding_result.failed()) {
    // Module verification failed. throw.
    thrower->CompileError("DecodeWasmModule failed: %s",
                          decoding_result.error_msg().c_str());
  }

  return std::move(decoding_result.val);
}

int32_t RunWasmModuleForTesting(Isolate* isolate, Handle<JSObject> instance,
                                int argc, Handle<Object> argv[]) {
  ErrorThrower thrower(isolate, "RunWasmModule");
  return CallWasmFunctionForTesting(isolate, instance, &thrower, "main", argc,
                                    argv);
}

int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end) {
  HandleScope scope(isolate);
  ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
  MaybeHandle<WasmInstanceObject> instance = SyncCompileAndInstantiate(
      isolate, &thrower, ModuleWireBytes(module_start, module_end), {}, {});
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
  MaybeHandle<WasmModuleObject> module = wasm::SyncCompileTranslatedAsmJs(
      isolate, &thrower, ModuleWireBytes(module_start, module_end),
      Handle<Script>::null(), Vector<const byte>());
  DCHECK_EQ(thrower.error(), module.is_null());
  if (module.is_null()) return -1;

  MaybeHandle<WasmInstanceObject> instance = wasm::SyncInstantiate(
      isolate, &thrower, module.ToHandleChecked(), Handle<JSReceiver>::null(),
      Handle<JSArrayBuffer>::null());
  DCHECK_EQ(thrower.error(), instance.is_null());
  if (instance.is_null()) return -1;

  return RunWasmModuleForTesting(isolate, instance.ToHandleChecked(), 0,
                                 nullptr);
}
int32_t InterpretWasmModule(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            ErrorThrower* thrower, int32_t function_index,
                            WasmValue* args, bool* possible_nondeterminism) {
  // Don't execute more than 16k steps.
  constexpr int kMaxNumSteps = 16 * 1024;

  Zone zone(isolate->allocator(), ZONE_NAME);
  v8::internal::HandleScope scope(isolate);

  WasmInterpreter* interpreter = WasmDebugInfo::SetupForTesting(instance);
  WasmInterpreter::HeapObjectsScope heap_objects_scope(interpreter, instance);
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);
  thread->Reset();
  thread->InitFrame(&(instance->module()->functions[function_index]), args);
  WasmInterpreter::State interpreter_result = thread->Run(kMaxNumSteps);

  *possible_nondeterminism = thread->PossibleNondeterminism();
  if (interpreter_result == WasmInterpreter::FINISHED) {
    WasmValue val = thread->GetReturnValue();
    return val.to<int32_t>();
  } else if (thread->state() == WasmInterpreter::TRAPPED) {
    return 0xdeadbeef;
  } else {
    thrower->RangeError(
        "Interpreter did not finish execution within its step bound");
    return -1;
  }
}

int32_t CallWasmFunctionForTesting(Isolate* isolate, Handle<JSObject> instance,
                                   ErrorThrower* thrower, const char* name,
                                   int argc, Handle<Object> argv[]) {
  Handle<JSObject> exports_object;
  Handle<Name> exports = isolate->factory()->InternalizeUtf8String("exports");
  exports_object = Handle<JSObject>::cast(
      JSObject::GetProperty(instance, exports).ToHandleChecked());

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
  WasmJs::Install(isolate);
}

}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8
