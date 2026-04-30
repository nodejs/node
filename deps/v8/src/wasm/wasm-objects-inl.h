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
#include "src/objects/casting.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/managed.h"
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
Tagged<Managed<wasm::NativeModule>> WasmModuleObject::managed_native_module()
    const {
  return managed_native_module_.load();
}
void WasmModuleObject::set_managed_native_module(
    Tagged<Managed<wasm::NativeModule>> value, WriteBarrierMode mode) {
  managed_native_module_.store(this, value, mode);
}

Tagged<Script> WasmModuleObject::script() const { return script_.load(); }
void WasmModuleObject::set_script(Tagged<Script> value, WriteBarrierMode mode) {
  script_.store(this, value, mode);
}

Managed<wasm::NativeModule>::Ptr WasmModuleObject::native_module() {
  return managed_native_module()->ptr();
}

// WasmMemoryMapDescriptor
Tagged<Weak<HeapObject>> WasmMemoryMapDescriptor::memory() const {
  return memory_.load();
}
void WasmMemoryMapDescriptor::set_memory(Tagged<Weak<HeapObject>> value,
                                         WriteBarrierMode mode) {
  memory_.store(this, value, mode);
}

int32_t WasmMemoryMapDescriptor::file_descriptor() const {
  return file_descriptor_;
}
void WasmMemoryMapDescriptor::set_file_descriptor(int32_t value) {
  file_descriptor_ = value;
}

uint32_t WasmMemoryMapDescriptor::offset() const { return offset_; }
void WasmMemoryMapDescriptor::set_offset(uint32_t value) { offset_ = value; }

uint32_t WasmMemoryMapDescriptor::size() const { return size_; }
void WasmMemoryMapDescriptor::set_size(uint32_t value) { size_ = value; }

// WasmMemoryObject
Tagged<UnionOf<JSArrayBuffer, Undefined>> WasmMemoryObject::array_buffer()
    const {
  return array_buffer_.load();
}
void WasmMemoryObject::set_array_buffer(
    Tagged<UnionOf<JSArrayBuffer, Undefined>> value, WriteBarrierMode mode) {
  array_buffer_.store(this, value, mode);
}

Tagged<Managed<BackingStore>> WasmMemoryObject::managed_backing_store() const {
  return managed_backing_store_.load();
}
void WasmMemoryObject::set_managed_backing_store(
    Tagged<Managed<BackingStore>> value, WriteBarrierMode mode) {
  managed_backing_store_.store(this, value, mode);
}

int WasmMemoryObject::maximum_pages() const {
  return maximum_pages_.load().value();
}
void WasmMemoryObject::set_maximum_pages(int value) {
  maximum_pages_.store(this, Smi::FromInt(value));
}

Tagged<WeakArrayList> WasmMemoryObject::instances() const {
  return instances_.load();
}
void WasmMemoryObject::set_instances(Tagged<WeakArrayList> value,
                                     WriteBarrierMode mode) {
  instances_.store(this, value, mode);
}

wasm::AddressType WasmMemoryObject::address_type() const {
  return static_cast<wasm::AddressType>(address_type_);
}
void WasmMemoryObject::set_address_type(wasm::AddressType value) {
  address_type_ = static_cast<uint8_t>(value);
}

Managed<BackingStore>::Ptr WasmMemoryObject::backing_store() const {
  return managed_backing_store()->ptr();
}

// WasmGlobalObject
Tagged<WasmGlobalObject::BufferType> WasmGlobalObject::buffer() const {
  return buffer_.load();
}
void WasmGlobalObject::set_buffer(Tagged<WasmGlobalObject::BufferType> value,
                                  WriteBarrierMode mode) {
  buffer_.store(this, value, mode);
}

int WasmGlobalObject::raw_type() const { return raw_type_.load().value(); }
void WasmGlobalObject::set_raw_type(int value) {
  raw_type_.store(this, Smi::FromInt(value));
}

int WasmGlobalObject::offset() const { return offset_.load().value(); }
void WasmGlobalObject::set_offset(int value) {
  offset_.store(this, Smi::FromInt(value));
}

int WasmGlobalObject::is_mutable() const { return is_mutable_.load().value(); }
void WasmGlobalObject::set_is_mutable(int value) {
  is_mutable_.store(this, Smi::FromInt(value));
}

Tagged<WasmTrustedInstanceData> WasmGlobalObject::trusted_data(
    IsolateForSandbox isolate) const {
  return trusted_data_.load(isolate);
}
Tagged<WasmTrustedInstanceData> WasmGlobalObject::trusted_data(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_data_.Acquire_Load(isolate);
}
void WasmGlobalObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                        WriteBarrierMode mode) {
  trusted_data_.store(this, value, mode);
}
void WasmGlobalObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                        ReleaseStoreTag,
                                        WriteBarrierMode mode) {
  trusted_data_.Release_Store(this, value, mode);
}
bool WasmGlobalObject::has_trusted_data() const {
  return !trusted_data_.is_empty();
}
bool WasmGlobalObject::has_trusted_data_unpublished(
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), kTrustedDataOffset,
      kWasmTrustedInstanceDataIndirectPointerTag, isolate);
}
void WasmGlobalObject::clear_trusted_data() { trusted_data_.clear(this); }

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
// ACCESSORS/OPTIONAL_ACCESSORS/PROTECTED_POINTER_ACCESSORS all use
// CONDITIONAL_*_WRITE_BARRIER(*this, ...) which expands to (object)->... —
// arrow access that fails for HeapObjectLayout value types.  Spell out the
// impls manually using *Tagged<WasmTrustedInstanceData>(this) instead.

