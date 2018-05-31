// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_INL_H_
#define V8_WASM_WASM_OBJECTS_INL_H_

#include "src/heap/heap-inl.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

CAST_ACCESSOR(WasmCompiledModule)
CAST_ACCESSOR(WasmDebugInfo)
CAST_ACCESSOR(WasmGlobalObject)
CAST_ACCESSOR(WasmInstanceObject)
CAST_ACCESSOR(WasmMemoryObject)
CAST_ACCESSOR(WasmModuleObject)
CAST_ACCESSOR(WasmSharedModuleData)
CAST_ACCESSOR(WasmTableObject)

#define OPTIONAL_ACCESSORS(holder, name, type, offset)           \
  bool holder::has_##name() {                                    \
    return !READ_FIELD(this, offset)->IsUndefined(GetIsolate()); \
  }                                                              \
  ACCESSORS(holder, name, type, offset)

#define READ_PRIMITIVE_FIELD(p, type, offset) \
  (*reinterpret_cast<type const*>(FIELD_ADDR_CONST(p, offset)))

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
ACCESSORS(WasmModuleObject, compiled_module, WasmCompiledModule,
          kCompiledModuleOffset)

// WasmTableObject
ACCESSORS(WasmTableObject, functions, FixedArray, kFunctionsOffset)
ACCESSORS(WasmTableObject, maximum_length, Object, kMaximumLengthOffset)
ACCESSORS(WasmTableObject, dispatch_tables, FixedArray, kDispatchTablesOffset)

// WasmMemoryObject
ACCESSORS(WasmMemoryObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmMemoryObject, maximum_pages, kMaximumPagesOffset)
OPTIONAL_ACCESSORS(WasmMemoryObject, instances, FixedArrayOfWeakCells,
                   kInstancesOffset)

// WasmGlobalObject
ACCESSORS(WasmGlobalObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmGlobalObject, offset, kOffsetOffset)
SMI_ACCESSORS(WasmGlobalObject, flags, kFlagsOffset)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, type, WasmGlobalObject::TypeBits)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, is_mutable,
                    WasmGlobalObject::IsMutableBit)

// static
uint32_t WasmGlobalObject::TypeSize(wasm::ValueType type) {
  return 1U << ElementSizeLog2Of(type);
}

uint32_t WasmGlobalObject::type_size() const { return TypeSize(type()); }

Address WasmGlobalObject::address() const {
  uint32_t buffer_size = 0;
  DCHECK(array_buffer()->byte_length()->ToUint32(&buffer_size));
  DCHECK(offset() + type_size() <= buffer_size);
  USE(buffer_size);
  return Address(array_buffer()->backing_store()) + offset();
}

int32_t WasmGlobalObject::GetI32() { return Memory::int32_at(address()); }

float WasmGlobalObject::GetF32() { return Memory::float_at(address()); }

double WasmGlobalObject::GetF64() { return Memory::double_at(address()); }

void WasmGlobalObject::SetI32(int32_t value) {
  Memory::int32_at(address()) = value;
}

void WasmGlobalObject::SetF32(float value) {
  Memory::float_at(address()) = value;
}

void WasmGlobalObject::SetF64(double value) {
  Memory::double_at(address()) = value;
}

// WasmInstanceObject
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_start, byte*, kMemoryStartOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_size, uint32_t,
                    kMemorySizeOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_mask, uint32_t,
                    kMemoryMaskOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, imported_function_targets, Address*,
                    kImportedFunctionTargetsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, globals_start, byte*,
                    kGlobalsStartOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_size, uint32_t,
                    kIndirectFunctionTableSizeOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_sig_ids,
                    uint32_t*, kIndirectFunctionTableSigIdsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, indirect_function_table_targets,
                    Address*, kIndirectFunctionTableTargetsOffset)

ACCESSORS(WasmInstanceObject, compiled_module, WasmCompiledModule,
          kCompiledModuleOffset)
ACCESSORS(WasmInstanceObject, exports_object, JSObject, kExportsObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, WasmMemoryObject,
                   kMemoryObjectOffset)
ACCESSORS(WasmInstanceObject, globals_buffer, JSArrayBuffer,
          kGlobalsBufferOffset)
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
ACCESSORS(WasmInstanceObject, managed_native_allocations, Foreign,
          kManagedNativeAllocationsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, managed_indirect_patcher, Foreign,
                   kManagedIndirectPatcherOffset)

inline bool WasmInstanceObject::has_indirect_function_table() {
  return indirect_function_table_sig_ids() != nullptr;
}

IndirectFunctionTableEntry::IndirectFunctionTableEntry(
    WasmInstanceObject* instance, int index)
    : instance_(instance), index_(index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, instance->indirect_function_table_size());
}

ImportedFunctionEntry::ImportedFunctionEntry(WasmInstanceObject* instance,
                                             int index)
    : instance_(instance), index_(index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, instance->module()->num_imported_functions);
}

