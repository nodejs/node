// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_INL_H_
#define V8_WASM_WASM_OBJECTS_INL_H_

#include "src/wasm/wasm-objects.h"

#include "src/contexts-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/managed.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(WasmDebugInfo)
CAST_ACCESSOR(WasmExceptionObject)
CAST_ACCESSOR(WasmExportedFunctionData)
CAST_ACCESSOR(WasmGlobalObject)
CAST_ACCESSOR(WasmInstanceObject)
CAST_ACCESSOR(WasmMemoryObject)
CAST_ACCESSOR(WasmModuleObject)
CAST_ACCESSOR(WasmTableObject)

#define OPTIONAL_ACCESSORS(holder, name, type, offset) \
  bool holder::has_##name() {                          \
    return !READ_FIELD(this, offset)->IsUndefined();   \
  }                                                    \
  ACCESSORS(holder, name, type, offset)

#define READ_PRIMITIVE_FIELD(p, type, offset) \
  (*reinterpret_cast<type const*>(FIELD_ADDR(p, offset)))

#define WRITE_PRIMITIVE_FIELD(p, type, offset, value) \
  (*reinterpret_cast<type*>(FIELD_ADDR(p, offset)) = value)

#define PRIMITIVE_ACCESSORS(holder, name, type, offset) \
  type holder::name() const {                           \
    return READ_PRIMITIVE_FIELD(this, type, offset);    \
  }                                                     \
  void holder::set_##name(type value) {                 \
    WRITE_PRIMITIVE_FIELD(this, type, offset, value);   \
  }

// WasmModuleObject
ACCESSORS(WasmModuleObject, managed_native_module, Managed<wasm::NativeModule>,
          kNativeModuleOffset)
ACCESSORS(WasmModuleObject, export_wrappers, FixedArray, kExportWrappersOffset)
ACCESSORS(WasmModuleObject, script, Script, kScriptOffset)
ACCESSORS(WasmModuleObject, weak_instance_list, WeakArrayList,
          kWeakInstanceListOffset)
OPTIONAL_ACCESSORS(WasmModuleObject, asm_js_offset_table, ByteArray,
                   kAsmJsOffsetTableOffset)
OPTIONAL_ACCESSORS(WasmModuleObject, breakpoint_infos, FixedArray,
                   kBreakPointInfosOffset)
wasm::NativeModule* WasmModuleObject::native_module() const {
  return managed_native_module()->raw();
}
const wasm::WasmModule* WasmModuleObject::module() const {
  // TODO(clemensh): Remove this helper (inline in callers).
  return native_module()->module();
}
void WasmModuleObject::reset_breakpoint_infos() {
  WRITE_FIELD(this, kBreakPointInfosOffset,
              GetReadOnlyRoots().undefined_value());
}
bool WasmModuleObject::is_asm_js() {
  bool asm_js = module()->origin == wasm::kAsmJsOrigin;
  DCHECK_EQ(asm_js, script()->IsUserJavaScript());
  DCHECK_EQ(asm_js, has_asm_js_offset_table());
  return asm_js;
}

// WasmTableObject
ACCESSORS(WasmTableObject, functions, FixedArray, kFunctionsOffset)
ACCESSORS(WasmTableObject, maximum_length, Object, kMaximumLengthOffset)
ACCESSORS(WasmTableObject, dispatch_tables, FixedArray, kDispatchTablesOffset)

// WasmMemoryObject
ACCESSORS(WasmMemoryObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmMemoryObject, maximum_pages, kMaximumPagesOffset)
OPTIONAL_ACCESSORS(WasmMemoryObject, instances, WeakArrayList, kInstancesOffset)

// WasmGlobalObject
ACCESSORS(WasmGlobalObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmGlobalObject, offset, kOffsetOffset)
SMI_ACCESSORS(WasmGlobalObject, flags, kFlagsOffset)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, type, WasmGlobalObject::TypeBits)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, is_mutable,
                    WasmGlobalObject::IsMutableBit)

int WasmGlobalObject::type_size() const {
  return wasm::ValueTypes::ElementSizeInBytes(type());
}

Address WasmGlobalObject::address() const {
  DCHECK_LE(offset() + type_size(), array_buffer()->byte_length());
  return Address(array_buffer()->backing_store()) + offset();
}

int32_t WasmGlobalObject::GetI32() {
  return ReadLittleEndianValue<int32_t>(address());
}

int64_t WasmGlobalObject::GetI64() {
  return ReadLittleEndianValue<int64_t>(address());
}

float WasmGlobalObject::GetF32() {
  return ReadLittleEndianValue<float>(address());
}

double WasmGlobalObject::GetF64() {
  return ReadLittleEndianValue<double>(address());
}

void WasmGlobalObject::SetI32(int32_t value) {
  WriteLittleEndianValue<int32_t>(address(), value);
}