// Tagged field accessors (replaces ACCESSORS).
#define WTI_TAGGED_ACCESSORS(name, type, offset)                             \
  type WasmTrustedInstanceData::name() const {                               \
    PtrComprCageBase cage_base = GetPtrComprCageBase(*this);                 \
    return name(cage_base);                                                  \
  }                                                                          \
  type WasmTrustedInstanceData::name(PtrComprCageBase cage_base) const {     \
    return TaggedField<type, offset>::load(cage_base, *this);                \
  }                                                                          \
  void WasmTrustedInstanceData::set_##name(type value,                       \
                                           WriteBarrierMode mode) {          \
    TaggedField<type, offset>::store(*this, value);                          \
    CONDITIONAL_WRITE_BARRIER(Tagged<WasmTrustedInstanceData>(this), offset, \
                              value, mode);                                  \
  }

// Optional tagged field accessors (replaces OPTIONAL_ACCESSORS).
#define WTI_OPTIONAL_TAGGED_ACCESSORS(name, type, offset)                      \
  bool WasmTrustedInstanceData::has_##name() const {                           \
    PtrComprCageBase cage_base = GetPtrComprCageBase(*this);                   \
    return has_##name(cage_base);                                              \
  }                                                                            \
  bool WasmTrustedInstanceData::has_##name(PtrComprCageBase cage_base) const { \
    Tagged<Object> value =                                                     \
        TaggedField<Object, offset>::load(cage_base, *this);                   \
    return !IsUndefined(value);                                                \
  }                                                                            \
  type WasmTrustedInstanceData::name() const {                                 \
    PtrComprCageBase cage_base = GetPtrComprCageBase(*this);                   \
    return name(cage_base);                                                    \
  }                                                                            \
  type WasmTrustedInstanceData::name(PtrComprCageBase cage_base) const {       \
    DCHECK(has_##name(cage_base));                                             \
    return TaggedField<type, offset>::load(cage_base, *this);                  \
  }                                                                            \
  void WasmTrustedInstanceData::set_##name(type value,                         \
                                           WriteBarrierMode mode) {            \
    TaggedField<type, offset>::store(*this, value);                            \
    CONDITIONAL_WRITE_BARRIER(Tagged<WasmTrustedInstanceData>(this), offset,   \
                              value, mode);                                    \
  }

// Protected pointer accessors (replaces PROTECTED_POINTER_ACCESSORS).
// PROTECTED_POINTER_ACCESSORS requires a TrustedObject base; spell out
// the impls manually now that WasmTrustedInstanceData derives from
// ExposedTrustedObject.
#define WTI_PROTECTED_POINTER_ACCESSORS(name, type, offset)           \
  Tagged<type> WasmTrustedInstanceData::name() const {                \
    DCHECK(has_##name());                                             \
    return ReadProtectedPointerField<type>(offset);                   \
  }                                                                   \
  void WasmTrustedInstanceData::set_##name(Tagged<type> value,        \
                                           WriteBarrierMode mode) {   \
    WriteProtectedPointerField(offset, value);                        \
    CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(                      \
        *Tagged<WasmTrustedInstanceData>(this), offset, value, mode); \
  }                                                                   \
  bool WasmTrustedInstanceData::has_##name() const {                  \
    return !IsProtectedPointerFieldEmpty(offset);                     \
  }                                                                   \
  void WasmTrustedInstanceData::clear_##name() {                      \
    ClearProtectedPointerField(offset);                               \
  }

WTI_PROTECTED_POINTER_ACCESSORS(managed_native_module,
                                TrustedManaged<wasm::NativeModule>,
                                kProtectedManagedNativeModuleOffset)
WTI_TAGGED_ACCESSORS(imported_mutable_globals_buffers, Tagged<FixedArray>,
                     kImportedMutableGlobalsBuffersOffset)
WTI_TAGGED_ACCESSORS(imported_mutable_globals_offsets, Tagged<FixedUInt32Array>,
                     kImportedMutableGlobalsOffsetsOffset)
#if V8_ENABLE_DRUMBRAKE
WTI_TAGGED_ACCESSORS(imported_function_indices, Tagged<FixedInt32Array>,
                     kImportedFunctionIndicesOffset)
#endif  // V8_ENABLE_DRUMBRAKE
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, jump_table_start, Address,
                    kJumpTableStartOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, hook_on_function_call_address,
                    Address, kHookOnFunctionCallAddressOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, tiering_budget_array,
                    std::atomic<uint32_t>*, kTieringBudgetArrayOffset)
WTI_PROTECTED_POINTER_ACCESSORS(memory_bases_and_sizes,
                                TrustedFixedAddressArray,
                                kProtectedMemoryBasesAndSizesOffset)
WTI_PROTECTED_POINTER_ACCESSORS(data_segments,
                                TrustedPodArray<wasm::WireBytesRef>,
                                kProtectedDataSegmentsOffset)
