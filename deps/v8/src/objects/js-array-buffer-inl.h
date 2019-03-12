// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
#define V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_

#include "src/objects/js-array-buffer.h"

#include "src/objects-inl.h"  // Needed for write barriers
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
  return READ_UINTPTR_FIELD(this, kByteLengthOffset);
}

void JSArrayBuffer::set_byte_length(size_t value) {
  WRITE_UINTPTR_FIELD(this, kByteLengthOffset, value);
}

void* JSArrayBuffer::backing_store() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kBackingStoreOffset);
  return reinterpret_cast<void*>(ptr);
}

void JSArrayBuffer::set_backing_store(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kBackingStoreOffset, ptr);
}

size_t JSArrayBuffer::allocation_length() const {
  if (backing_store() == nullptr) {
    return 0;
  }
  // If this buffer is managed by the WasmMemoryTracker
  if (is_wasm_memory()) {
    const auto* data =
        GetIsolate()->wasm_engine()->memory_tracker()->FindAllocationData(
            backing_store());
    DCHECK_NOT_NULL(data);
    return data->allocation_length;
  }
  return byte_length();
}

void* JSArrayBuffer::allocation_base() const {
  if (backing_store() == nullptr) {
    return nullptr;
  }
  // If this buffer is managed by the WasmMemoryTracker
  if (is_wasm_memory()) {
    const auto* data =
        GetIsolate()->wasm_engine()->memory_tracker()->FindAllocationData(
            backing_store());
    DCHECK_NOT_NULL(data);
    return data->allocation_base;
  }
  return backing_store();
}

bool JSArrayBuffer::is_wasm_memory() const {
  bool const is_wasm_memory = IsWasmMemoryBit::decode(bit_field());
  DCHECK_EQ(is_wasm_memory,
            GetIsolate()->wasm_engine()->memory_tracker()->IsWasmMemory(
                backing_store()));
  return is_wasm_memory;
}

void JSArrayBuffer::set_is_wasm_memory(bool is_wasm_memory) {
  set_bit_field(IsWasmMemoryBit::update(bit_field(), is_wasm_memory));
}

void JSArrayBuffer::clear_padding() {
  if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
    memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
           FIELD_SIZE(kOptionalPaddingOffset));
  }
}

void JSArrayBuffer::set_bit_field(uint32_t bits) {
  WRITE_UINT32_FIELD(this, kBitFieldOffset, bits);
}

uint32_t JSArrayBuffer::bit_field() const {
  return READ_UINT32_FIELD(this, kBitFieldOffset);
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
BIT_FIELD_ACCESSORS(JSArrayBuffer, bit_field, is_growable,
                    JSArrayBuffer::IsGrowableBit)

size_t JSArrayBufferView::byte_offset() const {
  return READ_UINTPTR_FIELD(this, kByteOffsetOffset);
}

void JSArrayBufferView::set_byte_offset(size_t value) {
  WRITE_UINTPTR_FIELD(this, kByteOffsetOffset, value);
}

size_t JSArrayBufferView::byte_length() const {
  return READ_UINTPTR_FIELD(this, kByteLengthOffset);
}

void JSArrayBufferView::set_byte_length(size_t value) {
  WRITE_UINTPTR_FIELD(this, kByteLengthOffset, value);
}

ACCESSORS(JSArrayBufferView, buffer, Object, kBufferOffset)

bool JSArrayBufferView::WasDetached() const {
  return JSArrayBuffer::cast(buffer())->was_detached();
}

Object JSTypedArray::length() const { return READ_FIELD(this, kLengthOffset); }

size_t JSTypedArray::length_value() const {
  double val = length()->Number();
  DCHECK_LE(val, kMaxSafeInteger);   // 2^53-1
  DCHECK_GE(val, -kMaxSafeInteger);  // -2^53+1
  DCHECK_LE(val, std::numeric_limits<size_t>::max());
  DCHECK_GE(val, std::numeric_limits<size_t>::min());
  return static_cast<size_t>(val);
}

void JSTypedArray::set_length(Object value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kLengthOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kLengthOffset, value, mode);
}

bool JSTypedArray::is_on_heap() const {
  DisallowHeapAllocation no_gc;
  // Checking that buffer()->backing_store() is not nullptr is not sufficient;
  // it will be nullptr when byte_length is 0 as well.
  FixedTypedArrayBase fta = FixedTypedArrayBase::cast(elements());
  return fta->base_pointer()->ptr() == fta.ptr();
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

#ifdef VERIFY_HEAP
ACCESSORS(JSTypedArray, raw_length, Object, kLengthOffset)
#endif

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_BUFFER_INL_H_