void WasmGlobalObject::SetI64(int64_t value) {
  WriteLittleEndianValue<int64_t>(address(), value);
}

void WasmGlobalObject::SetF32(float value) {
  WriteLittleEndianValue<float>(address(), value);
}

void WasmGlobalObject::SetF64(double value) {
  WriteLittleEndianValue<double>(address(), value);
}

// WasmInstanceObject
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_start, byte*, kMemoryStartOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_size, size_t, kMemorySizeOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_mask, size_t, kMemoryMaskOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, roots_array_address, Address,
                    kRootsArrayAddressOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, stack_limit_address, Address,
                    kStackLimitAddressOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, real_stack_limit_address, Address,
                    kRealStackLimitAddressOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, imported_function_targets, Address*,
                    kImportedFunctionTargetsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, globals_start, byte*,
                    kGlobalsStartOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, imported_mutable_globals, Address*,
                    kImportedMutableGlobalsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_size, uint32_t,
                    kIndirectFunctionTableSizeOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_sig_ids,
                    uint32_t*, kIndirectFunctionTableSigIdsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_targets,
                    Address*, kIndirectFunctionTableTargetsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, jump_table_start, Address,
                    kJumpTableStartOffset)

ACCESSORS(WasmInstanceObject, module_object, WasmModuleObject,
          kModuleObjectOffset)
ACCESSORS(WasmInstanceObject, exports_object, JSObject, kExportsObjectOffset)
ACCESSORS(WasmInstanceObject, native_context, Context, kNativeContextOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, WasmMemoryObject,
                   kMemoryObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, globals_buffer, JSArrayBuffer,
                   kGlobalsBufferOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, imported_mutable_globals_buffers,
                   FixedArray, kImportedMutableGlobalsBuffersOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, debug_info, WasmDebugInfo,
                   kDebugInfoOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, table_object, WasmTableObject,
                   kTableObjectOffset)
ACCESSORS(WasmInstanceObject, imported_function_instances, FixedArray,
          kImportedFunctionInstancesOffset)
ACCESSORS(WasmInstanceObject, imported_function_callables, FixedArray,
          kImportedFunctionCallablesOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, indirect_function_table_instances,
                   FixedArray, kIndirectFunctionTableInstancesOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, managed_native_allocations, Foreign,
                   kManagedNativeAllocationsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, exceptions_table, FixedArray,
                   kExceptionsTableOffset)
ACCESSORS(WasmInstanceObject, undefined_value, Oddball, kUndefinedValueOffset)
ACCESSORS(WasmInstanceObject, null_value, Oddball, kNullValueOffset)
ACCESSORS(WasmInstanceObject, centry_stub, Code, kCEntryStubOffset)

inline bool WasmInstanceObject::has_indirect_function_table() {
  return indirect_function_table_sig_ids() != nullptr;
}

IndirectFunctionTableEntry::IndirectFunctionTableEntry(
    Handle<WasmInstanceObject> instance, int index)
    : instance_(instance), index_(index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, instance->indirect_function_table_size());
}

ImportedFunctionEntry::ImportedFunctionEntry(
    Handle<WasmInstanceObject> instance, int index)
    : instance_(instance), index_(index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, instance->module()->num_imported_functions);
}

// WasmExceptionObject
ACCESSORS(WasmExceptionObject, serialized_signature, PodArray<wasm::ValueType>,
          kSerializedSignatureOffset)
ACCESSORS(WasmExceptionObject, exception_tag, HeapObject, kExceptionTagOffset)

// WasmExportedFunctionData
ACCESSORS(WasmExportedFunctionData, wrapper_code, Code, kWrapperCodeOffset)
ACCESSORS(WasmExportedFunctionData, instance, WasmInstanceObject,
          kInstanceOffset)
SMI_ACCESSORS(WasmExportedFunctionData, jump_table_offset,
              kJumpTableOffsetOffset)
SMI_ACCESSORS(WasmExportedFunctionData, function_index, kFunctionIndexOffset)

// WasmDebugInfo
ACCESSORS(WasmDebugInfo, wasm_instance, WasmInstanceObject, kInstanceOffset)
ACCESSORS(WasmDebugInfo, interpreter_handle, Object, kInterpreterHandleOffset)
ACCESSORS(WasmDebugInfo, interpreted_functions, FixedArray,
          kInterpretedFunctionsOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, locals_names, FixedArray, kLocalsNamesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entries, FixedArray,
                   kCWasmEntriesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entry_map, Managed<wasm::SignatureMap>,
                   kCWasmEntryMapOffset)

#undef OPTIONAL_ACCESSORS
#undef READ_PRIMITIVE_FIELD
#undef WRITE_PRIMITIVE_FIELD
#undef PRIMITIVE_ACCESSORS

uint32_t WasmTableObject::current_length() { return functions()->length(); }

bool WasmMemoryObject::has_maximum_pages() { return maximum_pages() >= 0; }

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OBJECTS_INL_H_
