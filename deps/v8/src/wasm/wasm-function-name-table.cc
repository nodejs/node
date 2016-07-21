// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-function-name-table.h"

#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

// Build an array with all function names. If there are N functions in the
// module, then the first (kIntSize * (N+1)) bytes are integer entries.
// The first integer entry encodes the number of functions in the module.
// The entries 1 to N contain offsets into the second part of this array.
// After these N+1 integer entries, the second part begins, which holds a
// concatenation of all function names.
//
// Returns undefined if the array length would not fit in an integer value
Handle<Object> BuildFunctionNamesTable(Isolate* isolate, WasmModule* module) {
  uint64_t func_names_length = 0;
  for (auto& func : module->functions) func_names_length += func.name_length;
  int num_funcs_int = static_cast<int>(module->functions.size());
  int current_offset = (num_funcs_int + 1) * kIntSize;
  uint64_t total_array_length = current_offset + func_names_length;
  int total_array_length_int = static_cast<int>(total_array_length);
  // Check for overflow. Just skip function names if it happens.
  if (total_array_length_int != total_array_length || num_funcs_int < 0 ||
      num_funcs_int != module->functions.size())
    return isolate->factory()->undefined_value();
  Handle<ByteArray> func_names_array =
      isolate->factory()->NewByteArray(total_array_length_int, TENURED);
  if (func_names_array.is_null()) return isolate->factory()->undefined_value();
  func_names_array->set_int(0, num_funcs_int);
  int func_index = 0;
  for (WasmFunction& fun : module->functions) {
    WasmName name = module->GetNameOrNull(&fun);
    func_names_array->copy_in(current_offset,
                              reinterpret_cast<const byte*>(name.start()),
                              name.length());
    func_names_array->set_int(func_index + 1, current_offset);
    current_offset += name.length();
    ++func_index;
  }
  return func_names_array;
}

Handle<Object> GetWasmFunctionNameFromTable(Handle<ByteArray> func_names_array,
                                            uint32_t func_index) {
  uint32_t num_funcs = static_cast<uint32_t>(func_names_array->get_int(0));
  DCHECK(static_cast<int>(num_funcs) >= 0);
  auto undefined = [&func_names_array]() -> Handle<Object> {
    return func_names_array->GetIsolate()->factory()->undefined_value();
  };
  if (func_index >= num_funcs) return undefined();
  int offset = func_names_array->get_int(func_index + 1);
  int next_offset = func_index == num_funcs - 1
                        ? func_names_array->length()
                        : func_names_array->get_int(func_index + 2);
  ScopedVector<byte> buffer(next_offset - offset);
  func_names_array->copy_out(offset, buffer.start(), next_offset - offset);
  if (!unibrow::Utf8::Validate(buffer.start(), buffer.length())) {
    return undefined();
  }
  MaybeHandle<Object> maybe_name =
      func_names_array->GetIsolate()->factory()->NewStringFromUtf8(
          Vector<const char>::cast(buffer));
  return maybe_name.is_null() ? undefined() : maybe_name.ToHandleChecked();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
