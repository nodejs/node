// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_INL_H_
#define V8_WASM_WASM_OBJECTS_INL_H_

#include "src/wasm/wasm-objects.h"

#include "src/common/v8memory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/managed.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/script-inl.h"
#include "src/roots/roots.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(WasmExceptionObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WasmExceptionTag, Struct)
OBJECT_CONSTRUCTORS_IMPL(WasmExportedFunctionData, Struct)
OBJECT_CONSTRUCTORS_IMPL(WasmDebugInfo, Struct)
OBJECT_CONSTRUCTORS_IMPL(WasmGlobalObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WasmInstanceObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WasmMemoryObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WasmModuleObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(WasmTableObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(AsmWasmData, Struct)

NEVER_READ_ONLY_SPACE_IMPL(WasmDebugInfo)

CAST_ACCESSOR(WasmDebugInfo)
CAST_ACCESSOR(WasmExceptionObject)
CAST_ACCESSOR(WasmExceptionTag)
CAST_ACCESSOR(WasmExportedFunctionData)
CAST_ACCESSOR(WasmGlobalObject)
CAST_ACCESSOR(WasmInstanceObject)
CAST_ACCESSOR(WasmMemoryObject)
CAST_ACCESSOR(WasmModuleObject)
CAST_ACCESSOR(WasmTableObject)
CAST_ACCESSOR(AsmWasmData)

#define OPTIONAL_ACCESSORS(holder, name, type, offset) \
  bool holder::has_##name() {                          \
    return !READ_FIELD(*this, offset).IsUndefined();   \
  }                                                    \
  ACCESSORS(holder, name, type, offset)

#define PRIMITIVE_ACCESSORS(holder, name, type, offset)                       \
  type holder::name() const {                                                 \
    if (COMPRESS_POINTERS_BOOL && alignof(type) > kTaggedSize) {              \
      /* TODO(ishell, v8:8875): When pointer compression is enabled 8-byte */ \
      /* size fields (external pointers, doubles and BigInt data) are only */ \
      /* kTaggedSize aligned so we have to use unaligned pointer friendly  */ \
      /* way of accessing them in order to avoid undefined behavior in C++ */ \
      /* code. */                                                             \
      return ReadUnalignedValue<type>(FIELD_ADDR(*this, offset));             \
    } else {                                                                  \
      return *reinterpret_cast<type const*>(FIELD_ADDR(*this, offset));       \
    }                                                                         \
  }                                                                           \
  void holder::set_##name(type value) {                                       \
    if (COMPRESS_POINTERS_BOOL && alignof(type) > kTaggedSize) {              \
      /* TODO(ishell, v8:8875): When pointer compression is enabled 8-byte */ \
      /* size fields (external pointers, doubles and BigInt data) are only */ \
      /* kTaggedSize aligned so we have to use unaligned pointer friendly  */ \
      /* way of accessing them in order to avoid undefined behavior in C++ */ \
      /* code. */                                                             \
      WriteUnalignedValue<type>(FIELD_ADDR(*this, offset), value);            \
    } else {                                                                  \
      *reinterpret_cast<type*>(FIELD_ADDR(*this, offset)) = value;            \
    }                                                                         \
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
  return managed_native_module().raw();
}
const std::shared_ptr<wasm::NativeModule>&
WasmModuleObject::shared_native_module() const {
  return managed_native_module().get();
}
const wasm::WasmModule* WasmModuleObject::module() const {
  // TODO(clemensh): Remove this helper (inline in callers).
  return native_module()->module();
}
void WasmModuleObject::reset_breakpoint_infos() {
  WRITE_FIELD(*this, kBreakPointInfosOffset,
              GetReadOnlyRoots().undefined_value());
}
bool WasmModuleObject::is_asm_js() {
  bool asm_js = module()->origin == wasm::kAsmJsOrigin;
  DCHECK_EQ(asm_js, script().IsUserJavaScript());
  DCHECK_EQ(asm_js, has_asm_js_offset_table());
  return asm_js;
}

// WasmTableObject
ACCESSORS(WasmTableObject, entries, FixedArray, kEntriesOffset)
ACCESSORS(WasmTableObject, maximum_length, Object, kMaximumLengthOffset)
ACCESSORS(WasmTableObject, dispatch_tables, FixedArray, kDispatchTablesOffset)
SMI_ACCESSORS(WasmTableObject, raw_type, kRawTypeOffset)

