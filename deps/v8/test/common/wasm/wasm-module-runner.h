// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_RUNNER_H_
#define V8_WASM_MODULE_RUNNER_H_

#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace testing {

// Decodes the given encoded module.
const WasmModule* DecodeWasmModuleForTesting(Isolate* isolate, Zone* zone,
                                             ErrorThrower* thrower,
                                             const byte* module_start,
                                             const byte* module_end,
                                             ModuleOrigin origin);

// Instantiates a module without any imports and exports.
const Handle<JSObject> InstantiateModuleForTesting(Isolate* isolate,
                                                   ErrorThrower* thrower,
                                                   const WasmModule* module);

int32_t CallWasmFunctionForTesting(Isolate* isolate, Handle<JSObject> instance,
                                   ErrorThrower* thrower, const char* name,
                                   int argc, Handle<Object> argv[],
                                   ModuleOrigin origin);

// Decode, verify, and run the function labeled "main" in the
// given encoded module. The module should have no imports.
int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end, ModuleOrigin origin);

// Interprets the given module, starting at the function specified by
// {function_index}. The return type of the function has to be int32. The module
// should not have any imports or exports
int32_t InterpretWasmModule(Isolate* isolate, ErrorThrower* thrower,
                            const WasmModule* module, int function_index,
                            WasmVal* args);

// Compiles WasmModule bytes and return an instance of the compiled module.
const Handle<JSObject> CompileInstantiateWasmModuleForTesting(
    Isolate* isolate, Zone* zone, const byte* module_start,
    const byte* module_end, ModuleOrigin origin);

// Runs the module instance with arguments.
int32_t RunWasmModuleForTesting(Isolate* isolate, Handle<JSObject> instance,
                                int argc, Handle<Object> argv[],
                                ModuleOrigin origin);
// Install function map, module symbol for testing
void SetupIsolateForWasmModule(Isolate* isolate);
}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_RUNNER_H_
