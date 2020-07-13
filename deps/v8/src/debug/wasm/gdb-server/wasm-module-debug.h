// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_WASM_MODULE_DEBUG_H_
#define V8_DEBUG_WASM_GDB_SERVER_WASM_MODULE_DEBUG_H_

#include "src/debug/debug.h"
#include "src/debug/wasm/gdb-server/gdb-remote-util.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmValue;

namespace gdb_server {

// Represents the interface to access the Wasm engine state for a given module.
// For the moment it only works with interpreted functions, in the future it
// could be extended to also support Liftoff.
class WasmModuleDebug {
 public:
  WasmModuleDebug(v8::Isolate* isolate, Local<debug::WasmScript> script);

  std::string GetModuleName() const;
  i::Isolate* GetIsolate() const {
    return reinterpret_cast<i::Isolate*>(isolate_);
  }

  // Gets the value of the {index}th global value.
  static bool GetWasmGlobal(Isolate* isolate, uint32_t frame_index,
                            uint32_t index, uint8_t* buffer,
                            uint32_t buffer_size, uint32_t* size);

  // Gets the value of the {index}th local value in the {frame_index}th stack
  // frame.
  static bool GetWasmLocal(Isolate* isolate, uint32_t frame_index,
                           uint32_t index, uint8_t* buffer,
                           uint32_t buffer_size, uint32_t* size);

  // Gets the value of the {index}th value in the operand stack.
  static bool GetWasmStackValue(Isolate* isolate, uint32_t frame_index,
                                uint32_t index, uint8_t* buffer,
                                uint32_t buffer_size, uint32_t* size);

  // Reads {size} bytes, starting from {offset}, from the Memory instance
  // associated to this module.
  // Returns the number of byte copied to {buffer}, or 0 is case of error.
  // Note: only one Memory for Module is currently supported.
  static uint32_t GetWasmMemory(Isolate* isolate, uint32_t frame_index,
                                uint32_t offset, uint8_t* buffer,
                                uint32_t size);

  // Gets {size} bytes, starting from {offset}, from the Code space of this
  // module.
  // Returns the number of byte copied to {buffer}, or 0 is case of error.
  uint32_t GetWasmModuleBytes(wasm_addr_t wasm_addr, uint8_t* buffer,
                              uint32_t size);

  // Inserts a breakpoint at the offset {offset} of this module.
  // Returns {true} if the breakpoint was successfully added.
  bool AddBreakpoint(uint32_t offset, int* breakpoint_id);

  // Removes a breakpoint at the offset {offset} of the this module.
  void RemoveBreakpoint(uint32_t offset, int breakpoint_id);

  // Handle stepping in wasm functions via the wasm interpreter.
  void PrepareStep();

  // Returns the current stack trace as a vector of instruction pointers.
  static std::vector<wasm_addr_t> GetCallStack(uint32_t debug_context_id,
                                               Isolate* isolate);

 private:
  // Returns the module WasmInstance associated to the {frame_index}th frame
  // in the call stack.
  static Handle<WasmInstanceObject> GetWasmInstance(Isolate* isolate,
                                                    uint32_t frame_index);

  // Returns its first WasmInstance for this Wasm module.
  Handle<WasmInstanceObject> GetFirstWasmInstance();

  // Iterates on current stack frames and return frame information for the
  // {frame_index} specified.
  // Returns an empty array if the frame specified does not correspond to a Wasm
  // stack frame.
  static std::vector<FrameSummary> FindWasmFrame(
      StackTraceFrameIterator* frame_it, uint32_t* frame_index);

  // Converts a WasmValue into an array of bytes.
  static bool GetWasmValue(const wasm::WasmValue& wasm_value, uint8_t* buffer,
                           uint32_t buffer_size, uint32_t* size);

  v8::Isolate* isolate_;
  Global<debug::WasmScript> wasm_script_;
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_WASM_MODULE_DEBUG_H_