WTI_TAGGED_ACCESSORS(element_segments, Tagged<FixedArray>,
                     kElementSegmentsOffset)
PRIMITIVE_ACCESSORS(WasmTrustedInstanceData, break_on_entry, uint8_t,
                    kBreakOnEntryOffset)

WTI_OPTIONAL_TAGGED_ACCESSORS(instance_object, Tagged<WasmInstanceObject>,
                              kInstanceObjectOffset)
WTI_OPTIONAL_TAGGED_ACCESSORS(native_context, Tagged<Context>,
                              kNativeContextOffset)
WTI_TAGGED_ACCESSORS(memory_objects, Tagged<FixedArray>, kMemoryObjectsOffset)
WTI_TAGGED_ACCESSORS(untagged_globals_buffer, Tagged<ByteArray>,
                     kUntaggedGlobalsBufferOffset)
WTI_TAGGED_ACCESSORS(tagged_globals_buffer, Tagged<FixedArray>,
                     kTaggedGlobalsBufferOffset)
WTI_TAGGED_ACCESSORS(tables, Tagged<FixedArray>, kTablesOffset)
#if V8_ENABLE_DRUMBRAKE
WTI_OPTIONAL_TAGGED_ACCESSORS(interpreter_object, Tagged<Tuple2>,
                              kInterpreterObjectOffset)
#endif  // V8_ENABLE_DRUMBRAKE
WTI_PROTECTED_POINTER_ACCESSORS(shared_part, WasmTrustedInstanceData,
                                kProtectedSharedPartOffset)
WTI_PROTECTED_POINTER_ACCESSORS(dispatch_table0, WasmDispatchTable,
                                kProtectedDispatchTable0Offset)
WTI_PROTECTED_POINTER_ACCESSORS(dispatch_tables, ProtectedFixedArray,
                                kProtectedDispatchTablesOffset)
WTI_PROTECTED_POINTER_ACCESSORS(dispatch_table_for_imports,
                                WasmDispatchTableForImports,
                                kProtectedDispatchTableForImportsOffset)
#undef WTI_PROTECTED_POINTER_ACCESSORS
WTI_OPTIONAL_TAGGED_ACCESSORS(tags_table, Tagged<FixedArray>, kTagsTableOffset)
WTI_TAGGED_ACCESSORS(func_refs, Tagged<FixedArray>, kFuncRefsOffset)
WTI_TAGGED_ACCESSORS(managed_object_maps, Tagged<FixedArray>,
                     kManagedObjectMapsOffset)
WTI_TAGGED_ACCESSORS(feedback_vectors, Tagged<FixedArray>,
                     kFeedbackVectorsOffset)
WTI_TAGGED_ACCESSORS(well_known_imports, Tagged<FixedArray>,
                     kWellKnownImportsOffset)
#undef WTI_TAGGED_ACCESSORS
#undef WTI_OPTIONAL_TAGGED_ACCESSORS

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

Tagged<WasmDispatchTable> WasmTrustedInstanceData::dispatch_table(
    uint32_t i) const {
  if (i == 0) return dispatch_table0();
  return TrustedCast<WasmDispatchTable>(dispatch_tables()->get(i));
}

// WasmInstanceObject
Tagged<WasmTrustedInstanceData> WasmInstanceObject::trusted_data(
    IsolateForSandbox isolate) const {
  return trusted_data_.load(isolate);
}
Tagged<WasmTrustedInstanceData> WasmInstanceObject::trusted_data(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_data_.Acquire_Load(isolate);
}
void WasmInstanceObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                          WriteBarrierMode mode) {
  trusted_data_.store(this, value, mode);
}
void WasmInstanceObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                          ReleaseStoreTag,
                                          WriteBarrierMode mode) {
  trusted_data_.Release_Store(this, value, mode);
}
bool WasmInstanceObject::has_trusted_data() const {
  return !trusted_data_.is_empty();
}
bool WasmInstanceObject::has_trusted_data_unpublished(
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), kTrustedDataOffset,
      kWasmTrustedInstanceDataIndirectPointerTag, isolate);
}
void WasmInstanceObject::clear_trusted_data() { trusted_data_.clear(this); }

Tagged<WasmModuleObject> WasmInstanceObject::module_object() const {
  return module_object_.load();
}
void WasmInstanceObject::set_module_object(Tagged<WasmModuleObject> value,
                                           WriteBarrierMode mode) {
  module_object_.store(this, value, mode);
}

Tagged<JSObject> WasmInstanceObject::exports_object() const {
  return exports_object_.load();
}
void WasmInstanceObject::set_exports_object(Tagged<JSObject> value,
                                            WriteBarrierMode mode) {
  exports_object_.store(this, value, mode);
}

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

