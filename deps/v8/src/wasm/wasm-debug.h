// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_DEBUG_H_
#define V8_WASM_DEBUG_H_

#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmDebugInfo : public FixedArray {
 public:
  static Handle<WasmDebugInfo> New(Handle<JSObject> wasm);

  static bool IsDebugInfo(Object* object);
  static WasmDebugInfo* cast(Object* object);

  JSObject* wasm_object();

  bool SetBreakPoint(int byte_offset);

  // Get the Script for the specified function.
  static Script* GetFunctionScript(Handle<WasmDebugInfo> debug_info,
                                   int func_index);

  // Disassemble the specified function from this module.
  static Handle<String> DisassembleFunction(Handle<WasmDebugInfo> debug_info,
                                            int func_index);

  // Get the offset table for the specified function, mapping from byte offsets
  // to position in the disassembly.
  // Returns an array with three entries per instruction: byte offset, line and
  // column.
  static Handle<FixedArray> GetFunctionOffsetTable(
      Handle<WasmDebugInfo> debug_info, int func_index);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_DEBUG_H_
