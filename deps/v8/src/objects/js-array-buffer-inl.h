// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-array-buffer-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSArrayBuffer)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSArrayBufferView)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTypedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSDataView)

ACCESSORS(JSTypedArray, base_pointer, Object, kBasePointerOffset)
RELEASE_ACQUIRE_ACCESSORS(JSTypedArray, base_pointer, Object,
                          kBasePointerOffset)

size_t JSArrayBuffer::byte_length() const {
  return ReadField<size_t>(kByteLengthOffset);
}

void JSArrayBuffer::set_byte_length(size_t value) {
  WriteField<size_t>(kByteLengthOffset, value);
}

DEF_GETTER(JSArrayBuffer, backing_store, void*) {
  Address value = ReadSandboxedPointerField(kBackingStoreOffset, cage_base);
  return reinterpret_cast<void*>(value);
}

void JSArrayBuffer::set_backing_store(Isolate* isolate, void* value) {
  Address addr = reinterpret_cast<Address>(value);
  WriteSandboxedPointerField(kBackingStoreOffset, isolate, addr);
}

std::shared_ptr<BackingStore> JSArrayBuffer::GetBackingStore() const {
  if (!extension()) return nullptr;
  return extension()->backing_store();
}

size_t JSArrayBuffer::GetByteLength() const {
  if V8_UNLIKELY (is_shared() && is_resizable()) {
    // Invariant: byte_length for GSAB is 0 (it needs to be read from the
    // BackingStore).
    DCHECK_EQ(0, byte_length());

    return GetBackingStore()->byte_length(std::memory_order_seq_cst);
  }
  return byte_length();
}

uint32_t JSArrayBuffer::GetBackingStoreRefForDeserialization() const {
  return static_cast<uint32_t>(ReadField<Address>(kBackingStoreOffset));
}

void JSArrayBuffer::SetBackingStoreRefForSerialization(uint32_t ref) {
  WriteField<Address>(kBackingStoreOffset, static_cast<Address>(ref));
}

ArrayBufferExtension* JSArrayBuffer::extension() const {
#if V8_COMPRESS_POINTERS
    // With pointer compression the extension-field might not be
    // pointer-aligned. However on ARM64 this field needs to be aligned to
    // perform atomic operations on it. Therefore we split the pointer into two
    // 32-bit words that we update atomically. We don't have an ABA problem here
    // since there can never be an Attach() after Detach() (transitions only
    // from NULL --> some ptr --> NULL).

    // Synchronize with publishing release store of non-null extension
    uint32_t lo = base::AsAtomic32::Acquire_Load(extension_lo());
    if (lo & kUninitializedTagMask) return nullptr;

    // Synchronize with release store of null extension
    uint32_t hi = base::AsAtomic32::Acquire_Load(extension_hi());
    uint32_t verify_lo = base::AsAtomic32::Relaxed_Load(extension_lo());
    if (lo != verify_lo) return nullptr;

    uintptr_t address = static_cast<uintptr_t>(lo);
    address |= static_cast<uintptr_t>(hi) << 32;
    return reinterpret_cast<ArrayBufferExtension*>(address);
#else
    return base::AsAtomicPointer::Acquire_Load(extension_location());
#endif
}

void JSArrayBuffer::set_extension(ArrayBufferExtension* extension) {
#if V8_COMPRESS_POINTERS
    if (extension != nullptr) {
      uintptr_t address = reinterpret_cast<uintptr_t>(extension);
      base::AsAtomic32::Relaxed_Store(extension_hi(),
                                      static_cast<uint32_t>(address >> 32));
      base::AsAtomic32::Release_Store(extension_lo(),
                                      static_cast<uint32_t>(address));
    } else {
      base::AsAtomic32::Relaxed_Store(extension_lo(),
                                      0 | kUninitializedTagMask);
      base::AsAtomic32::Release_Store(extension_hi(), 0);
    }
#else
    base::AsAtomicPointer::Release_Store(extension_location(), extension);
#endif
    WriteBarrier::Marking(*this, extension);
}

ArrayBufferExtension** JSArrayBuffer::extension_location() const {
  Address location = field_address(kExtensionOffset);
  return reinterpret_cast<ArrayBufferExtension**>(location);
}

#if V8_COMPRESS_POINTERS
uint32_t* JSArrayBuffer::extension_lo() const {
  Address location = field_address(kExtensionOffset);
  return reinterpret_cast<uint32_t*>(location);
}

uint32_t* JSArrayBuffer::extension_hi() const {
  Address location = field_address(kExtensionOffset) + sizeof(uint32_t);
  return reinterpret_cast<uint32_t*>(location);
}
#endif

void JSArrayBuffer::clear_padding() {
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }
}

void JSArrayBuffer::set_bit_field(uint32_t bits) {
  RELAXED_WRITE_UINT32_FIELD(*this, kBitFieldOffset, bits);
}

uint32_t JSArrayBuffer::bit_field() const {
  return RELAXED_READ_UINT32_FIELD(*this, kBitFieldOffset);
}

// |bit_field| fields.
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_external,
                    JSArrayBuffer::IsExternalBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_detachable,
                    JSArrayBuffer::IsDetachableBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, was_detached,
                    JSArrayBuffer::WasDetachedBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_asmjs_memory,
                    JSArrayBuffer::IsAsmJsMemoryBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_shared,
                    JSArrayBuffer::IsSharedBit)
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_resizable,
                    JSArrayBuffer::IsResizableBit)

bool JSArrayBuffer::IsEmpty() const {
  auto backing_store = GetBackingStore();
  bool is_empty = !backing_store || backing_store->IsEmpty();
  DCHECK_IMPLIES(is_empty, byte_length() == 0);
  return is_empty;
}

