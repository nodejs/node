// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_RUNNER_H_
#define V8_WASM_MODULE_RUNNER_H_

#include "src/execution/isolate.h"
#include "src/objects/objects.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

template <typename T>
class MaybeHandle;

namespace wasm {
namespace testing {

// Returns a MaybeHandle to the JsToWasm wrapper of the wasm function exported
// with the given name by the provided instance.
MaybeHandle<WasmExportedFunction> GetExportedFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, const char* name);

// Call an exported wasm function by name. Returns -1 if the export does not
// exist or throws an error. Errors are cleared from the isolate before
// returning. {exception} is set to to true if an exception happened during
// execution of the wasm function.
int32_t CallWasmFunctionForTesting(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   const char* name, int argc,
                                   Handle<Object> argv[],
                                   bool* exception = nullptr);

// Decode, verify, and run the function labeled "main" in the
// given encoded module. The module should have no imports.
int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end);

// Decode and compile the given module with no imports.
MaybeHandle<WasmModuleObject> CompileForTesting(Isolate* isolate,
                                                ErrorThrower* thrower,
                                                const ModuleWireBytes& bytes);

// Decode, compile, and instantiate the given module with no imports.
MaybeHandle<WasmInstanceObject> CompileAndInstantiateForTesting(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes);

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
  // to call was found in the module, the function did not termine within a
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
// should not have any imports or exports
WasmInterpretationResult InterpretWasmModule(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    int32_t function_index, WasmValue* args);

// Generate an array of default arguments for the given signature, to be used in
// the interpreter.
OwnedVector<WasmValue> MakeDefaultInterpreterArguments(Isolate* isolate,
                                                       const FunctionSig* sig);

// Generate an array of default arguments for the given signature, to be used
// when calling compiled code. Make sure that the arguments match the ones
// returned by {MakeDefaultInterpreterArguments}, otherwise fuzzers will report
// differences between interpreter and compiled code.
OwnedVector<Handle<Object>> MakeDefaultArguments(Isolate* isolate,
                                                 const FunctionSig* sig);

// Install function map, module symbol for testing
void SetupIsolateForWasmModule(Isolate* isolate);

}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_RUNNER_H_
