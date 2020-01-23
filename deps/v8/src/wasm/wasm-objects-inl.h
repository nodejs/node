// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_INL_H_
#define V8_WASM_WASM_OBJECTS_INL_H_

#include "src/wasm/wasm-objects.h"

#include "src/base/memory.h"
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
TQ_OBJECT_CONSTRUCTORS_IMPL(WasmExceptionTag)
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
CAST_ACCESSOR(WasmExportedFunctionData)
CAST_ACCESSOR(WasmGlobalObject)
CAST_ACCESSOR(WasmInstanceObject)
CAST_ACCESSOR(WasmMemoryObject)
CAST_ACCESSOR(WasmModuleObject)
CAST_ACCESSOR(WasmTableObject)
CAST_ACCESSOR(AsmWasmData)

#define OPTIONAL_ACCESSORS(holder, name, type, offset)                \
  DEF_GETTER(holder, has_##name, bool) {                              \
    Object value = TaggedField<Object, offset>::load(isolate, *this); \
    return !value.IsUndefined(GetReadOnlyRoots(isolate));             \
  }                                                                   \
  ACCESSORS(holder, name, type, offset)

#define PRIMITIVE_ACCESSORS(holder, name, type, offset)                       \
  type holder::name() const {                                                 \
    if (COMPRESS_POINTERS_BOOL && alignof(type) > kTaggedSize) {              \
      /* TODO(ishell, v8:8875): When pointer compression is enabled 8-byte */ \
      /* size fields (external pointers, doubles and BigInt data) are only */ \
      /* kTaggedSize aligned so we have to use unaligned pointer friendly  */ \
      /* way of accessing them in order to avoid undefined behavior in C++ */ \
      /* code. */                                                             \
      return base::ReadUnalignedValue<type>(FIELD_ADDR(*this, offset));       \
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
      base::WriteUnalignedValue<type>(FIELD_ADDR(*this, offset), value);      \
    } else {                                                                  \
      *reinterpret_cast<type*>(FIELD_ADDR(*this, offset)) = value;            \
    }                                                                         \
  }

// WasmModuleObject
ACCESSORS(WasmModuleObject, managed_native_module, Managed<wasm::NativeModule>,
          kNativeModuleOffset)
ACCESSORS(WasmModuleObject, export_wrappers, FixedArray, kExportWrappersOffset)
ACCESSORS(WasmModuleObject, script, Script, kScriptOffset)
OPTIONAL_ACCESSORS(WasmModuleObject, asm_js_offset_table, ByteArray,
                   kAsmJsOffsetTableOffset)
wasm::NativeModule* WasmModuleObject::native_module() const {
  return managed_native_module().raw();
}
const std::shared_ptr<wasm::NativeModule>&
WasmModuleObject::shared_native_module() const {
  return managed_native_module().get();
}
const wasm::WasmModule* WasmModuleObject::module() const {
  // TODO(clemensb): Remove this helper (inline in callers).
  return native_module()->module();
}
bool WasmModuleObject::is_asm_js() {
  bool asm_js = is_asmjs_module(module());
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
  return base::ReadLittleEndianValue<int32_t>(address());
}

int64_t WasmGlobalObject::GetI64() {
  return base::ReadLittleEndianValue<int64_t>(address());
}

float WasmGlobalObject::GetF32() {
  return base::ReadLittleEndianValue<float>(address());
}

double WasmGlobalObject::GetF64() {
  return base::ReadLittleEndianValue<double>(address());
}

Handle<Object> WasmGlobalObject::GetRef() {
  // We use this getter for anyref, funcref, and exnref.
  DCHECK(wasm::ValueTypes::IsReferenceType(type()));
  return handle(tagged_buffer().get(offset()), GetIsolate());
}

void WasmGlobalObject::SetI32(int32_t value) {
  base::WriteLittleEndianValue<int32_t>(address(), value);
}

void WasmGlobalObject::SetI64(int64_t value) {
  base::WriteLittleEndianValue<int64_t>(address(), value);
}

void WasmGlobalObject::SetF32(float value) {
  base::WriteLittleEndianValue<float>(address(), value);
}

void WasmGlobalObject::SetF64(double value) {
  base::WriteLittleEndianValue<double>(address(), value);
}

void WasmGlobalObject::SetAnyRef(Handle<Object> value) {
  // We use this getter anyref and exnref.
  DCHECK(type() == wasm::kWasmAnyRef || type() == wasm::kWasmExnRef);
  tagged_buffer().set(offset(), *value);
}