// TODO(jgruber): Spelled out (not PROTECTED_POINTER_ACCESSORS) because the
// macro expands to CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(*this, ...),
// which is not valid on a V8_OBJECT / HeapObjectLayout holder. The macro
// needs a Tagged<T>-wrapped self; spell the accessors out until the barrier
// macro is updated.
Tagged<TrustedManaged<WasmDispatchTableData>>
WasmDispatchTable::protected_offheap_data() const {
  DCHECK(has_protected_offheap_data());
  return ReadProtectedPointerField<TrustedManaged<WasmDispatchTableData>>(
      kProtectedOffheapDataOffset);
}
void WasmDispatchTable::set_protected_offheap_data(
    Tagged<TrustedManaged<WasmDispatchTableData>> value,
    WriteBarrierMode mode) {
  WriteProtectedPointerField(kProtectedOffheapDataOffset, value);
  CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(*Tagged<WasmDispatchTable>(this),
                                              kProtectedOffheapDataOffset,
                                              value, mode);
}
bool WasmDispatchTable::has_protected_offheap_data() const {
  return !IsProtectedPointerFieldEmpty(kProtectedOffheapDataOffset);
}
void WasmDispatchTable::clear_protected_offheap_data() {
  ClearProtectedPointerField(kProtectedOffheapDataOffset);
}
// TODO(jgruber): See the WasmDispatchTable block above:
// PROTECTED_POINTER_ACCESSORS expands to
// CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(*this, ...) which does not
// work on a V8_OBJECT holder; spell the accessors out until the barrier
// macro is updated.
Tagged<TrustedManaged<WasmDispatchTableData>>
WasmDispatchTableForImports::protected_offheap_data() const {
  DCHECK(has_protected_offheap_data());
  return ReadProtectedPointerField<TrustedManaged<WasmDispatchTableData>>(
      kProtectedOffheapDataOffset);
}
void WasmDispatchTableForImports::set_protected_offheap_data(
    Tagged<TrustedManaged<WasmDispatchTableData>> value,
    WriteBarrierMode mode) {
  WriteProtectedPointerField(kProtectedOffheapDataOffset, value);
  CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(
      *Tagged<WasmDispatchTableForImports>(this), kProtectedOffheapDataOffset,
      value, mode);
}
bool WasmDispatchTableForImports::has_protected_offheap_data() const {
  return !IsProtectedPointerFieldEmpty(kProtectedOffheapDataOffset);
}
void WasmDispatchTableForImports::clear_protected_offheap_data() {
  ClearProtectedPointerField(kProtectedOffheapDataOffset);
}

WasmDispatchTableData* WasmDispatchTable::offheap_data() const {
  if (!has_protected_offheap_data()) return nullptr;
  return protected_offheap_data()->get().get();
}
WasmDispatchTableData* WasmDispatchTableForImports::offheap_data() const {
  if (!has_protected_offheap_data()) return nullptr;
  return protected_offheap_data()->get().get();
}

Tagged<ProtectedWeakFixedArray> WasmDispatchTable::protected_uses() const {
  DCHECK(has_protected_uses());
  return ReadProtectedPointerField<ProtectedWeakFixedArray>(
      kProtectedUsesOffset);
}
void WasmDispatchTable::set_protected_uses(
    Tagged<ProtectedWeakFixedArray> value, WriteBarrierMode mode) {
  WriteProtectedPointerField(kProtectedUsesOffset, value);
  CONDITIONAL_PROTECTED_POINTER_WRITE_BARRIER(
      *Tagged<WasmDispatchTable>(this), kProtectedUsesOffset, value, mode);
}
bool WasmDispatchTable::has_protected_uses() const {
  return !IsProtectedPointerFieldEmpty(kProtectedUsesOffset);
}
void WasmDispatchTable::clear_protected_uses() {
  ClearProtectedPointerField(kProtectedUsesOffset);
}

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