// WasmSharedModuleData
ACCESSORS(WasmSharedModuleData, module_wrapper, Object, kModuleWrapperOffset)
ACCESSORS(WasmSharedModuleData, module_bytes, SeqOneByteString,
          kModuleBytesOffset)
ACCESSORS(WasmSharedModuleData, script, Script, kScriptOffset)
OPTIONAL_ACCESSORS(WasmSharedModuleData, asm_js_offset_table, ByteArray,
                   kAsmJsOffsetTableOffset)
OPTIONAL_ACCESSORS(WasmSharedModuleData, breakpoint_infos, FixedArray,
                   kBreakPointInfosOffset)
void WasmSharedModuleData::reset_breakpoint_infos() {
  DCHECK(IsWasmSharedModuleData());
  WRITE_FIELD(this, kBreakPointInfosOffset, GetHeap()->undefined_value());
}

// WasmDebugInfo
ACCESSORS(WasmDebugInfo, wasm_instance, WasmInstanceObject, kInstanceOffset)
ACCESSORS(WasmDebugInfo, interpreter_handle, Object, kInterpreterHandleOffset)
ACCESSORS(WasmDebugInfo, interpreted_functions, Object,
          kInterpretedFunctionsOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, locals_names, FixedArray, kLocalsNamesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entries, FixedArray,
                   kCWasmEntriesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entry_map, Managed<wasm::SignatureMap>,
                   kCWasmEntryMapOffset)

#undef OPTIONAL_ACCESSORS

#define WCM_OBJECT_OR_WEAK(TYPE, NAME, OFFSET, TYPE_CHECK)   \
  bool WasmCompiledModule::has_##NAME() const {              \
    Object* value = READ_FIELD(this, OFFSET);                \
    return TYPE_CHECK;                                       \
  }                                                          \
                                                             \
  void WasmCompiledModule::reset_##NAME() {                  \
    WRITE_FIELD(this, OFFSET, GetHeap()->undefined_value()); \
  }                                                          \
                                                             \
  ACCESSORS_CHECKED2(WasmCompiledModule, NAME, TYPE, OFFSET, TYPE_CHECK, true)

#define WCM_OBJECT(TYPE, NAME, OFFSET) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, OFFSET, value->Is##TYPE())

#define WCM_SMALL_CONST_NUMBER(TYPE, NAME, OFFSET)                  \
  TYPE WasmCompiledModule::NAME() const {                           \
    return static_cast<TYPE>(Smi::ToInt(READ_FIELD(this, OFFSET))); \
  }                                                                 \
                                                                    \
  void WasmCompiledModule::set_##NAME(TYPE value) {                 \
    WRITE_FIELD(this, OFFSET, Smi::FromInt(value));                 \
  }

#define WCM_WEAK_LINK(TYPE, NAME, OFFSET)                                \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, OFFSET, value->IsWeakCell()) \
                                                                         \
  TYPE* WasmCompiledModule::NAME() const {                               \
    DCHECK(!weak_##NAME()->cleared());                                   \
    return TYPE::cast(weak_##NAME()->value());                           \
  }

// WasmCompiledModule
WCM_OBJECT(WasmSharedModuleData, shared, kSharedOffset)
WCM_WEAK_LINK(Context, native_context, kNativeContextOffset)
WCM_OBJECT(FixedArray, export_wrappers, kExportWrappersOffset)
WCM_OBJECT(WasmCompiledModule, next_instance, kNextInstanceOffset)
WCM_OBJECT(WasmCompiledModule, prev_instance, kPrevInstanceOffset)
WCM_WEAK_LINK(WasmInstanceObject, owning_instance, kOwningInstanceOffset)
WCM_WEAK_LINK(WasmModuleObject, wasm_module, kWasmModuleOffset)
WCM_OBJECT(Foreign, native_module, kNativeModuleOffset)
WCM_SMALL_CONST_NUMBER(bool, use_trap_handler, kUseTrapHandlerOffset)
ACCESSORS(WasmCompiledModule, raw_next_instance, Object, kNextInstanceOffset);
ACCESSORS(WasmCompiledModule, raw_prev_instance, Object, kPrevInstanceOffset);

#undef WCM_OBJECT_OR_WEAK
#undef WCM_OBJECT
#undef WCM_SMALL_CONST_NUMBER
#undef WCM_WEAK_LINK
#undef READ_PRIMITIVE_FIELD
#undef WRITE_PRIMITIVE_FIELD
#undef PRIMITIVE_ACCESSORS

uint32_t WasmTableObject::current_length() { return functions()->length(); }

bool WasmMemoryObject::has_maximum_pages() { return maximum_pages() >= 0; }

inline bool WasmCompiledModule::has_instance() const {
  return !weak_owning_instance()->cleared();
}

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OBJECTS_INL_H_
