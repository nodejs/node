// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_INTERPRETER_WASM_INTERPRETER_OBJECTS_H_
#define V8_WASM_INTERPRETER_WASM_INTERPRETER_OBJECTS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/objects/managed.h"
#include "src/objects/struct.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {
class Isolate;
class WasmInstanceObject;
class WasmTrustedInstanceData;

namespace wasm {
class InterpreterHandle;
}  // namespace wasm

struct WasmInterpreterStackEntry {
  int function_index;
  int byte_offset;
};

// The interpreter state for an instance is a TrustedManaged<InterpreterHandle>
// reached via the protected `interpreter_handle` link on the instance's
// WasmTrustedInstanceData. WasmInterpreterObject only has static helpers that
// operate on a DirectHandle<WasmTrustedInstanceData>.
class WasmInterpreterObject {
 public:
  // Execute the specified function in the interpreter. Read arguments from the
  // {argument_values} vector and write to {return_values} on regular exit.
  // The frame_pointer will be used to identify the new activation of the
  // interpreter for unwinding and frame inspection.
  // Returns true if exited regularly, false if a trap occurred. In the latter
  // case, a pending exception will have been set on the isolate.
  static bool RunInterpreter(
      Isolate* isolate, Address frame_pointer,
      DirectHandle<WasmTrustedInstanceData> trusted_data, int func_index,
      const std::vector<wasm::WasmValue>& argument_values,
      std::vector<wasm::WasmValue>& return_values);
  static bool RunInterpreter(Isolate* isolate, Address frame_pointer,
                             DirectHandle<WasmTrustedInstanceData> trusted_data,
                             int func_index, uint8_t* interpreter_sp);

  // Get the stack of the wasm interpreter as pairs of {function index, byte
  // offset}. The list is ordered bottom-to-top, i.e. caller before callee.
  static std::vector<WasmInterpreterStackEntry> GetInterpretedStack(
      Tagged<WasmTrustedInstanceData> trusted_data, Address frame_pointer);

  // Get the function index for the index-th frame in the Activation identified
  // by a given frame_pointer.
  static int GetFunctionIndex(Tagged<WasmTrustedInstanceData> trusted_data,
                              Address frame_pointer, int index);
};

namespace wasm {
V8_EXPORT_PRIVATE DirectHandle<TrustedManaged<InterpreterHandle>>
GetInterpreterHandle(Isolate* isolate,
                     DirectHandle<WasmTrustedInstanceData> trusted_data);
V8_EXPORT_PRIVATE DirectHandle<TrustedManaged<InterpreterHandle>>
GetOrCreateInterpreterHandle(
    Isolate* isolate, DirectHandle<WasmTrustedInstanceData> trusted_data);
}  // namespace wasm

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_INTERPRETER_WASM_INTERPRETER_OBJECTS_H_