Tagged<WasmTrustedInstanceData> WasmImportData::instance_data() const {
  DCHECK(has_instance_data());
  return protected_instance_data_.load();
}
void WasmImportData::set_instance_data(Tagged<WasmTrustedInstanceData> value,
                                       WriteBarrierMode mode) {
  protected_instance_data_.store(this, value, mode);
}
bool WasmImportData::has_instance_data() const {
  return !protected_instance_data_.load().is_null();
}
void WasmImportData::clear_instance_data() {
  protected_instance_data_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<TrustedObject> WasmImportData::call_origin() const {
  DCHECK(has_call_origin());
  return protected_call_origin_.load();
}
void WasmImportData::set_call_origin(Tagged<TrustedObject> value,
                                     WriteBarrierMode mode) {
  protected_call_origin_.store(this, value, mode);
}
bool WasmImportData::has_call_origin() const {
  return !protected_call_origin_.load().is_null();
}
void WasmImportData::clear_call_origin() {
  protected_call_origin_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<NativeContext> WasmImportData::native_context() const {
  return native_context_.load();
}
void WasmImportData::set_native_context(Tagged<NativeContext> value,
                                        WriteBarrierMode mode) {
  native_context_.store(this, value, mode);
}

Tagged<UnionOf<JSReceiver, Undefined>> WasmImportData::callable() const {
  return callable_.load();
}
void WasmImportData::set_callable(Tagged<UnionOf<JSReceiver, Undefined>> value,
                                  WriteBarrierMode mode) {
  callable_.store(this, value, mode);
}

Tagged<Cell> WasmImportData::wrapper_budget() const {
  return wrapper_budget_.load();
}
void WasmImportData::set_wrapper_budget(Tagged<Cell> value,
                                        WriteBarrierMode mode) {
  wrapper_budget_.store(this, value, mode);
}

const wasm::CanonicalSig* WasmImportData::sig() const {
  return reinterpret_cast<const wasm::CanonicalSig*>(sig_.value());
}
void WasmImportData::set_sig(const wasm::CanonicalSig* value) {
  sig_.set_value(reinterpret_cast<Address>(value));
}

uint32_t WasmImportData::bit_field() const { return bit_field_; }
void WasmImportData::set_bit_field(uint32_t value) { bit_field_ = value; }

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

void WasmImportData::clear_padding() {
#if TAGGED_SIZE_8_BYTES
  optional_padding_ = 0;
#endif
}

// WasmInternalFunction

// {implicit_arg} will be a WasmTrustedInstanceData or a WasmImportData.
Tagged<TrustedObject> WasmInternalFunction::implicit_arg() const {
  DCHECK(has_implicit_arg());
  return protected_implicit_arg_.load();
}
void WasmInternalFunction::set_implicit_arg(Tagged<TrustedObject> value,
                                            WriteBarrierMode mode) {
  protected_implicit_arg_.store(this, value, mode);
}
bool WasmInternalFunction::has_implicit_arg() const {
  return !protected_implicit_arg_.load().is_null();
}
void WasmInternalFunction::clear_implicit_arg() {
  protected_implicit_arg_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<UnionOf<JSFunction, Undefined>> WasmInternalFunction::external() const {
  return external_.load();
}
void WasmInternalFunction::set_external(
    Tagged<UnionOf<JSFunction, Undefined>> value, WriteBarrierMode mode) {
  external_.store(this, value, mode);
}

int WasmInternalFunction::function_index() const {
  return function_index_.load().value();
}
void WasmInternalFunction::set_function_index(int value) {
  function_index_.store_no_write_barrier(Smi::FromInt(value));
}

uint32_t WasmInternalFunction::raw_call_target() const {
  return raw_call_target_;
}
void WasmInternalFunction::set_raw_call_target(uint32_t value) {
  raw_call_target_ = value;
}

const wasm::CanonicalSig* WasmInternalFunction::sig() const {
  return reinterpret_cast<const wasm::CanonicalSig*>(sig_.value());
}
void WasmInternalFunction::set_sig(const wasm::CanonicalSig* value) {
  sig_.set_value(reinterpret_cast<Address>(value));
}

// WasmFuncRef
Tagged<WasmInternalFunction> WasmFuncRef::internal(
    IsolateForSandbox isolate) const {
  return trusted_internal_.load(isolate);
}
Tagged<WasmInternalFunction> WasmFuncRef::internal(IsolateForSandbox isolate,
                                                   AcquireLoadTag) const {
  return trusted_internal_.Acquire_Load(isolate);
}
void WasmFuncRef::set_internal(Tagged<WasmInternalFunction> value,
                               WriteBarrierMode mode) {
  trusted_internal_.store(this, value, mode);
}
void WasmFuncRef::set_internal(Tagged<WasmInternalFunction> value,
                               ReleaseStoreTag, WriteBarrierMode mode) {
  trusted_internal_.Release_Store(this, value, mode);
}
bool WasmFuncRef::has_internal() const { return !trusted_internal_.is_empty(); }
void WasmFuncRef::clear_internal() { trusted_internal_.clear(this); }

// WasmFunctionData
Tagged<Code> WasmFunctionData::wrapper_code(IsolateForSandbox isolate) const {
  return wrapper_code_.load(isolate);
}
void WasmFunctionData::set_wrapper_code(Tagged<Code> value,
                                        WriteBarrierMode mode) {
  wrapper_code_.store(this, value, mode);
}

Tagged<WasmInternalFunction> WasmFunctionData::internal() const {
  DCHECK(has_internal());
  return protected_internal_.load();
}
void WasmFunctionData::set_internal(Tagged<WasmInternalFunction> value,
                                    WriteBarrierMode mode) {
  protected_internal_.store(this, value, mode);
}
bool WasmFunctionData::has_internal() const {
  return !protected_internal_.load().is_null();
}
void WasmFunctionData::clear_internal() {
  protected_internal_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<WasmFuncRef> WasmFunctionData::func_ref() const {
  return func_ref_.load();
}
void WasmFunctionData::set_func_ref(Tagged<WasmFuncRef> value,
                                    WriteBarrierMode mode) {
  func_ref_.store(this, value, mode);
}

int WasmFunctionData::js_promise_flags() const {
  return js_promise_flags_.load().value();
}
void WasmFunctionData::set_js_promise_flags(int value) {
  js_promise_flags_.store_no_write_barrier(Smi::FromInt(value));
}

// WasmExportedFunctionData
Tagged<WasmTrustedInstanceData> WasmExportedFunctionData::instance_data()
    const {
  DCHECK(has_instance_data());
  return protected_instance_data_.load();
}
void WasmExportedFunctionData::set_instance_data(
    Tagged<WasmTrustedInstanceData> value, WriteBarrierMode mode) {
  protected_instance_data_.store(this, value, mode);
}
bool WasmExportedFunctionData::has_instance_data() const {
  return !protected_instance_data_.load().is_null();
}
void WasmExportedFunctionData::clear_instance_data() {
  protected_instance_data_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<Code> WasmExportedFunctionData::c_wrapper_code(
    IsolateForSandbox isolate) const {
  return c_wrapper_code_.load(isolate);
}
void WasmExportedFunctionData::set_c_wrapper_code(Tagged<Code> value,
                                                  WriteBarrierMode mode) {
  c_wrapper_code_.store(this, value, mode);
}

int WasmExportedFunctionData::function_index() const {
  return function_index_.load().value();
}
void WasmExportedFunctionData::set_function_index(int value) {
  function_index_.store_no_write_barrier(Smi::FromInt(value));
}

Tagged<Cell> WasmExportedFunctionData::wrapper_budget() const {
  return wrapper_budget_.load();
}
void WasmExportedFunctionData::set_wrapper_budget(Tagged<Cell> value,
                                                  WriteBarrierMode mode) {
  wrapper_budget_.store(this, value, mode);
}

int WasmExportedFunctionData::packed_args_size() const {
  return packed_args_size_.load().value();
}
void WasmExportedFunctionData::set_packed_args_size(int value) {
  packed_args_size_.store_no_write_barrier(Smi::FromInt(value));
}

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

// WasmCapiFunctionData
Tagged<Foreign> WasmCapiFunctionData::embedder_data() const {
  return embedder_data_.load();
}
void WasmCapiFunctionData::set_embedder_data(Tagged<Foreign> value,
                                             WriteBarrierMode mode) {
  embedder_data_.store(this, value, mode);
}

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
uint32_t WasmTypeInfo::canonical_type() const { return canonical_type_; }
void WasmTypeInfo::set_canonical_type(uint32_t value) {
  canonical_type_ = value;
}

uint32_t WasmTypeInfo::canonical_element_type() const {
  return canonical_element_type_;
}
void WasmTypeInfo::set_canonical_element_type(uint32_t value) {
  canonical_element_type_ = value;
}

int WasmTypeInfo::supertypes_length() const {
  return supertypes_length_.load().value();
}
void WasmTypeInfo::set_supertypes_length(int value) {
  supertypes_length_.store(this, Smi::FromInt(value));
}

Tagged<Object> WasmTypeInfo::supertypes(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, supertypes_length());
  return supertypes()[i].load();
}
void WasmTypeInfo::set_supertypes(int i, Tagged<Object> value,
                                  WriteBarrierMode mode) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, supertypes_length());
  supertypes()[i].store(this, value, mode);
}

int WasmTypeInfo::AllocatedSize() const { return SizeFor(supertypes_length()); }

wasm::CanonicalValueType WasmTypeInfo::type() const {
  return wasm::CanonicalValueType::FromRawBitField(canonical_type());
}

wasm::CanonicalTypeIndex WasmTypeInfo::type_index() const {
  return type().ref_index();
}

wasm::CanonicalValueType WasmTypeInfo::element_type() const {
  return wasm::CanonicalValueType::FromRawBitField(canonical_element_type());
}

// WasmContinuationObject
Tagged<WasmStackObject> WasmContinuationObject::stack_obj() const {
  return stack_obj_.load();
}
void WasmContinuationObject::set_stack_obj(Tagged<WasmStackObject> value,
                                           WriteBarrierMode mode) {
  stack_obj_.store(this, value, mode);
}

#undef OPTIONAL_ACCESSORS
#undef READ_PRIMITIVE_FIELD
#undef WRITE_PRIMITIVE_FIELD
#undef PRIMITIVE_ACCESSORS

// WasmTableObject
Tagged<FixedArray> WasmTableObject::entries() const { return entries_.load(); }
void WasmTableObject::set_entries(Tagged<FixedArray> value,
                                  WriteBarrierMode mode) {
  entries_.store(this, value, mode);
}

int WasmTableObject::current_length() const {
  return current_length_.load().value();
}
void WasmTableObject::set_current_length(int value) {
  current_length_.store(this, Smi::FromInt(value));
}

Tagged<UnionOf<Smi, HeapNumber, BigInt, Undefined>>
WasmTableObject::maximum_length() const {
  return maximum_length_.load();
}
void WasmTableObject::set_maximum_length(
    Tagged<UnionOf<Smi, HeapNumber, BigInt, Undefined>> value,
    WriteBarrierMode mode) {
  maximum_length_.store(this, value, mode);
}

int WasmTableObject::raw_type() const { return raw_type_.load().value(); }
void WasmTableObject::set_raw_type(int value) {
  raw_type_.store(this, Smi::FromInt(value));
}

wasm::AddressType WasmTableObject::address_type() const {
  return static_cast<wasm::AddressType>(address_type_);
}
void WasmTableObject::set_address_type(wasm::AddressType value) {
  address_type_ = static_cast<uint8_t>(value);
}

Tagged<WasmTrustedInstanceData> WasmTableObject::trusted_data(
    IsolateForSandbox isolate) const {
  return trusted_data_.load(isolate);
}
Tagged<WasmTrustedInstanceData> WasmTableObject::trusted_data(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_data_.Acquire_Load(isolate);
}
void WasmTableObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                       WriteBarrierMode mode) {
  trusted_data_.store(this, value, mode);
}
void WasmTableObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                       ReleaseStoreTag, WriteBarrierMode mode) {
  trusted_data_.Release_Store(this, value, mode);
}
bool WasmTableObject::has_trusted_data() const {
  return !trusted_data_.is_empty();
}
bool WasmTableObject::has_trusted_data_unpublished(
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), kTrustedDataOffset,
      kWasmTrustedInstanceDataIndirectPointerTag, isolate);
}
void WasmTableObject::clear_trusted_data() { trusted_data_.clear(this); }

