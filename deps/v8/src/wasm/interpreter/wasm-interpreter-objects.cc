// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/interpreter/wasm-interpreter-objects.h"

#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/interpreter/wasm-interpreter-runtime.h"
#include "src/wasm/interpreter/wasm-interpreter.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

// static
bool WasmInterpreterObject::RunInterpreter(
    Isolate* isolate, Address frame_pointer,
    DirectHandle<WasmTrustedInstanceData> trusted_data, int func_index,
    const std::vector<wasm::WasmValue>& argument_values,
    std::vector<wasm::WasmValue>& return_values) {
  DCHECK_LE(0, func_index);

  wasm::WasmInterpreterThread* thread =
      wasm::WasmInterpreterThread::GetCurrentInterpreterThread(isolate);
  DCHECK_NOT_NULL(thread);

  // Assume an instance can run in only one thread.
  DirectHandle<TrustedManaged<wasm::InterpreterHandle>> handle =
      wasm::GetOrCreateInterpreterHandle(isolate, trusted_data);

  // Publish the currently-executing trusted instance data to the runtime so
  // that wasm_trusted_instance_data() serves it from the GC-rooted cache.
  wasm::WasmInterpreterRuntime::InstanceScope instance_scope(
      handle->raw()->interpreter()->GetWasmRuntime(), trusted_data);

  return handle->raw()->Execute(thread, frame_pointer,
                                static_cast<uint32_t>(func_index),
                                argument_values, return_values);
}

// static
bool WasmInterpreterObject::RunInterpreter(
    Isolate* isolate, Address frame_pointer,
    DirectHandle<WasmTrustedInstanceData> trusted_data, int func_index,
    uint8_t* interpreter_sp) {
  DCHECK_LE(0, func_index);

  wasm::WasmInterpreterThread* thread =
      wasm::WasmInterpreterThread::GetCurrentInterpreterThread(isolate);
  DCHECK_NOT_NULL(thread);

  // Assume an instance can run in only one thread.
  DirectHandle<TrustedManaged<wasm::InterpreterHandle>> handle =
      wasm::GetInterpreterHandle(isolate, trusted_data);

  wasm::WasmInterpreterRuntime::InstanceScope instance_scope(
      handle->raw()->interpreter()->GetWasmRuntime(), trusted_data);

  return handle->raw()->Execute(
      thread, frame_pointer, static_cast<uint32_t>(func_index), interpreter_sp);
}

// static
std::vector<WasmInterpreterStackEntry>
WasmInterpreterObject::GetInterpretedStack(
    Tagged<WasmTrustedInstanceData> trusted_data, Address frame_pointer) {
  CHECK(trusted_data->has_interpreter_handle());
  return trusted_data->interpreter_handle()->raw()->GetInterpretedStack(
      frame_pointer);
}

// static
int WasmInterpreterObject::GetFunctionIndex(
    Tagged<WasmTrustedInstanceData> trusted_data, Address frame_pointer,
    int index) {
  CHECK(trusted_data->has_interpreter_handle());
  return trusted_data->interpreter_handle()->raw()->GetFunctionIndex(
      frame_pointer, index);
}

}  // namespace internal
}  // namespace v8
