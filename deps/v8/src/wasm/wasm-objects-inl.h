// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_INL_H_
#define V8_WASM_WASM_OBJECTS_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/wasm-objects.h"
// Include the non-inl header before the rest of the headers.

#include <type_traits>

#include "src/base/memory.h"
#include "src/common/ptr-compr.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/trusted-pointer-inl.h"
#include "src/roots/roots.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"
#include "third_party/fp16/src/include/fp16.h"

#if V8_ENABLE_DRUMBRAKE
#include "src/wasm/interpreter/wasm-interpreter-objects.h"
#endif  // V8_ENABLE_DRUMBRAKE

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/wasm/wasm-objects-tq-inl.inc"

#define OPTIONAL_ACCESSORS(holder, name, type, offset)       \
  DEF_GETTER(holder, has_##name, bool) {                     \
    Tagged<Object> value =                                   \
        TaggedField<Object, offset>::load(cage_base, *this); \
    return !IsUndefined(value);                              \
  }                                                          \
  ACCESSORS_CHECKED2(holder, name, type, offset, !IsUndefined(value), true)

#define PRIMITIVE_ACCESSORS(holder, name, type, offset)               \
  type holder::name() const {                                         \
    return ReadMaybeUnalignedValue<type>(FIELD_ADDR(*this, offset));  \
  }                                                                   \
  void holder::set_##name(type value) {                               \
    WriteMaybeUnalignedValue<type>(FIELD_ADDR(*this, offset), value); \
  }

// WasmModuleObject
Managed<wasm::NativeModule>::Ptr WasmModuleObject::native_module() const {
  return managed_native_module()->ptr();
}
const std::shared_ptr<wasm::NativeModule>&
WasmModuleObject::shared_native_module() const {
  return managed_native_module()->get();
}

// WasmMemoryObject
ACCESSORS(WasmMemoryObject, instances, Tagged<WeakArrayList>, kInstancesOffset)

const std::shared_ptr<BackingStore>& WasmMemoryObject::backing_store() const {
  return managed_backing_store()->get();
}

// WasmGlobalObject
ACCESSORS(WasmGlobalObject, buffer, Tagged<WasmGlobalObject::BufferType>,
          kBufferOffset)
TRUSTED_POINTER_ACCESSORS(WasmGlobalObject, trusted_data,
                          WasmTrustedInstanceData, kTrustedDataOffset,
                          kWasmTrustedInstanceDataIndirectPointerTag)

wasm::ValueType WasmGlobalObject::unsafe_type() const {
  // Various consumers of ValueKind (e.g. ValueKind::name()) use the raw enum
  // value as index into a global array. As such, if the index is corrupted
  // (which must be assumed, as it comes from within the sandbox), this can
  // lead to out-of-bounds reads outside the sandbox. While these are not
  // technically sandbox violations, we should still try to avoid them to keep
  // fuzzers happy. This SBXCHECK accomplishes that.
  wasm::ValueType type = wasm::ValueType::FromRawBitField(raw_type());
  SBXCHECK(type.is_valid());
  return type;
}
void WasmGlobalObject::set_unsafe_type(wasm::ValueType value) {
  set_raw_type(static_cast<int>(value.raw_bit_field()));
}

int32_t WasmGlobalObject::GetI32() const {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kI32);
  return base::ReadUnalignedValue<int32_t>(storage());
}

int64_t WasmGlobalObject::GetI64() const {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kI64);
  return base::ReadUnalignedValue<int64_t>(storage());
}

float WasmGlobalObject::GetF32() const {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kF32);
  return base::ReadUnalignedValue<float>(storage());
}

double WasmGlobalObject::GetF64() const {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kF64);
  return base::ReadUnalignedValue<double>(storage());
}

uint8_t* WasmGlobalObject::GetS128RawBytes() const {
  return reinterpret_cast<uint8_t*>(storage());
}