Tagged<WasmDispatchTable> WasmTableObject::trusted_dispatch_table(
    IsolateForSandbox isolate) const {
  return trusted_dispatch_table_.load(isolate);
}
Tagged<WasmDispatchTable> WasmTableObject::trusted_dispatch_table(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_dispatch_table_.Acquire_Load(isolate);
}
void WasmTableObject::set_trusted_dispatch_table(
    Tagged<WasmDispatchTable> value, WriteBarrierMode mode) {
  trusted_dispatch_table_.store(this, value, mode);
}
void WasmTableObject::set_trusted_dispatch_table(
    Tagged<WasmDispatchTable> value, ReleaseStoreTag, WriteBarrierMode mode) {
  trusted_dispatch_table_.Release_Store(this, value, mode);
}
bool WasmTableObject::has_trusted_dispatch_table() const {
  return !trusted_dispatch_table_.is_empty();
}
bool WasmTableObject::has_trusted_dispatch_table_unpublished(
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), kTrustedDispatchTableOffset,
      kWasmDispatchTableIndirectPointerTag, isolate);
}
void WasmTableObject::clear_trusted_dispatch_table() {
  trusted_dispatch_table_.clear(this);
}

// WasmResumeData
Tagged<WasmSuspenderObject> WasmResumeData::trusted_suspender(
    IsolateForSandbox isolate) const {
  return trusted_suspender_.load(isolate);
}
Tagged<WasmSuspenderObject> WasmResumeData::trusted_suspender(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_suspender_.Acquire_Load(isolate);
}
void WasmResumeData::set_trusted_suspender(Tagged<WasmSuspenderObject> value,
                                           WriteBarrierMode mode) {
  trusted_suspender_.store(this, value, mode);
}
void WasmResumeData::set_trusted_suspender(Tagged<WasmSuspenderObject> value,
                                           ReleaseStoreTag,
                                           WriteBarrierMode mode) {
  trusted_suspender_.Release_Store(this, value, mode);
}
bool WasmResumeData::has_trusted_suspender() const {
  return !trusted_suspender_.is_empty();
}
void WasmResumeData::clear_trusted_suspender() {
  trusted_suspender_.clear(this);
}

