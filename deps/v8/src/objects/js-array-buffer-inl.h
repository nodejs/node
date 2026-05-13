// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_

#include "src/objects/js-array-buffer.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/utils/memcopy.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

//
// JSArrayBuffer.
//

Tagged<MaybeObject> JSArrayBuffer::views_or_detach_key() const {
  return views_or_detach_key_.load();
}

void JSArrayBuffer::set_views_or_detach_key(Tagged<MaybeObject> value,
                                            WriteBarrierMode mode) {
  views_or_detach_key_.store(this, value, mode);
}

size_t JSArrayBuffer::byte_length() const {
  // Use GetByteLength() for growable SharedArrayBuffers.
  DCHECK(!is_shared() || !is_resizable_by_js());
  return byte_length_unchecked();
}

size_t JSArrayBuffer::byte_length_unchecked() const {
  return ReadBoundedSizeField(offsetof(JSArrayBuffer, raw_byte_length_));
}

void JSArrayBuffer::set_byte_length(size_t value) {
  WriteBoundedSizeField(offsetof(JSArrayBuffer, raw_byte_length_), value);
}

size_t JSArrayBuffer::max_byte_length() const {
  return ReadBoundedSizeField(offsetof(JSArrayBuffer, raw_max_byte_length_));
}

void JSArrayBuffer::set_max_byte_length(size_t value) {
  WriteBoundedSizeField(offsetof(JSArrayBuffer, raw_max_byte_length_), value);
}

void* JSArrayBuffer::backing_store() const {
  return backing_store(GetPtrComprCageBase(this));
}

void* JSArrayBuffer::backing_store(PtrComprCageBase cage_base) const {
  Address value = ReadSandboxedPointerField(
      offsetof(JSArrayBuffer, backing_store_), cage_base);
  return reinterpret_cast<void*>(value);
}

void JSArrayBuffer::set_backing_store(Isolate* isolate, void* value) {
  Address addr = reinterpret_cast<Address>(value);
  WriteSandboxedPointerField(offsetof(JSArrayBuffer, backing_store_), isolate,
                             addr);
}

std::shared_ptr<BackingStore> JSArrayBuffer::GetBackingStore() const {
  if (!extension()) return nullptr;
  return extension()->backing_store();
}

size_t JSArrayBuffer::GetByteLength() const {
  if (V8_UNLIKELY(is_shared() && is_resizable_by_js())) {
    // Invariant: byte_length for GSAB is 0 (it needs to be read from the
    // BackingStore). Don't use the byte_length getter, which DCHECKs that it's
    // not used on growable SharedArrayBuffers.
    DCHECK_EQ(0, byte_length_unchecked());

    // If the byte length is read after the JSArrayBuffer object is allocated
    // but before it's attached to the backing store, GetBackingStore returns
    // nullptr. This is rare, but can happen e.g., when memory measurements
    // are enabled (via performance.measureMemory()).
    auto backing_store = GetBackingStore();
    if (!backing_store) {
      return 0;
    }

    return backing_store->byte_length(std::memory_order_seq_cst);
  }
  return byte_length();
}

uint32_t JSArrayBuffer::GetBackingStoreRefForDeserialization() const {
  return static_cast<uint32_t>(
      ReadField<Address>(offsetof(JSArrayBuffer, backing_store_)));
}

void JSArrayBuffer::SetBackingStoreRefForSerialization(uint32_t ref) {
  WriteField<Address>(offsetof(JSArrayBuffer, backing_store_),
                      static_cast<Address>(ref));
}

void JSArrayBuffer::init_extension() {
#if V8_COMPRESS_POINTERS
  // The extension field is lazily-initialized, so set it to null initially.
  base::AsAtomic32::Release_Store(extension_handle_location(),
                                  kNullExternalPointerHandle);
#else
  base::AsAtomicPointer::Release_Store(extension_location(), nullptr);
#endif  // V8_COMPRESS_POINTERS
}

ArrayBufferExtension* JSArrayBuffer::extension() const {
#if V8_COMPRESS_POINTERS
  Isolate* isolate = Isolate::Current();
  ExternalPointerHandle handle =
      base::AsAtomic32::Relaxed_Load(extension_handle_location());
  return reinterpret_cast<ArrayBufferExtension*>(
      isolate->external_pointer_table().Get(handle, kArrayBufferExtensionTag));
#else
  return base::AsAtomicPointer::Relaxed_Load(extension_location());
#endif  // V8_COMPRESS_POINTERS
}