DirectHandle<Object> WasmGlobalObject::GetRef() const {
  // We use this getter for externref, funcref, and stringref.
  DCHECK(unsafe_type().is_ref());
  return direct_handle(ObjectSlot{storage()}.load(), Isolate::Current());
}

void WasmGlobalObject::SetI32(int32_t value) {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kI32);
  base::WriteUnalignedValue(storage(), value);
}

void WasmGlobalObject::SetI64(int64_t value) {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kI64);
  base::WriteUnalignedValue(storage(), value);
}

void WasmGlobalObject::SetF32(float value) {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kF32);
  base::WriteUnalignedValue(storage(), value);
}

void WasmGlobalObject::SetF64(double value) {
  DCHECK(unsafe_type().is_numeric());
  DCHECK_EQ(unsafe_type().numeric_kind(), wasm::NumericKind::kF64);
  base::WriteUnalignedValue(storage(), value);
}

void WasmGlobalObject::SetRef(DirectHandle<Object> value) {
  DCHECK(unsafe_type().is_ref());
  MaybeObjectSlot slot{storage()};
  // Use relaxed store (like `FixedArray::set`) to avoid races with concurrent
  // marking.
  slot.Relaxed_Store(*value);
  WriteBarrier::ForValue(buffer(), slot, *value, UPDATE_WRITE_BARRIER);
}

Address WasmGlobalObject::storage() const {
  // Quick verification that the returned pointer is within the buffer's data.
  DCHECK_LE(unsafe_type().is_ref() ? FixedArray::OffsetOfElementAt(0)
                                   : ByteArray::OffsetOfElementAt(0),
            offset() + kHeapObjectTag);
  DCHECK_LE(offset() + kHeapObjectTag + unsafe_type().value_kind_size(),
            buffer()->Size());
  return buffer()->ptr() + offset();
}

// WasmTrustedInstanceData

PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, memory0_start, uint8_t*,
                    kMemory0StartOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, memory0_size, size_t,
                    kMemory0SizeOffset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, managed_native_module,
                            TrustedManaged<wasm::NativeModule>,
                            kProtectedManagedNativeModuleOffset)
ACCESSORS(WasmTrustedInstanceData, imported_mutable_globals_buffers,
          Tagged<FixedArray>, kImportedMutableGlobalsBuffersOffset)
ACCESSORS(WasmTrustedInstanceData, imported_mutable_globals_offsets,
          Tagged<FixedUInt32Array>, kImportedMutableGlobalsOffsetsOffset)
#if V8_ENABLE_DRUMBRAKE
ACCESSORS(WasmTrustedInstanceData, imported_function_indices,
          Tagged<FixedInt32Array>, kImportedFunctionIndicesOffset)
#endif  // V8_ENABLE_DRUMBRAKE
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, jump_table_start, Address,
                    kJumpTableStartOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, hook_on_function_call_address,
                    Address, kHookOnFunctionCallAddressOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, tiering_budget_array,
                    std::atomic<uint32_t>*, kTieringBudgetArrayOffset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, memory_bases_and_sizes,
                            TrustedFixedAddressArray,
                            kProtectedMemoryBasesAndSizesOffset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, data_segments,
                            TrustedPodArray<wasm::WireBytesRef>,
                            kProtectedDataSegmentsOffset)
ACCESSORS(WasmTrustedInstanceData, element_segments, Tagged<FixedArray>,
          kElementSegmentsOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, break_on_entry, uint8_t,
                    kBreakOnEntryOffset)

OPTIONAL_ACCESSORS(WasmTrustedInstanceData, instance_object,
                   Tagged<WasmInstanceObject>, kInstanceObjectOffset)
OPTIONAL_ACCESSORS(WasmTrustedInstanceData, native_context, Tagged<Context>,
                   kNativeContextOffset)
ACCESSORS(WasmTrustedInstanceData, memory_objects, Tagged<FixedArray>,
          kMemoryObjectsOffset)