int WasmResumeData::on_resume() const { return on_resume_.load().value(); }
void WasmResumeData::set_on_resume(int value) {
  on_resume_.store(this, Smi::FromInt(value));
}

Tagged<WasmSuspenderObject> WasmSuspenderObject::parent() const {
  DCHECK(has_parent());
  return parent_.load();
}
void WasmSuspenderObject::set_parent(Tagged<WasmSuspenderObject> value,
                                     WriteBarrierMode mode) {
  parent_.store(this, value, mode);
}
bool WasmSuspenderObject::has_parent() const {
  return !parent_.load().is_null();
}
void WasmSuspenderObject::clear_parent() {
  parent_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<UnionOf<JSPromise, Undefined>> WasmSuspenderObject::promise() const {
  return promise_.load();
}
void WasmSuspenderObject::set_promise(
    Tagged<UnionOf<JSPromise, Undefined>> value, WriteBarrierMode mode) {
  promise_.store(this, value, mode);
}

Tagged<UnionOf<JSObject, Undefined>> WasmSuspenderObject::resume() const {
  return resume_.load();
}
void WasmSuspenderObject::set_resume(Tagged<UnionOf<JSObject, Undefined>> value,
                                     WriteBarrierMode mode) {
  resume_.store(this, value, mode);
}

Tagged<UnionOf<JSObject, Undefined>> WasmSuspenderObject::reject() const {
  return reject_.load();
}
void WasmSuspenderObject::set_reject(Tagged<UnionOf<JSObject, Undefined>> value,
                                     WriteBarrierMode mode) {
  reject_.store(this, value, mode);
}

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
    case wasm::kI32: {
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
  return FIELD_ADDR(Tagged<WasmStruct>(this), offset);
}

ObjectSlot WasmStruct::RawField(int raw_offset) {
  return ObjectSlot(RawFieldAddress(raw_offset));
}

void WasmStruct::SetTaggedFieldValue(int raw_offset, Tagged<Object> value,
                                     WriteBarrierMode mode) {
  int offset = WasmStruct::kHeaderSize + raw_offset;
  TaggedField<Object>::store(Tagged<WasmStruct>(this), offset, value);
  CONDITIONAL_WRITE_BARRIER(Tagged<HeapObject>(this), offset, value, mode);
}

Tagged<Map> WasmStruct::described_rtt() const {
  Tagged<Map> value = TaggedField<Map, kHeaderSize>::load(
      GetPtrComprCageBase(Tagged<WasmStruct>(this)), Tagged<WasmStruct>(this));
  DCHECK(GcSafeType(map())->is_descriptor());
  return value;
}

void WasmStruct::set_described_rtt(Tagged<Map> value, WriteBarrierMode mode) {
  DCHECK(GcSafeType(map())->is_descriptor());
  TaggedField<Map, kHeaderSize>::store(Tagged<WasmStruct>(this), value);
  CONDITIONAL_WRITE_BARRIER(Tagged<HeapObject>(this), kHeaderSize, value, mode);
}

uint32_t WasmArray::length() const { return length_; }
void WasmArray::set_length(uint32_t value) { length_ = value; }

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

wasm::StackMemory* WasmSuspenderObject::stack(IsolateForSandbox isolate) const {
  Address result = stack_.load<kWasmStackMemoryTag>(isolate);
  return reinterpret_cast<wasm::StackMemory*>(result);
}
wasm::StackMemory* WasmSuspenderObject::stack() const {
  return stack(GetCurrentIsolateForSandbox());
}
void WasmSuspenderObject::init_stack(IsolateForSandbox isolate,
                                     const wasm::StackMemory* initial_value) {
  stack_.Init<kWasmStackMemoryTag>(address(), isolate,
                                   reinterpret_cast<Address>(initial_value));
}
void WasmSuspenderObject::set_stack(IsolateForSandbox isolate,
                                    const wasm::StackMemory* value) {
  stack_.store<kWasmStackMemoryTag>(isolate, reinterpret_cast<Address>(value));
}

EXTERNAL_POINTER_ACCESSORS(WasmStackObject, stack, wasm::StackMemory*,
                           kStackOffset, kWasmStackMemoryTag)

// WasmTagObject
Tagged<HeapObject> WasmTagObject::tag() const { return tag_.load(); }
void WasmTagObject::set_tag(Tagged<HeapObject> value, WriteBarrierMode mode) {
  tag_.store(this, value, mode);
}

int WasmTagObject::canonical_type_index() const {
  return canonical_type_index_.load().value();
}
void WasmTagObject::set_canonical_type_index(int value) {
  canonical_type_index_.store(this, Smi::FromInt(value));
}

Tagged<WasmTrustedInstanceData> WasmTagObject::trusted_data(
    IsolateForSandbox isolate) const {
  return trusted_data_.load(isolate);
}
Tagged<WasmTrustedInstanceData> WasmTagObject::trusted_data(
    IsolateForSandbox isolate, AcquireLoadTag) const {
  return trusted_data_.Acquire_Load(isolate);
}
void WasmTagObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                     WriteBarrierMode mode) {
  trusted_data_.store(this, value, mode);
}
void WasmTagObject::set_trusted_data(Tagged<WasmTrustedInstanceData> value,
                                     ReleaseStoreTag, WriteBarrierMode mode) {
  trusted_data_.Release_Store(this, value, mode);
}
bool WasmTagObject::has_trusted_data() const {
  return !trusted_data_.is_empty();
}
bool WasmTagObject::has_trusted_data_unpublished(
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), kTrustedDataOffset,
      kWasmTrustedInstanceDataIndirectPointerTag, isolate);
}
void WasmTagObject::clear_trusted_data() { trusted_data_.clear(this); }

