// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_RUNNER_H_
#define V8_WASM_MODULE_RUNNER_H_

#include "src/isolate.h"
#include "src/objects.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

namespace wasm {
namespace testing {

// Decodes the given encoded module.
std::unique_ptr<WasmModule> DecodeWasmModuleForTesting(
    Isolate* isolate, ErrorThrower* thrower, const byte* module_start,
    const byte* module_end, ModuleOrigin origin, bool verify_functions = false);

// Returns a MaybeHandle to the JsToWasm wrapper of the wasm function exported
// with the given name by the provided instance.
MaybeHandle<WasmExportedFunction> GetExportedFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, const char* name);

// Call an exported wasm function by name. Returns -1 if the export does not
// exist or throws an error. Errors are cleared from the isolate before
// returning.
int32_t CallWasmFunctionForTesting(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   ErrorThrower* thrower, const char* name,
                                   int argc, Handle<Object> argv[]);

// Interprets an exported wasm function by name. Returns false if it was not
// possible to execute the function (e.g. because it does not exist), or if the
// interpretation does not finish after kMaxNumSteps. Otherwise returns true.
// The arguments array is extended with default values if necessary.
bool InterpretWasmModuleForTesting(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   const char* name, size_t argc,
                                   WasmValue* args);

// Decode, verify, and run the function labeled "main" in the
// given encoded module. The module should have no imports.
int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end);

// Interprets the given module, starting at the function specified by
// {function_index}. The return type of the function has to be int32. The module
// should not have any imports or exports
int32_t InterpretWasmModule(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            ErrorThrower* thrower, int32_t function_index,
                            WasmValue* args, bool* possible_nondeterminism);

// Runs the module instance with arguments.
int32_t RunWasmModuleForTesting(Isolate* isolate,
                                Handle<WasmInstanceObject> instance, int argc,
                                Handle<Object> argv[]);

// Install function map, module symbol for testing
void SetupIsolateForWasmModule(Isolate* isolate);

}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_RUNNER_H_