void JSArrayBuffer::set_extension(ArrayBufferExtension* extension) {
#if V8_COMPRESS_POINTERS
  // TODO(saelo): if we ever use the external pointer table for all external
  // pointer fields in the no-sandbox-ptr-compression config, replace this code
  // here and above with the respective external pointer accessors.
  IsolateForPointerCompression isolate = Isolate::Current();
  constexpr ExternalPointerTag tag = kArrayBufferExtensionTag;
  Address value = reinterpret_cast<Address>(extension);
  ExternalPointerTable& table = isolate.GetExternalPointerTableFor(tag);
  ExternalPointerHandle current_handle =
      base::AsAtomic32::Relaxed_Load(extension_handle_location());
  if (current_handle == kNullExternalPointerHandle) {
    ExternalPointerHandle handle = table.AllocateAndInitializeEntry(
        isolate.GetExternalPointerTableSpaceFor(tag, address()), value, tag);
    base::AsAtomic32::Relaxed_Store(extension_handle_location(), handle);
    WriteBarrier::ForExternalPointer(this, ExternalPointerSlot(&extension_));
  } else {
    table.Set(current_handle, value, tag);
  }
#else
  base::AsAtomicPointer::Relaxed_Store(extension_location(), extension);
#endif  // V8_COMPRESS_POINTERS
  WriteBarrier::ForArrayBufferExtension(this, extension);
}

#if V8_COMPRESS_POINTERS
ExternalPointerHandle* JSArrayBuffer::extension_handle_location() const {
  return reinterpret_cast<ExternalPointerHandle*>(extension_.storage_address());
}
#else
ArrayBufferExtension** JSArrayBuffer::extension_location() const {
  return reinterpret_cast<ArrayBufferExtension**>(extension_.storage_address());
}
#endif  // V8_COMPRESS_POINTERS

void JSArrayBuffer::clear_padding() {
#if TAGGED_SIZE_8_BYTES
  static_assert(sizeof(optional_padding_) == 4);
  optional_padding_ = 0;
#endif
}

Tagged<MaybeObject> JSArrayBuffer::views() const {
  if (has_detach_key()) return kManyViews;
  Tagged<MaybeObject> res = views_or_detach_key();
  DCHECK(res.IsSmi() || res.IsWeak() || res.IsCleared());
  return res;
}

void JSArrayBuffer::set_views(Tagged<MaybeObject> value,
                              WriteBarrierMode mode) {
  DCHECK(!has_detach_key());
  DCHECK(value.IsWeak() || value == kNoView || value == kManyViews);
  set_views_or_detach_key(value, mode);
}

Tagged<Cell> JSArrayBuffer::detach_key() const {
  DCHECK(has_detach_key());
  return Cast<Cell>(views_or_detach_key().GetHeapObjectAssumeStrong());
}

void JSArrayBuffer::set_detach_key(Tagged<Cell> value, WriteBarrierMode mode) {
  set_views_or_detach_key(value, mode);
}

bool JSArrayBuffer::has_detach_key() const {
  Tagged<MaybeObject> value = views_or_detach_key();
  return value.IsStrong() && IsCell(value.GetHeapObjectAssumeStrong());
}

Tagged<Object> JSArrayBuffer::DetachKey(Isolate* isolate) {
  if (!has_detach_key()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  return detach_key()->value();
}

inline void JSArrayBuffer::AttachView(Tagged<JSArrayBufferView> new_view) {
  if (!v8_flags.track_array_buffer_views) return;
  if (is_shared() || has_detach_key()) return;
  Tagged<MaybeObject> current_views = views();
  if (current_views == kManyViews) return;
  if (current_views.IsCleared() || current_views == kNoView) {
    set_views(MakeWeak(new_view));
    return;
  }
  DCHECK(IsJSArrayBufferView(current_views.GetHeapObjectAssumeWeak()));
  set_views(kManyViews);
}

void JSArrayBuffer::set_bit_field(uint32_t bits) {
  base::AsAtomic32::Relaxed_Store(&bit_field_, bits);
}

uint32_t JSArrayBuffer::bit_field() const {
  return base::AsAtomic32::Relaxed_Load(&bit_field_);
}

// |bit_field| fields.
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_external,
                    JSArrayBuffer::IsExternalBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_detachable,
                    JSArrayBuffer::IsDetachableBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, was_detached,
                    JSArrayBuffer::WasDetachedBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_shared,
                    JSArrayBuffer::IsSharedBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_resizable_by_js,
                    JSArrayBuffer::IsResizableByJsBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_immutable,
                    JSArrayBuffer::IsImmutableBit)