size_t JSArrayBufferView::byte_offset() const {
  return ReadField<size_t>(kByteOffsetOffset);
}

void JSArrayBufferView::set_byte_offset(size_t value) {
  WriteField<size_t>(kByteOffsetOffset, value);
}

size_t JSArrayBufferView::byte_length() const {
  return ReadField<size_t>(kByteLengthOffset);
}

void JSArrayBufferView::set_byte_length(size_t value) {
  WriteField<size_t>(kByteLengthOffset, value);
}

bool JSArrayBufferView::WasDetached() const {
  return JSArrayBuffer::cast(buffer()).was_detached();
}

BIT_FIELD_ACCESSORS(JSArrayBufferView, bit_field, is_length_tracking,
                    JSArrayBufferView::IsLengthTrackingBit)
BIT_FIELD_ACCESSORS(JSArrayBufferView, bit_field, is_backed_by_rab,
                    JSArrayBufferView::IsBackedByRabBit)

bool JSArrayBufferView::IsVariableLength() const {
  return is_length_tracking() || is_backed_by_rab();
}

size_t JSTypedArray::GetLengthOrOutOfBounds(bool& out_of_bounds) const {
  DCHECK(!out_of_bounds);
  if (WasDetached()) return 0;
  if (IsVariableLength()) {
    return GetVariableLengthOrOutOfBounds(out_of_bounds);
  }
  return LengthUnchecked();
}

size_t JSTypedArray::GetLength() const {
  bool out_of_bounds = false;
  return GetLengthOrOutOfBounds(out_of_bounds);
}

size_t JSTypedArray::GetByteLength() const {
  return GetLength() * element_size();
}

bool JSTypedArray::IsOutOfBounds() const {
  bool out_of_bounds = false;
  GetLengthOrOutOfBounds(out_of_bounds);
  return out_of_bounds;
}

bool JSTypedArray::IsDetachedOrOutOfBounds() const {
  if (WasDetached()) {
    return true;
  }
  bool out_of_bounds = false;
  GetLengthOrOutOfBounds(out_of_bounds);
  return out_of_bounds;
}

size_t JSTypedArray::length() const {
  DCHECK(!is_length_tracking());
  DCHECK(!is_backed_by_rab());
  return ReadField<size_t>(kLengthOffset);
}

size_t JSTypedArray::LengthUnchecked() const {
  return ReadField<size_t>(kLengthOffset);
}

void JSTypedArray::set_length(size_t value) {
  WriteField<size_t>(kLengthOffset, value);
}

DEF_GETTER(JSTypedArray, external_pointer, Address) {
  return ReadSandboxedPointerField(kExternalPointerOffset, cage_base);
}

void JSTypedArray::set_external_pointer(Isolate* isolate, Address value) {
  WriteSandboxedPointerField(kExternalPointerOffset, isolate, value);
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
  return static_cast<uint32_t>(ReadField<Address>(kExternalPointerOffset));
}

void JSTypedArray::SetExternalBackingStoreRefForSerialization(uint32_t ref) {
  DCHECK(!is_on_heap());
  WriteField<Address>(kExternalPointerOffset, static_cast<Address>(ref));
}

void JSTypedArray::RemoveExternalPointerCompensationForSerialization(
    Isolate* isolate) {
  DCHECK(is_on_heap());
  Address offset =
      external_pointer() - ExternalPointerCompensationForOnHeapArray(isolate);
  WriteField<Address>(kExternalPointerOffset, offset);
}

void JSTypedArray::AddExternalPointerCompensationForDeserialization(
    Isolate* isolate) {
  DCHECK(is_on_heap());
  Address pointer = ReadField<Address>(kExternalPointerOffset) +
                    ExternalPointerCompensationForOnHeapArray(isolate);
  set_external_pointer(isolate, pointer);
}

void* JSTypedArray::DataPtr() {
  // Zero-extend Tagged_t to Address according to current compression scheme
  // so that the addition with |external_pointer| (which already contains
  // compensated offset value) will decompress the tagged value.
  // See JSTypedArray::ExternalPointerCompensationForOnHeapArray() for details.
  STATIC_ASSERT(kOffHeapDataPtrEqualsExternalPointer);
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
MaybeHandle<JSTypedArray> JSTypedArray::Validate(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 const char* method_name) {
  if (V8_UNLIKELY(!receiver->IsJSTypedArray())) {
    const MessageTemplate message = MessageTemplate::kNotTypedArray;
    THROW_NEW_ERROR(isolate, NewTypeError(message), JSTypedArray);
  }

  Handle<JSTypedArray> array = Handle<JSTypedArray>::cast(receiver);
  if (V8_UNLIKELY(array->WasDetached())) {
    const MessageTemplate message = MessageTemplate::kDetachedOperation;
    Handle<String> operation =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR(isolate, NewTypeError(message, operation), JSTypedArray);
  }

  if (V8_UNLIKELY(array->IsVariableLength() && array->IsOutOfBounds())) {
    const MessageTemplate message = MessageTemplate::kDetachedOperation;
    Handle<String> operation =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR(isolate, NewTypeError(message, operation), JSTypedArray);
  }

  // spec describes to return `buffer`, but it may disrupt current
  // implementations, and it's much useful to return array for now.
  return array;
}

DEF_GETTER(JSDataView, data_pointer, void*) {
  Address value = ReadSandboxedPointerField(kDataPointerOffset, cage_base);
  return reinterpret_cast<void*>(value);
}

void JSDataView::set_data_pointer(Isolate* isolate, void* ptr) {
  Address value = reinterpret_cast<Address>(ptr);
  WriteSandboxedPointerField(kDataPointerOffset, isolate, value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
