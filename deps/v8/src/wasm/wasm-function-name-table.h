// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_NAME_TABLE_H_
#define V8_WASM_FUNCTION_NAME_TABLE_H_

#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace wasm {

// Forward declarations for some WASM data structures.
struct WasmModule;

// Encode all function names of the WasmModule into one ByteArray.
Handle<Object> BuildFunctionNamesTable(Isolate* isolate, WasmModule* module);

// Extract the function name for the given func_index from the wasm module.
// Returns undefined if the function index is invalid.
Handle<Object> GetWasmFunctionNameFromTable(Handle<ByteArray> wasm_names_table,
                                            uint32_t func_index);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_NAME_TABLE_H_