ACCESSORS(WasmTrustedInstanceData, untagged_globals_buffer, Tagged<ByteArray>,
          kUntaggedGlobalsBufferOffset)
ACCESSORS(WasmTrustedInstanceData, tagged_globals_buffer, Tagged<FixedArray>,
          kTaggedGlobalsBufferOffset)
ACCESSORS(WasmTrustedInstanceData, tables, Tagged<FixedArray>, kTablesOffset)
#if V8_ENABLE_DRUMBRAKE
OPTIONAL_ACCESSORS(WasmTrustedInstanceData, interpreter_object, Tagged<Tuple2>,
                   kInterpreterObjectOffset)
#endif  // V8_ENABLE_DRUMBRAKE
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, shared_part,
                            WasmTrustedInstanceData, kProtectedSharedPartOffset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, dispatch_table0,
                            WasmDispatchTable, kProtectedDispatchTable0Offset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, dispatch_tables,
                            ProtectedFixedArray, kProtectedDispatchTablesOffset)
PROTECTED_POINTER_ACCESSORS(WasmTrustedInstanceData, dispatch_table_for_imports,
                            WasmDispatchTableForImports,
                            kProtectedDispatchTableForImportsOffset)
OPTIONAL_ACCESSORS(WasmTrustedInstanceData, tags_table, Tagged<FixedArray>,
                   kTagsTableOffset)
ACCESSORS(WasmTrustedInstanceData, func_refs, Tagged<FixedArray>,
          kFuncRefsOffset)
ACCESSORS(WasmTrustedInstanceData, managed_object_maps, Tagged<FixedArray>,
          kManagedObjectMapsOffset)
ACCESSORS(WasmTrustedInstanceData, feedback_vectors, Tagged<FixedArray>,
          kFeedbackVectorsOffset)
ACCESSORS(WasmTrustedInstanceData, well_known_imports, Tagged<FixedArray>,
          kWellKnownImportsOffset)

void WasmTrustedInstanceData::clear_padding() {
  constexpr int kPaddingBytes = FIELD_SIZE(kOptionalPaddingOffset);
  static_assert(kPaddingBytes == 0 || kPaddingBytes == kIntSize);
  if constexpr (kPaddingBytes != 0) {
    WriteField<int>(kOptionalPaddingOffset, 0);
  }
}

Tagged<WasmMemoryObject> WasmTrustedInstanceData::memory_object(
    int memory_index) const {
  return Cast<WasmMemoryObject>(memory_objects()->get(memory_index));
}

uint8_t* WasmTrustedInstanceData::memory_base(uint32_t memory_index) const {
  DCHECK_EQ(memory0_start(),
            reinterpret_cast<uint8_t*>(memory_bases_and_sizes()->get(0)));
  return reinterpret_cast<uint8_t*>(
      memory_bases_and_sizes()->get(2 * memory_index));
}

size_t WasmTrustedInstanceData::memory_size(uint32_t memory_index) const {
  DCHECK_EQ(memory0_size(), memory_bases_and_sizes()->get(1));
  return memory_bases_and_sizes()->get(2 * memory_index + 1);
}

wasm::NativeModule* WasmTrustedInstanceData::native_module() const {
  return managed_native_module()->raw();
}

Tagged<WasmModuleObject> WasmTrustedInstanceData::module_object() const {
  return instance_object()->module_object();
}

const wasm::WasmModule* WasmTrustedInstanceData::module() const {
  return native_module()->module();
}

Tagged<Union<Smi, WasmDispatchTable>> WasmTrustedInstanceData::dispatch_table(
    uint32_t i) const {
  if (i == 0) return dispatch_table0();
  return TrustedCast<Union<Smi, WasmDispatchTable>>(dispatch_tables()->get(i));
}

// WasmInstanceObject
TRUSTED_POINTER_ACCESSORS(WasmInstanceObject, trusted_data,
                          WasmTrustedInstanceData, kTrustedDataOffset,
                          kWasmTrustedInstanceDataIndirectPointerTag)