bool WasmGlobalObject::SetFuncRef(Isolate* isolate, Handle<Object> value) {
  DCHECK_EQ(type(), wasm::kWasmFuncRef);
  if (!value->IsNull(isolate) &&
      !WasmExportedFunction::IsWasmExportedFunction(*value) &&
      !WasmCapiFunction::IsWasmCapiFunction(*value)) {
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
OPTIONAL_ACCESSORS(WasmInstanceObject, indirect_function_tables, FixedArray,
                   kIndirectFunctionTablesOffset)
ACCESSORS(WasmInstanceObject, imported_function_refs, FixedArray,
          kImportedFunctionRefsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, indirect_function_table_refs, FixedArray,
                   kIndirectFunctionTableRefsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, managed_native_allocations, Foreign,
                   kManagedNativeAllocationsOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, exceptions_table, FixedArray,
                   kExceptionsTableOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, wasm_external_functions, FixedArray,
                   kWasmExternalFunctionsOffset)

void WasmInstanceObject::clear_padding() {
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }
}

IndirectFunctionTableEntry::IndirectFunctionTableEntry(
    Handle<WasmInstanceObject> instance, int table_index, int entry_index)
    : instance_(table_index == 0 ? instance
                                 : Handle<WasmInstanceObject>::null()),
      table_(table_index != 0
                 ? handle(WasmIndirectFunctionTable::cast(
                              instance->indirect_function_tables().get(
                                  table_index)),
                          instance->GetIsolate())
                 : Handle<WasmIndirectFunctionTable>::null()),
      index_(entry_index) {
  DCHECK_GE(entry_index, 0);
  DCHECK_LT(entry_index, table_index == 0
                             ? instance->indirect_function_table_size()
                             : table_->size());
}

IndirectFunctionTableEntry::IndirectFunctionTableEntry(
    Handle<WasmIndirectFunctionTable> table, int entry_index)
    : instance_(Handle<WasmInstanceObject>::null()),
      table_(table),
      index_(entry_index) {
  DCHECK_GE(entry_index, 0);
  DCHECK_LT(entry_index, table_->size());
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

// WasmExceptionPackage
OBJECT_CONSTRUCTORS_IMPL(WasmExceptionPackage, JSReceiver)
CAST_ACCESSOR(WasmExceptionPackage)

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
ACCESSORS(WasmExportedFunctionData, c_wrapper_code, Object, kCWrapperCodeOffset)
ACCESSORS(WasmExportedFunctionData, wasm_call_target, Object,
          kWasmCallTargetOffset)
SMI_ACCESSORS(WasmExportedFunctionData, packed_args_size, kPackedArgsSizeOffset)

// WasmJSFunction
WasmJSFunction::WasmJSFunction(Address ptr) : JSFunction(ptr) {
  SLOW_DCHECK(IsWasmJSFunction(*this));
}
CAST_ACCESSOR(WasmJSFunction)

// WasmJSFunctionData
OBJECT_CONSTRUCTORS_IMPL(WasmJSFunctionData, Struct)
CAST_ACCESSOR(WasmJSFunctionData)
SMI_ACCESSORS(WasmJSFunctionData, serialized_return_count,
              kSerializedReturnCountOffset)
SMI_ACCESSORS(WasmJSFunctionData, serialized_parameter_count,
              kSerializedParameterCountOffset)
ACCESSORS(WasmJSFunctionData, serialized_signature, PodArray<wasm::ValueType>,
          kSerializedSignatureOffset)
ACCESSORS(WasmJSFunctionData, callable, JSReceiver, kCallableOffset)
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
ACCESSORS(WasmCapiFunctionData, embedder_data, Foreign, kEmbedderDataOffset)
ACCESSORS(WasmCapiFunctionData, wrapper_code, Code, kWrapperCodeOffset)
ACCESSORS(WasmCapiFunctionData, serialized_signature, PodArray<wasm::ValueType>,
          kSerializedSignatureOffset)

// WasmExternalFunction
WasmExternalFunction::WasmExternalFunction(Address ptr) : JSFunction(ptr) {
  SLOW_DCHECK(IsWasmExternalFunction(*this));
}
CAST_ACCESSOR(WasmExternalFunction)

// WasmIndirectFunctionTable
OBJECT_CONSTRUCTORS_IMPL(WasmIndirectFunctionTable, Struct)
CAST_ACCESSOR(WasmIndirectFunctionTable)
PRIMITIVE_ACCESSORS(WasmIndirectFunctionTable, size, uint32_t, kSizeOffset)
PRIMITIVE_ACCESSORS(WasmIndirectFunctionTable, sig_ids, uint32_t*,
                    kSigIdsOffset)
PRIMITIVE_ACCESSORS(WasmIndirectFunctionTable, targets, Address*,
                    kTargetsOffset)
OPTIONAL_ACCESSORS(WasmIndirectFunctionTable, managed_native_allocations,
                   Foreign, kManagedNativeAllocationsOffset)
ACCESSORS(WasmIndirectFunctionTable, refs, FixedArray, kRefsOffset)

// WasmDebugInfo
ACCESSORS(WasmDebugInfo, wasm_instance, WasmInstanceObject, kInstanceOffset)
ACCESSORS(WasmDebugInfo, interpreter_handle, Object, kInterpreterHandleOffset)
ACCESSORS(WasmDebugInfo, interpreter_reference_stack, Cell,
          kInterpreterReferenceStackOffset)
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
TQ_SMI_ACCESSORS(WasmExceptionTag, index)

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
