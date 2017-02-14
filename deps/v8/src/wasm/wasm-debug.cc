// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assert-scope.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"

using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {

enum {
  kWasmDebugInfoWasmObj,
  kWasmDebugInfoWasmBytesHash,
  kWasmDebugInfoAsmJsOffsets,
  kWasmDebugInfoNumEntries
};

// TODO(clemensh): Move asm.js offset tables to the compiled module.
FixedArray *GetAsmJsOffsetTables(Handle<WasmDebugInfo> debug_info,
                                 Isolate *isolate) {
  Object *offset_tables = debug_info->get(kWasmDebugInfoAsmJsOffsets);
  if (!offset_tables->IsUndefined(isolate)) {
    return FixedArray::cast(offset_tables);
  }

  Handle<JSObject> wasm_instance(debug_info->wasm_instance(), isolate);
  Handle<WasmCompiledModule> compiled_module(GetCompiledModule(*wasm_instance),
                                             isolate);
  DCHECK(compiled_module->has_asm_js_offset_tables());

  AsmJsOffsetsResult asm_offsets;
  {
    Handle<ByteArray> asm_offset_tables =
        compiled_module->asm_js_offset_tables();
    DisallowHeapAllocation no_gc;
    const byte *bytes_start = asm_offset_tables->GetDataStartAddress();
    const byte *bytes_end = bytes_start + asm_offset_tables->length();
    asm_offsets = wasm::DecodeAsmJsOffsets(bytes_start, bytes_end);
  }
  // Wasm bytes must be valid and must contain asm.js offset table.
  DCHECK(asm_offsets.ok());
  DCHECK_GE(static_cast<size_t>(kMaxInt), asm_offsets.val.size());
  int num_functions = static_cast<int>(asm_offsets.val.size());
  DCHECK_EQ(
      wasm::GetNumberOfFunctions(handle(debug_info->wasm_instance())),
      static_cast<int>(num_functions +
                       compiled_module->module()->num_imported_functions));
  Handle<FixedArray> all_tables =
      isolate->factory()->NewFixedArray(num_functions);
  debug_info->set(kWasmDebugInfoAsmJsOffsets, *all_tables);
  for (int func = 0; func < num_functions; ++func) {
    std::vector<std::pair<int, int>> &func_asm_offsets = asm_offsets.val[func];
    if (func_asm_offsets.empty()) continue;
    size_t array_size = 2 * kIntSize * func_asm_offsets.size();
    CHECK_LE(array_size, static_cast<size_t>(kMaxInt));
    ByteArray *arr =
        *isolate->factory()->NewByteArray(static_cast<int>(array_size));
    all_tables->set(func, arr);
    int idx = 0;
    for (std::pair<int, int> p : func_asm_offsets) {
      // Byte offsets must be strictly monotonously increasing:
      DCHECK(idx == 0 || p.first > arr->get_int(idx - 2));
      arr->set_int(idx++, p.first);
      arr->set_int(idx++, p.second);
    }
    DCHECK_EQ(arr->length(), idx * kIntSize);
  }
  return *all_tables;
}
}  // namespace

Handle<WasmDebugInfo> WasmDebugInfo::New(Handle<JSObject> wasm) {
  Isolate *isolate = wasm->GetIsolate();
  Factory *factory = isolate->factory();
  Handle<FixedArray> arr =
      factory->NewFixedArray(kWasmDebugInfoNumEntries, TENURED);
  arr->set(kWasmDebugInfoWasmObj, *wasm);
  int hash = 0;
  Handle<SeqOneByteString> wasm_bytes = GetWasmBytes(wasm);
  {
    DisallowHeapAllocation no_gc;
    hash = StringHasher::HashSequentialString(
        wasm_bytes->GetChars(), wasm_bytes->length(), kZeroHashSeed);
  }
  Handle<Object> hash_obj = factory->NewNumberFromInt(hash, TENURED);
  arr->set(kWasmDebugInfoWasmBytesHash, *hash_obj);

  return Handle<WasmDebugInfo>::cast(arr);
}

bool WasmDebugInfo::IsDebugInfo(Object *object) {
  if (!object->IsFixedArray()) return false;
  FixedArray *arr = FixedArray::cast(object);
  return arr->length() == kWasmDebugInfoNumEntries &&
         IsWasmInstance(arr->get(kWasmDebugInfoWasmObj)) &&
         arr->get(kWasmDebugInfoWasmBytesHash)->IsNumber();
}

WasmDebugInfo *WasmDebugInfo::cast(Object *object) {
  DCHECK(IsDebugInfo(object));
  return reinterpret_cast<WasmDebugInfo *>(object);
}

JSObject *WasmDebugInfo::wasm_instance() {
  return JSObject::cast(get(kWasmDebugInfoWasmObj));
}

int WasmDebugInfo::GetAsmJsSourcePosition(Handle<WasmDebugInfo> debug_info,
                                          int func_index, int byte_offset) {
  Isolate *isolate = debug_info->GetIsolate();
  Handle<JSObject> instance(debug_info->wasm_instance(), isolate);
  FixedArray *offset_tables = GetAsmJsOffsetTables(debug_info, isolate);

  WasmCompiledModule *compiled_module = wasm::GetCompiledModule(*instance);
  int num_imported_functions =
      compiled_module->module()->num_imported_functions;
  DCHECK_LE(num_imported_functions, func_index);
  func_index -= num_imported_functions;
  DCHECK_LT(func_index, offset_tables->length());
  ByteArray *offset_table = ByteArray::cast(offset_tables->get(func_index));

  // Binary search for the current byte offset.
  int left = 0;                                       // inclusive
  int right = offset_table->length() / kIntSize / 2;  // exclusive
  DCHECK_LT(left, right);
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (offset_table->get_int(2 * mid) <= byte_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // There should be an entry for each position that could show up on the stack
  // trace:
  DCHECK_EQ(byte_offset, offset_table->get_int(2 * left));
  return offset_table->get_int(2 * left + 1);
}