// WasmMemoryObject
ACCESSORS(WasmMemoryObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmMemoryObject, maximum_pages, kMaximumPagesOffset)
OPTIONAL_ACCESSORS(WasmMemoryObject, instances, WeakArrayList, kInstancesOffset)

// WasmGlobalObject
ACCESSORS(WasmGlobalObject, untagged_buffer, JSArrayBuffer,
          kUntaggedBufferOffset)
ACCESSORS(WasmGlobalObject, tagged_buffer, FixedArray, kTaggedBufferOffset)
SMI_ACCESSORS(WasmGlobalObject, offset, kOffsetOffset)
SMI_ACCESSORS(WasmGlobalObject, flags, kFlagsOffset)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, type, WasmGlobalObject::TypeBits)
BIT_FIELD_ACCESSORS(WasmGlobalObject, flags, is_mutable,
                    WasmGlobalObject::IsMutableBit)

int WasmGlobalObject::type_size() const {
  return wasm::ValueTypes::ElementSizeInBytes(type());
}

Address WasmGlobalObject::address() const {
  DCHECK_NE(type(), wasm::kWasmAnyRef);
  DCHECK_LE(offset() + type_size(), untagged_buffer().byte_length());
  return Address(untagged_buffer().backing_store()) + offset();
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

Handle<Object> WasmGlobalObject::GetRef() {
  // We use this getter for anyref, anyfunc, and except_ref.
  DCHECK(wasm::ValueTypes::IsReferenceType(type()));
  return handle(tagged_buffer().get(offset()), GetIsolate());
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

void WasmGlobalObject::SetAnyRef(Handle<Object> value) {
  // We use this getter anyref and except_ref.
  DCHECK(type() == wasm::kWasmAnyRef || type() == wasm::kWasmExceptRef);
  tagged_buffer().set(offset(), *value);
}

bool WasmGlobalObject::SetAnyFunc(Isolate* isolate, Handle<Object> value) {
  DCHECK_EQ(type(), wasm::kWasmAnyFunc);
  if (!value->IsNull(isolate) &&
      !WasmExportedFunction::IsWasmExportedFunction(*value)) {
    return false;
  }
  tagged_buffer().set(offset(), *value);
  return true;
}

// WasmInstanceObject
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_start, byte*, kMemoryStartOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_size, size_t, kMemorySizeOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, memory_mask, size_t, kMemoryMaskOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, isolate_root, Address,
                    kIsolateRootOffset)
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
PRIMITIVE_ACCESSORS(WasmInstanceObject, data_segment_starts, Address*,
                    kDataSegmentStartsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, data_segment_sizes, uint32_t*,
                    kDataSegmentSizesOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, dropped_data_segments, byte*,
                    kDroppedDataSegmentsOffset)
PRIMITIVE_ACCESSORS(WasmInstanceObject, dropped_elem_segments, byte*,
                    kDroppedElemSegmentsOffset)

ACCESSORS(WasmInstanceObject, module_object, WasmModuleObject,
          kModuleObjectOffset)
ACCESSORS(WasmInstanceObject, exports_object, JSObject, kExportsObjectOffset)
ACCESSORS(WasmInstanceObject, native_context, Context, kNativeContextOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, WasmMemoryObject,
                   kMemoryObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, untagged_globals_buffer, JSArrayBuffer,
                   kUntaggedGlobalsBufferOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, tagged_globals_buffer, FixedArray,
                   kTaggedGlobalsBufferOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, imported_mutable_globals_buffers,
                   FixedArray, kImportedMutableGlobalsBuffersOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, debug_info, WasmDebugInfo,
                   kDebugInfoOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, tables, FixedArray, kTablesOffset)
ACCESSORS(WasmInstanceObject, imported_function_refs, FixedArray,
          kImportedFunctionRefsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, indirect_function_table_refs, FixedArray,
                   kIndirectFunctionTableRefsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, managed_native_allocations, Foreign,
                   kManagedNativeAllocationsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, exceptions_table, FixedArray,
                   kExceptionsTableOffset)
ACCESSORS(WasmInstanceObject, undefined_value, Oddball, kUndefinedValueOffset)
ACCESSORS(WasmInstanceObject, null_value, Oddball, kNullValueOffset)
ACCESSORS(WasmInstanceObject, centry_stub, Code, kCEntryStubOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, wasm_exported_functions, FixedArray,
                   kWasmExportedFunctionsOffset)

inline bool WasmInstanceObject::has_indirect_function_table() {
  return indirect_function_table_sig_ids() != nullptr;
}

void WasmInstanceObject::clear_padding() {
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }
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

// WasmExportedFunction
WasmExportedFunction::WasmExportedFunction(Address ptr) : JSFunction(ptr) {
  SLOW_DCHECK(IsWasmExportedFunction(*this));
}
CAST_ACCESSOR(WasmExportedFunction)

// WasmExportedFunctionData
ACCESSORS(WasmExportedFunctionData, wrapper_code, Code, kWrapperCodeOffset)
ACCESSORS(WasmExportedFunctionData, instance, WasmInstanceObject,
          kInstanceOffset)
SMI_ACCESSORS(WasmExportedFunctionData, jump_table_offset,
              kJumpTableOffsetOffset)
SMI_ACCESSORS(WasmExportedFunctionData, function_index, kFunctionIndexOffset)

// WasmJSFunction
WasmJSFunction::WasmJSFunction(Address ptr) : JSFunction(ptr) {
  SLOW_DCHECK(IsWasmJSFunction(*this));
}
CAST_ACCESSOR(WasmJSFunction)

// WasmJSFunctionData
OBJECT_CONSTRUCTORS_IMPL(WasmJSFunctionData, Struct)
CAST_ACCESSOR(WasmJSFunctionData)
ACCESSORS(WasmJSFunctionData, wrapper_code, Code, kWrapperCodeOffset)

// WasmCapiFunction
WasmCapiFunction::WasmCapiFunction(Address ptr) : JSFunction(ptr) {
  SLOW_DCHECK(IsWasmCapiFunction(*this));
}
CAST_ACCESSOR(WasmCapiFunction)

// WasmCapiFunctionData
OBJECT_CONSTRUCTORS_IMPL(WasmCapiFunctionData, Struct)
CAST_ACCESSOR(WasmCapiFunctionData)
PRIMITIVE_ACCESSORS(WasmCapiFunctionData, call_target, Address,
                    kCallTargetOffset)
PRIMITIVE_ACCESSORS(WasmCapiFunctionData, embedder_data, void*,
                    kEmbedderDataOffset)
ACCESSORS(WasmCapiFunctionData, wrapper_code, Code, kWrapperCodeOffset)
ACCESSORS(WasmCapiFunctionData, serialized_signature, PodArray<wasm::ValueType>,
          kSerializedSignatureOffset)

// WasmDebugInfo
ACCESSORS(WasmDebugInfo, wasm_instance, WasmInstanceObject, kInstanceOffset)
ACCESSORS(WasmDebugInfo, interpreter_handle, Object, kInterpreterHandleOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, locals_names, FixedArray, kLocalsNamesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entries, FixedArray,
                   kCWasmEntriesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entry_map, Managed<wasm::SignatureMap>,
                   kCWasmEntryMapOffset)

#undef OPTIONAL_ACCESSORS
#undef READ_PRIMITIVE_FIELD
#undef WRITE_PRIMITIVE_FIELD
#undef PRIMITIVE_ACCESSORS

uint32_t WasmTableObject::current_length() { return entries().length(); }

wasm::ValueType WasmTableObject::type() {
  return static_cast<wasm::ValueType>(raw_type());
}

bool WasmMemoryObject::has_maximum_pages() { return maximum_pages() >= 0; }

// WasmExceptionTag
SMI_ACCESSORS(WasmExceptionTag, index, kIndexOffset)

// AsmWasmData
ACCESSORS(AsmWasmData, managed_native_module, Managed<wasm::NativeModule>,
          kManagedNativeModuleOffset)
ACCESSORS(AsmWasmData, export_wrappers, FixedArray, kExportWrappersOffset)
ACCESSORS(AsmWasmData, asm_js_offset_table, ByteArray, kAsmJsOffsetTableOffset)
ACCESSORS(AsmWasmData, uses_bitset, HeapNumber, kUsesBitsetOffset)

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OBJECTS_INL_H_