int WasmExceptionTag::index() const { return index_.load().value(); }
void WasmExceptionTag::set_index(int value) {
  index_.store(this, Smi::FromInt(value));
}

Tagged<Managed<wasm::NativeModule>> AsmWasmData::managed_native_module() const {
  return managed_native_module_.load();
}
void AsmWasmData::set_managed_native_module(
    Tagged<Managed<wasm::NativeModule>> value, WriteBarrierMode mode) {
  managed_native_module_.store(this, value, mode);
}

Tagged<HeapNumber> AsmWasmData::uses_bitset() const {
  return uses_bitset_.load();
}
void AsmWasmData::set_uses_bitset(Tagged<HeapNumber> value,
                                  WriteBarrierMode mode) {
  uses_bitset_.store(this, value, mode);
}

Tagged<JSReceiver> WasmSuspendingObject::callable() const {
  return callable_.load();
}
void WasmSuspendingObject::set_callable(Tagged<JSReceiver> value,
                                        WriteBarrierMode mode) {
  callable_.store(this, value, mode);
}

Tagged<HeapObject> WasmFastApiCallData::signature() const {
  return signature_.load();
}
void WasmFastApiCallData::set_signature(Tagged<HeapObject> value,
                                        WriteBarrierMode mode) {
  signature_.store(this, value, mode);
}
Tagged<Object> WasmFastApiCallData::callback_data() const {
  return callback_data_.load();
}
void WasmFastApiCallData::set_callback_data(Tagged<Object> value,
                                            WriteBarrierMode mode) {
  callback_data_.store(this, value, mode);
}
Tagged<MaybeObject> WasmFastApiCallData::cached_map() const {
  return cached_map_.load();
}
void WasmFastApiCallData::set_cached_map(Tagged<MaybeObject> value,
                                         WriteBarrierMode mode) {
  cached_map_.store(this, value, mode);
}

Tagged<String> WasmStringViewIter::string() const { return string_.load(); }
void WasmStringViewIter::set_string(Tagged<String> value,
                                    WriteBarrierMode mode) {
  string_.store(this, value, mode);
}
uint32_t WasmStringViewIter::offset() const { return offset_; }
void WasmStringViewIter::set_offset(uint32_t value) { offset_ = value; }

#include "src/objects/object-macros-undef.h"

}  // namespace v8::internal

#endif  // V8_WASM_WASM_OBJECTS_INL_H_