// Note: in case of existing in-sandbox corruption, this could return an
// incorrect WasmModule! For security-relevant code, prefer reading
// {native_module()} from a {WasmTrustedInstanceData}.
const wasm::WasmModule* WasmInstanceObject::module() const {
  return module_object()->native_module()->module();
}

ImportedFunctionEntry::ImportedFunctionEntry(
    DirectHandle<WasmTrustedInstanceData> instance_data, int index)
    : instance_data_(instance_data), index_(index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, instance_data->module()->num_imported_functions);
}

// WasmDispatchTable

PROTECTED_POINTER_ACCESSORS(WasmDispatchTable, protected_offheap_data,
                            TrustedManaged<WasmDispatchTableData>,
                            kProtectedOffheapDataOffset)
PROTECTED_POINTER_ACCESSORS(WasmDispatchTableForImports, protected_offheap_data,
                            TrustedManaged<WasmDispatchTableData>,
                            kProtectedOffheapDataOffset)

WasmDispatchTableData* WasmDispatchTable::offheap_data() const {
  return protected_offheap_data()->get().get();
}
WasmDispatchTableData* WasmDispatchTableForImports::offheap_data() const {
  return protected_offheap_data()->get().get();
}

PROTECTED_POINTER_ACCESSORS(WasmDispatchTable, protected_uses,
                            ProtectedWeakFixedArray, kProtectedUsesOffset)

wasm::CanonicalValueType WasmDispatchTable::table_type() const {
  return wasm::CanonicalValueType::FromRawBitField(
      ReadField<uint32_t>(kTableTypeOffset));
}
void WasmDispatchTable::set_table_type(wasm::CanonicalValueType type) {
  DCHECK(type.IsFunctionType());
  WriteField(kTableTypeOffset, type.raw_bit_field());
}

int WasmDispatchTable::length(AcquireLoadTag) const {
  return ACQUIRE_READ_INT32_FIELD(*this, kLengthOffset);
}
int WasmDispatchTableForImports::length(AcquireLoadTag) const {
  return ACQUIRE_READ_INT32_FIELD(*this, kLengthOffset);
}

int WasmDispatchTable::length() const { return ReadField<int>(kLengthOffset); }
int WasmDispatchTableForImports::length() const {
  return ReadField<int>(kLengthOffset);
}

int WasmDispatchTable::capacity() const {
  return ReadField<int>(kCapacityOffset);
}

inline Tagged<Object> WasmDispatchTable::implicit_arg(int index) const {
  DCHECK_LT(index, length());
  Tagged<Object> implicit_arg =
      ReadProtectedPointerField(OffsetOf(index) + kImplicitArgBias);
  DCHECK(IsWasmTrustedInstanceData(implicit_arg) ||
         IsWasmImportData(implicit_arg) || implicit_arg == Smi::zero());
  return implicit_arg;
}
inline Tagged<Object> WasmDispatchTableForImports::implicit_arg(
    int index) const {
  DCHECK_LT(index, length());
  Tagged<Object> implicit_arg =
      ReadProtectedPointerField(OffsetOf(index) + kImplicitArgBias);
  DCHECK(IsWasmTrustedInstanceData(implicit_arg) ||
         IsWasmImportData(implicit_arg) || implicit_arg == Smi::zero());
  return implicit_arg;
}

inline WasmCodePointer WasmDispatchTable::target(int index) const {
  DCHECK_LT(index, length());
  if (v8_flags.wasm_jitless) return wasm::kInvalidWasmCodePointer;
  static_assert(sizeof(WasmCodePointer) == sizeof(uint32_t));
  return WasmCodePointer{ReadField<uint32_t>(OffsetOf(index) + kTargetBias)};
}
inline WasmCodePointer WasmDispatchTableForImports::target(int index) const {
  DCHECK_LT(index, length());
  if (v8_flags.wasm_jitless) return wasm::kInvalidWasmCodePointer;
  static_assert(sizeof(WasmCodePointer) == sizeof(uint32_t));
  return WasmCodePointer{ReadField<uint32_t>(OffsetOf(index) + kTargetBias)};
}