bool JSArrayBuffer::was_detached(AcquireLoadTag) const {
  uint32_t bits = base::AsAtomic32::Acquire_Load(&bit_field_);
  return WasDetachedBit::decode(bits);
}

void JSArrayBuffer::set_was_detached(bool value, ReleaseStoreTag) {
  uint32_t bits = bit_field();
  bits = WasDetachedBit::update(bits, value);
  base::AsAtomic32::Release_Store(&bit_field_, bits);
}

bool JSArrayBuffer::IsEmpty() const {
  auto backing_store = GetBackingStore();
  bool is_empty = !backing_store || backing_store->IsEmpty();
  DCHECK_IMPLIES(is_empty, byte_length() == 0);
  return is_empty;
}

//
// JSArrayBufferView.
//

Tagged<JSArrayBuffer> JSArrayBufferView::buffer() const {
  return buffer_.load();
}

void JSArrayBufferView::set_buffer(Tagged<JSArrayBuffer> value,
                                   WriteBarrierMode mode) {
  buffer_.store(this, value, mode);
}

uint32_t JSArrayBufferView::bit_field() const {
  return base::AsAtomic32::Relaxed_Load(&bit_field_);
}

void JSArrayBufferView::set_bit_field(uint32_t value) {
  base::AsAtomic32::Relaxed_Store(&bit_field_, value);
}

size_t JSArrayBufferView::byte_offset() const {
  return ReadBoundedSizeField(offsetof(JSArrayBufferView, raw_byte_offset_));
}

void JSArrayBufferView::set_byte_offset(size_t value) {
  WriteBoundedSizeField(offsetof(JSArrayBufferView, raw_byte_offset_), value);
}

size_t JSArrayBufferView::byte_length() const {
  return ReadBoundedSizeField(offsetof(JSArrayBufferView, raw_byte_length_));
}

void JSArrayBufferView::set_byte_length(size_t value) {
  WriteBoundedSizeField(offsetof(JSArrayBufferView, raw_byte_length_), value);
}

bool JSArrayBufferView::WasDetached() const { return buffer()->was_detached(); }

bool JSArrayBufferView::IsDetachedOrOutOfBounds() const {
  Tagged<JSArrayBuffer> backing_buffer = buffer();
  if (backing_buffer->was_detached()) {
    return true;
  }
  if (!is_backed_by_rab()) {
    // TypedArrays backed by GSABs or regular AB/SABs are never out of bounds.
    // This shortcut is load-bearing; this enables determining
    // IsDetachedOrOutOfBounds without consulting the BackingStore.
    return false;
  }
  size_t buffer_byte_length =
      backing_buffer->GetBackingStore()->byte_length();
  // Length-tracking ArrayBufferViews have byte_length() == 0, so this math
  // works.
  return byte_offset() + byte_length() > buffer_byte_length;
}

BIT_FIELD_ACCESSORS(JSArrayBufferView, bit_field, is_length_tracking,
                    JSArrayBufferView::IsLengthTrackingBit)
BIT_FIELD_ACCESSORS(JSArrayBufferView, bit_field, is_backed_by_rab,
                    JSArrayBufferView::IsBackedByRabBit)

bool JSArrayBufferView::IsVariableLength() const {
  return is_length_tracking() || is_backed_by_rab();
}

//
// JSTypedArray.
//

