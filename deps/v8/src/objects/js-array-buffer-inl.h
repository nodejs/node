// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_

#include "src/objects/js-array-buffer.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-engine.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSArrayBuffer, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSArrayBufferView, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSTypedArray, JSArrayBufferView)
OBJECT_CONSTRUCTORS_IMPL(JSDataView, JSArrayBufferView)

CAST_ACCESSOR(JSArrayBuffer)
CAST_ACCESSOR(JSArrayBufferView)
CAST_ACCESSOR(JSTypedArray)
CAST_ACCESSOR(JSDataView)

size_t JSArrayBuffer::byte_length() const {
  return ReadField<size_t>(kByteLengthOffset);
}

void JSArrayBuffer::set_byte_length(size_t value) {
  WriteField<size_t>(kByteLengthOffset, value);
}

void* JSArrayBuffer::backing_store() const {
  return reinterpret_cast<void*>(ReadField<Address>(kBackingStoreOffset));
}

void JSArrayBuffer::set_backing_store(void* value, WriteBarrierMode mode) {
  WriteField<Address>(kBackingStoreOffset, reinterpret_cast<Address>(value));
}

size_t JSArrayBuffer::allocation_length() const {
  if (backing_store() == nullptr) {
    return 0;
  }
  return byte_length();
}

void* JSArrayBuffer::allocation_base() const {
  if (backing_store() == nullptr) {
    return nullptr;
  }
  return backing_store();
}

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

ACCESSORS(JSArrayBufferView, buffer, Object, kBufferOffset)

bool JSArrayBufferView::WasDetached() const {
  return JSArrayBuffer::cast(buffer()).was_detached();
}

size_t JSTypedArray::length() const { return ReadField<size_t>(kLengthOffset); }

void JSTypedArray::set_length(size_t value) {
  WriteField<size_t>(kLengthOffset, value);
}

Address JSTypedArray::external_pointer() const {
  return ReadField<Address>(kExternalPointerOffset);
}

void JSTypedArray::set_external_pointer(Address value) {
  WriteField<Address>(kExternalPointerOffset, value);
}

Address JSTypedArray::ExternalPointerCompensationForOnHeapArray(
    const Isolate* isolate) {
#ifdef V8_COMPRESS_POINTERS
  return GetIsolateRoot(isolate);
#else
  return 0;
#endif
}

void JSTypedArray::RemoveExternalPointerCompensationForSerialization() {
  DCHECK(is_on_heap());
  const Isolate* isolate = GetIsolateForPtrCompr(*this);
  set_external_pointer(external_pointer() -
                       ExternalPointerCompensationForOnHeapArray(isolate));
}

ACCESSORS(JSTypedArray, base_pointer, Object, kBasePointerOffset)

void* JSTypedArray::DataPtr() {
  // Zero-extend Tagged_t to Address according to current compression scheme
  // so that the addition with |external_pointer| (which already contains
  // compensated offset value) will decompress the tagged value.
  // See JSTypedArray::ExternalPointerCompensationForOnHeapArray() for details.
  return reinterpret_cast<void*>(external_pointer() +
                                 static_cast<Tagged_t>(base_pointer().ptr()));
}

void JSTypedArray::SetOffHeapDataPtr(void* base, Address offset) {
  set_base_pointer(Smi::zero(), SKIP_WRITE_BARRIER);
  Address address = reinterpret_cast<Address>(base) + offset;
  set_external_pointer(address);
  DCHECK_EQ(address, reinterpret_cast<Address>(DataPtr()));
}

void JSTypedArray::SetOnHeapDataPtr(HeapObject base, Address offset) {
  set_base_pointer(base);
  const Isolate* isolate = GetIsolateForPtrCompr(*this);
  set_external_pointer(offset +
                       ExternalPointerCompensationForOnHeapArray(isolate));
  DCHECK_EQ(base.ptr() + offset, reinterpret_cast<Address>(DataPtr()));
}

bool JSTypedArray::is_on_heap() const {
  DisallowHeapAllocation no_gc;
  // Checking that buffer()->backing_store() is not nullptr is not sufficient;
  // it will be nullptr when byte_length is 0 as well.
  return base_pointer() == elements();
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

  // spec describes to return `buffer`, but it may disrupt current
  // implementations, and it's much useful to return array for now.
  return array;
}

void* JSDataView::data_pointer() const {
  return reinterpret_cast<void*>(ReadField<Address>(kDataPointerOffset));
}

void JSDataView::set_data_pointer(void* value) {
  WriteField<Address>(kDataPointerOffset, reinterpret_cast<Address>(value));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