inline wasm::CanonicalTypeIndex WasmDispatchTable::sig(int index) const {
  DCHECK_LT(index, length());
  return wasm::CanonicalTypeIndex{
      ReadField<uint32_t>(OffsetOf(index) + kSigBias)};
}

#if V8_ENABLE_DRUMBRAKE
inline uint32_t WasmDispatchTable::function_index(int index) const {
  DCHECK_LT(index, length());
  if (!v8_flags.wasm_jitless) return UINT_MAX;
  return ReadField<uint32_t>(OffsetOf(index) + kFunctionIndexBias);
}
#endif  // V8_ENABLE_DRUMBRAKE

// WasmExportedFunction

template <>
struct CastTraits<WasmExportedFunction> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return WasmExportedFunction::IsWasmExportedFunction(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return WasmExportedFunction::IsWasmExportedFunction(value);
  }
};

// WasmImportData

PROTECTED_POINTER_ACCESSORS(WasmImportData, instance_data,
                            WasmTrustedInstanceData,
                            kProtectedInstanceDataOffset)

PROTECTED_POINTER_ACCESSORS(WasmImportData, call_origin, TrustedObject,
                            kProtectedCallOriginOffset)

wasm::Suspend WasmImportData::suspend() const {
  return SuspendField::decode(bit_field());
}

void WasmImportData::set_suspend(wasm::Suspend value) {
  set_bit_field(SuspendField::update(bit_field(), value));
}

uint32_t WasmImportData::table_slot() const {
  return TableSlotField::decode(bit_field());
}

void WasmImportData::set_table_slot(uint32_t value) {
  set_bit_field(TableSlotField::update(bit_field(), value));
}

// WasmInternalFunction

// {implicit_arg} will be a WasmTrustedInstanceData or a WasmImportData.
PROTECTED_POINTER_ACCESSORS(WasmInternalFunction, implicit_arg, TrustedObject,
                            kProtectedImplicitArgOffset)

// WasmFuncRef
TRUSTED_POINTER_ACCESSORS(WasmFuncRef, internal, WasmInternalFunction,
                          kTrustedInternalOffset,
                          kWasmInternalFunctionIndirectPointerTag)

// WasmFunctionData
CODE_POINTER_ACCESSORS(WasmFunctionData, wrapper_code, kWrapperCodeOffset)

PROTECTED_POINTER_ACCESSORS(WasmFunctionData, internal, WasmInternalFunction,
                            kProtectedInternalOffset)

// WasmExportedFunctionData
PROTECTED_POINTER_ACCESSORS(WasmExportedFunctionData, instance_data,
                            WasmTrustedInstanceData,
                            kProtectedInstanceDataOffset)

CODE_POINTER_ACCESSORS(WasmExportedFunctionData, c_wrapper_code,
                       kCWrapperCodeOffset)

bool WasmExportedFunctionData::is_promising() const {
  return WasmFunctionData::PromiseField::decode(js_promise_flags()) ==
         wasm::kPromise;
}

WasmCodePointer WasmInternalFunction::call_target() {
  return WasmCodePointer{raw_call_target()};
}
void WasmInternalFunction::set_call_target(WasmCodePointer code_pointer) {
  set_raw_call_target(code_pointer.value());
}

// WasmJSFunctionData
PROTECTED_POINTER_ACCESSORS(WasmJSFunctionData, protected_offheap_data,
                            TrustedManaged<WasmJSFunctionData::OffheapData>,
                            kProtectedOffheapDataOffset)

WasmJSFunctionData::OffheapData* WasmJSFunctionData::offheap_data() const {
  return protected_offheap_data()->get().get();
}

// WasmJSFunction

template <>
struct CastTraits<WasmJSFunction> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return WasmJSFunction::IsWasmJSFunction(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return WasmJSFunction::IsWasmJSFunction(value);
  }
};