// static
constexpr std::pair<ExternalArrayType, size_t>
JSTypedArray::TypeAndElementSizeFor(ElementsKind kind) {
  switch (kind) {
#define ELEMENTS_KIND_TO_ARRAY_TYPE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                                      \
    return {kExternal##Type##Array, sizeof(ctype)};

    TYPED_ARRAYS(ELEMENTS_KIND_TO_ARRAY_TYPE)
    RAB_GSAB_TYPED_ARRAYS_WITH_TYPED_ARRAY_TYPE(ELEMENTS_KIND_TO_ARRAY_TYPE)
#undef ELEMENTS_KIND_TO_ARRAY_TYPE

    default:
      UNREACHABLE();
  }
}

size_t JSTypedArray::length() const {
  return ReadBoundedSizeField(offsetof(JSTypedArray, raw_length_));
}

void JSTypedArray::set_length(size_t value) {
  WriteBoundedSizeField(offsetof(JSTypedArray, raw_length_), value);
}

Tagged<Object> JSTypedArray::base_pointer() const {
  return base_pointer_.load();
}

Tagged<Object> JSTypedArray::base_pointer(AcquireLoadTag) const {
  return base_pointer_.Acquire_Load();
}

void JSTypedArray::set_base_pointer(Tagged<Object> value,
                                    WriteBarrierMode mode) {
  base_pointer_.store(this, value, mode);
}

void JSTypedArray::set_base_pointer(Tagged<Object> value, ReleaseStoreTag,
                                    WriteBarrierMode mode) {
  base_pointer_.Release_Store(this, value, mode);
}

Address JSTypedArray::external_pointer() const {
  return external_pointer(GetPtrComprCageBase(this));
}

Address JSTypedArray::external_pointer(PtrComprCageBase cage_base) const {
  return ReadSandboxedPointerField(offsetof(JSTypedArray, external_pointer_),
                                   cage_base);
}

void JSTypedArray::set_external_pointer(Isolate* isolate, Address value) {
  WriteSandboxedPointerField(offsetof(JSTypedArray, external_pointer_), isolate,
                             value);
}

size_t JSTypedArray::GetLengthOrOutOfBounds(bool& out_of_bounds) const {
  DCHECK(!out_of_bounds);
  if (WasDetached()) return 0;
  if (IsVariableLength()) {
    return GetVariableLengthOrOutOfBounds(out_of_bounds);
  }
  return byte_length() / element_size();
}

size_t JSTypedArray::GetLength() const {
  bool out_of_bounds = false;
  return GetLengthOrOutOfBounds(out_of_bounds);
}

size_t JSTypedArray::GetByteLength() const {
  if (WasDetached()) return 0;
  if (IsVariableLength()) {
    bool out_of_bounds = false;
    return GetVariableByteLengthOrOutOfBounds(out_of_bounds);
  }
  return byte_length();
}

bool JSTypedArray::IsOutOfBounds() const {
  bool out_of_bounds = false;
  GetLengthOrOutOfBounds(out_of_bounds);
  return out_of_bounds;
}

// static
inline void JSTypedArray::ForFixedTypedArray(ExternalArrayType array_type,
                                             size_t* element_size,
                                             ElementsKind* element_kind) {
  switch (array_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    *element_size = sizeof(ctype);                \
    *element_kind = TYPE##_ELEMENTS;              \
    return;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

Address JSTypedArray::ExternalPointerCompensationForOnHeapArray(
    PtrComprCageBase cage_base) {
#ifdef V8_COMPRESS_POINTERS
  return cage_base.address();
#else
  return 0;
#endif
}

uint32_t JSTypedArray::GetExternalBackingStoreRefForDeserialization() const {
  DCHECK(!is_on_heap());
  return static_cast<uint32_t>(
      ReadField<Address>(offsetof(JSTypedArray, external_pointer_)));
}

void JSTypedArray::SetExternalBackingStoreRefForSerialization(uint32_t ref) {
  DCHECK(!is_on_heap());
  WriteField<Address>(offsetof(JSTypedArray, external_pointer_),
                      static_cast<Address>(ref));
}

void JSTypedArray::RemoveExternalPointerCompensationForSerialization(
    Isolate* isolate) {
  DCHECK(is_on_heap());
  Address offset =
      external_pointer() - ExternalPointerCompensationForOnHeapArray(isolate);
  WriteField<Address>(offsetof(JSTypedArray, external_pointer_), offset);
}

void JSTypedArray::AddExternalPointerCompensationForDeserialization(
    Isolate* isolate) {
  DCHECK(is_on_heap());
  Address pointer =
      ReadField<Address>(offsetof(JSTypedArray, external_pointer_)) +
      ExternalPointerCompensationForOnHeapArray(isolate);
  set_external_pointer(isolate, pointer);
}

void* JSTypedArray::DataPtr() {
  // Zero-extend Tagged_t to Address according to current compression scheme
  // so that the addition with |external_pointer| (which already contains
  // compensated offset value) will decompress the tagged value.
  // See JSTypedArray::ExternalPointerCompensationForOnHeapArray() for details.
  static_assert(kOffHeapDataPtrEqualsExternalPointer);
  return reinterpret_cast<void*>(external_pointer() +
                                 static_cast<Tagged_t>(base_pointer().ptr()));
}

void JSTypedArray::SetOffHeapDataPtr(Isolate* isolate, void* base,
                                     Address offset) {
  Address address = reinterpret_cast<Address>(base) + offset;
  set_external_pointer(isolate, address);
  // This is the only spot in which the `base_pointer` field can be mutated
  // after object initialization. Note this can happen at most once, when
  // `JSTypedArray::GetBuffer` transitions from an on- to off-heap
  // representation.
  // To play well with Turbofan concurrency requirements, `base_pointer` is set
  // with a release store, after external_pointer has been set.
  set_base_pointer(Smi::zero(), kReleaseStore, SKIP_WRITE_BARRIER);
  DCHECK_EQ(address, reinterpret_cast<Address>(DataPtr()));
}

bool JSTypedArray::is_on_heap() const {
  // Keep synced with `is_on_heap(AcquireLoadTag)`.
  DisallowGarbageCollection no_gc;
  return base_pointer() != Smi::zero();
}

bool JSTypedArray::is_on_heap(AcquireLoadTag tag) const {
  // Keep synced with `is_on_heap()`.
  // Note: For Turbofan concurrency requirements, it's important that this
  // function reads only `base_pointer`.
  DisallowGarbageCollection no_gc;
  return base_pointer(tag) != Smi::zero();
}

// static
MaybeDirectHandle<JSTypedArray> JSTypedArray::Validate(
    Isolate* isolate, DirectHandle<Object> receiver, const char* method_name,
    TypedArrayAccessMode access_mode) {
  if (V8_UNLIKELY(!IsJSTypedArray(*receiver))) {
    const MessageTemplate message = MessageTemplate::kNotTypedArray;
    THROW_NEW_ERROR(isolate, NewTypeError(message));
  }

  // All errors throw the same message. In theory we could be more specific
  // here. However, many of the fast-paths do not distinguish and we'd rather be
  // consistent.
  const MessageTemplate message =
      access_mode == TypedArrayAccessMode::kWrite
          ? MessageTemplate::kTypedArrayValidateWriteErrorOperation
          : MessageTemplate::kTypedArrayValidateErrorOperation;

  DirectHandle<JSTypedArray> array = Cast<JSTypedArray>(receiver);
  if (V8_UNLIKELY(array->WasDetached())) {
    DirectHandle<String> operation =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR(isolate, NewTypeError(message, operation));
  }

  if (access_mode == TypedArrayAccessMode::kWrite &&
      Cast<JSArrayBuffer>(array->buffer())->is_immutable()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(message, isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  if (V8_UNLIKELY(array->IsVariableLength() && array->IsOutOfBounds())) {
    DirectHandle<String> operation =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR(isolate, NewTypeError(message, operation));
  }

  // spec describes to return `buffer`, but it may disrupt current
  // implementations, and it's much useful to return array for now.
  return array;
}

//
// JSDataViewOrRabGsabDataView.
//

void* JSDataViewOrRabGsabDataView::data_pointer() const {
  return data_pointer(GetPtrComprCageBase(this));
}

void* JSDataViewOrRabGsabDataView::data_pointer(
    PtrComprCageBase cage_base) const {
  Address value = ReadSandboxedPointerField(
      offsetof(JSDataViewOrRabGsabDataView, data_pointer_), cage_base);
  return reinterpret_cast<void*>(value);
}

void JSDataViewOrRabGsabDataView::set_data_pointer(Isolate* isolate,
                                                   void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  WriteSandboxedPointerField(
      offsetof(JSDataViewOrRabGsabDataView, data_pointer_), isolate, value);
}

size_t JSRabGsabDataView::GetByteLength() const {
  if (IsOutOfBounds()) {
    return 0;
  }
  if (is_length_tracking()) {
    // Invariant: byte_length of length tracking DataViews is 0.
    DCHECK_EQ(0, byte_length());
    return buffer()->GetByteLength() - byte_offset();
  }
  return byte_length();
}

bool JSRabGsabDataView::IsOutOfBounds() const {
  if (!is_backed_by_rab()) {
    return false;
  }
  if (is_length_tracking()) {
    return byte_offset() > buffer()->GetByteLength();
  }
  return byte_offset() + byte_length() > buffer()->GetByteLength();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