// WasmCapiFunction

template <>
struct CastTraits<WasmCapiFunction> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return WasmCapiFunction::IsWasmCapiFunction(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return WasmCapiFunction::IsWasmCapiFunction(value);
  }
};

// WasmExternalFunction

template <>
struct CastTraits<WasmExternalFunction> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return WasmExternalFunction::IsWasmExternalFunction(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return WasmExternalFunction::IsWasmExternalFunction(value);
  }
};

Tagged<WasmFuncRef> WasmExternalFunction::func_ref() const {
  return shared()->wasm_function_data()->func_ref();
}

// WasmTypeInfo
wasm::CanonicalValueType WasmTypeInfo::type() const {
  return wasm::CanonicalValueType::FromRawBitField(canonical_type());
}

wasm::CanonicalTypeIndex WasmTypeInfo::type_index() const {
  return type().ref_index();
}

wasm::CanonicalValueType WasmTypeInfo::element_type() const {
  return wasm::CanonicalValueType::FromRawBitField(canonical_element_type());
}

#undef OPTIONAL_ACCESSORS
#undef READ_PRIMITIVE_FIELD
#undef WRITE_PRIMITIVE_FIELD
#undef PRIMITIVE_ACCESSORS

TRUSTED_POINTER_ACCESSORS(WasmTableObject, trusted_data,
                          WasmTrustedInstanceData, kTrustedDataOffset,
                          kWasmTrustedInstanceDataIndirectPointerTag)

TRUSTED_POINTER_ACCESSORS(WasmTableObject, trusted_dispatch_table,
                          WasmDispatchTable, kTrustedDispatchTableOffset,
                          kWasmDispatchTableIndirectPointerTag)

TRUSTED_POINTER_ACCESSORS(WasmResumeData, trusted_suspender,
                          WasmSuspenderObject, kTrustedSuspenderOffset,
                          kWasmSuspenderIndirectPointerTag)

PROTECTED_POINTER_ACCESSORS(WasmSuspenderObject, parent, WasmSuspenderObject,
                            kParentOffset)

wasm::ValueType WasmTableObject::type(const wasm::WasmModule* module) {
  wasm::ValueType type = unsafe_type();
  SBXCHECK(!type.has_index() || module->has_type(type.ref_index()));
  return type;
}

wasm::CanonicalValueType WasmTableObject::canonical_type(
    const wasm::WasmModule* module) {
  wasm::ValueType unsafe = unsafe_type();
  if (!unsafe.has_index()) return wasm::CanonicalValueType{unsafe};
  SBXCHECK(module != nullptr && module->has_type(unsafe.ref_index()));
  return module->canonical_type(unsafe);
}

wasm::ValueType WasmTableObject::unsafe_type() {
  // Various consumers of ValueKind (e.g. ValueKind::name()) use the raw enum
  // value as index into a global array. As such, if the index is corrupted
  // (which must be assumed, as it comes from within the sandbox), this can
  // lead to out-of-bounds reads outside the sandbox. While these are not
  // technically sandbox violations, we should still try to avoid them to keep
  // fuzzers happy. This SBXCHECK accomplishes that.
  wasm::ValueType type = wasm::ValueType::FromRawBitField(raw_type());
  SBXCHECK(type.is_valid());
  return type;
}

bool WasmTableObject::is_in_bounds(uint32_t entry_index) {
  return entry_index < static_cast<uint32_t>(current_length());
}

bool WasmTableObject::is_table64() const {
  return address_type() == wasm::AddressType::kI64;
}

std::optional<uint64_t> WasmTableObject::maximum_length_u64() const {
  Tagged<Object> max = maximum_length();
  if (IsUndefined(max)) return std::nullopt;
  if (is_table64()) {
    DCHECK(IsBigInt(max));
#if DEBUG
    bool lossless;
    uint64_t value = Cast<BigInt>(maximum_length())->AsUint64(&lossless);
    DCHECK(lossless);
    return value;
#else
    return Cast<BigInt>(maximum_length())->AsUint64();
#endif
  }
  DCHECK(IsNumber(max));
  double value = Object::NumberValue(max);
  DCHECK_LE(0, value);
  DCHECK_GE(std::numeric_limits<uint64_t>::max(), value);
  return value;
}

bool WasmMemoryObject::has_maximum_pages() { return maximum_pages() >= 0; }

bool WasmMemoryObject::is_memory64() const {
  return address_type() == wasm::AddressType::kI64;
}

// static
DirectHandle<Object> WasmObject::ReadValueAt(Isolate* isolate,
                                             DirectHandle<HeapObject> obj,
                                             wasm::CanonicalValueType type,
                                             uint32_t offset) {
  Address field_address = obj->GetFieldAddress(offset);
  switch (type.kind()) {
    case wasm::kI8: {
      int8_t value = base::Memory<int8_t>(field_address);
      return direct_handle(Smi::FromInt(value), isolate);
    }
    case wasm::kI16: {
      int16_t value = base::Memory<int16_t>(field_address);
      return direct_handle(Smi::FromInt(value), isolate);
    }
    case wasm::kI32:
    case wasm::kWaitQueue: {
      int32_t value = base::Memory<int32_t>(field_address);
      return isolate->factory()->NewNumberFromInt(value);
    }
    case wasm::kI64: {
      int64_t value = base::ReadUnalignedValue<int64_t>(field_address);
      return BigInt::FromInt64(isolate, value);
    }
    case wasm::kF16: {
      uint16_t value = base::Memory<uint16_t>(field_address);
      return isolate->factory()->NewNumber(fp16_ieee_to_fp32_value(value));
    }
    case wasm::kF32: {
      float value = base::Memory<float>(field_address);
      return isolate->factory()->NewNumber(value);
    }
    case wasm::kF64: {
      double value = base::ReadUnalignedValue<double>(field_address);
      return isolate->factory()->NewNumber(value);
    }
    case wasm::kS128:
      // TODO(v8:11804): implement
      UNREACHABLE();

    case wasm::kRef:
    case wasm::kRefNull: {
      ObjectSlot slot(field_address);
      return direct_handle(slot.load(isolate), isolate);
    }

    case wasm::kVoid:
    case wasm::kTop:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

// static
void WasmStruct::EncodeInstanceSizeInMap(int instance_size, Tagged<Map> map) {
  // WasmStructs can be bigger than the {map.instance_size_in_words} field
  // can describe; yet we have to store the instance size somewhere on the
  // map so that the GC can read it without relying on any other objects
  // still being around. To solve this problem, we store the instance size
  // in two other fields that are otherwise unused for WasmStructs.
  static_assert(0xFFFF > ((kHeaderSize + wasm::kMaxValueTypeSize *
                                             wasm::kV8MaxWasmStructFields) >>
                          kObjectAlignmentBits));
  map->SetWasmByte1((instance_size >> kObjectAlignmentBits) & 0xff);
  map->SetWasmByte2(instance_size >> (8 + kObjectAlignmentBits));
}

// static
int WasmStruct::DecodeInstanceSizeFromMap(Tagged<Map> map) {
  return (map->WasmByte2() << (8 + kObjectAlignmentBits)) |
         (map->WasmByte1() << kObjectAlignmentBits);
}

int WasmStruct::GcSafeSize(Tagged<Map> map) {
  return DecodeInstanceSizeFromMap(map);
}

Address WasmStruct::RawFieldAddress(int raw_offset) {
  int offset = WasmStruct::kHeaderSize + raw_offset;
  return FIELD_ADDR(*this, offset);
}

ObjectSlot WasmStruct::RawField(int raw_offset) {
  return ObjectSlot(RawFieldAddress(raw_offset));
}

void WasmStruct::SetTaggedFieldValue(int raw_offset, Tagged<Object> value,
                                     WriteBarrierMode mode) {
  int offset = WasmStruct::kHeaderSize + raw_offset;
  TaggedField<Object>::store(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

ACCESSORS_CHECKED(WasmStruct, described_rtt, Tagged<Map>, kHeaderSize,
                  GcSafeType(map())->is_descriptor())

const wasm::CanonicalValueType WasmArray::GcSafeElementType(Tagged<Map> map) {
  DCHECK_EQ(WASM_ARRAY_TYPE, map->instance_type());
  Tagged<HeapObject> raw = Cast<HeapObject>(map->constructor_or_back_pointer());
  // The {WasmTypeInfo} might be in the middle of being moved, which is why we
  // can't read its map for a checked cast. But we can rely on its native type
  // pointer being intact in the old location.
  Tagged<WasmTypeInfo> type_info = UncheckedCast<WasmTypeInfo>(raw);
  return type_info->element_type();
}

int WasmArray::SizeFor(Tagged<Map> map, int length) {
  int element_size = DecodeElementSizeFromMap(map);
  return SizeFor(element_size, length);
}

constexpr int WasmArray::SizeFor(int element_size, int length) {
  return kHeaderSize + RoundUp(element_size * length, kTaggedSize);
}

// Allocating arrays currently requires passing the requested byte size to the
// runtime function as a Smi.
static_assert(Smi::IsValid(WasmArray::SizeFor(1, WasmArray::MaxLength(1))));
static_assert(Smi::IsValid(WasmArray::SizeFor(2, WasmArray::MaxLength(2))));
static_assert(Smi::IsValid(WasmArray::SizeFor(4, WasmArray::MaxLength(4))));
static_assert(Smi::IsValid(WasmArray::SizeFor(8, WasmArray::MaxLength(8))));
static_assert(Smi::IsValid(WasmArray::SizeFor(16, WasmArray::MaxLength(16))));

uint32_t WasmArray::element_offset(uint32_t index) {
  DCHECK_LE(index, length());
  int element_size = DecodeElementSizeFromMap(map());
  return WasmArray::kHeaderSize + index * element_size;
}

Address WasmArray::ElementAddress(uint32_t index) {
  return ptr() + element_offset(index) - kHeapObjectTag;
}

ObjectSlot WasmArray::ElementSlot(uint32_t index) {
  DCHECK_LE(index, length());
  DCHECK(map()->wasm_type_info()->element_type().is_ref());
  return RawField(kHeaderSize + kTaggedSize * index);
}

// static
DirectHandle<Object> WasmArray::GetElement(Isolate* isolate,
                                           DirectHandle<WasmArray> array,
                                           uint32_t index) {
  if (index >= array->length()) {
    return isolate->factory()->undefined_value();
  }
  wasm::CanonicalValueType element_type =
      array->map()->wasm_type_info()->element_type();
  return ReadValueAt(isolate, array, element_type,
                     array->element_offset(index));
}

// static
void WasmArray::EncodeElementSizeInMap(int element_size, Tagged<Map> map) {
  map->SetWasmByte1(element_size);
}

// static
int WasmArray::DecodeElementSizeFromMap(Tagged<Map> map) {
  return map->WasmByte1();
}

EXTERNAL_POINTER_ACCESSORS(WasmSuspenderObject, stack, wasm::StackMemory*,
                           kStackOffset, kWasmStackMemoryTag)

EXTERNAL_POINTER_ACCESSORS(WasmStackObject, stack, wasm::StackMemory*,
                           kStackOffset, kWasmStackMemoryTag)

TRUSTED_POINTER_ACCESSORS(WasmTagObject, trusted_data, WasmTrustedInstanceData,
                          kTrustedDataOffset,
                          kWasmTrustedInstanceDataIndirectPointerTag)

#include "src/objects/object-macros-undef.h"

}  // namespace v8::internal

#endif  // V8_WASM_WASM_OBJECTS_INL_H_
