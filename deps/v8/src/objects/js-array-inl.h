// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_INL_H_
#define V8_OBJECTS_JS_ARRAY_INL_H_

#include "src/objects/js-array.h"
#include "src/wasm/wasm-engine.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TYPE_CHECKER(JSArray, JS_ARRAY_TYPE)
TYPE_CHECKER(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)
TYPE_CHECKER(JSTypedArray, JS_TYPED_ARRAY_TYPE)

CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSArrayBuffer)
CAST_ACCESSOR(JSArrayBufferView)
CAST_ACCESSOR(JSArrayIterator)
CAST_ACCESSOR(JSTypedArray)

ACCESSORS(JSArray, length, Object, kLengthOffset)

template <>
inline bool Is<JSArray>(Object* obj) {
  return obj->IsJSArray();
}

void JSArray::set_length(Smi* length) {
  // Don't need a write barrier for a Smi.
  set_length(static_cast<Object*>(length), SKIP_WRITE_BARRIER);
}

bool JSArray::SetLengthWouldNormalize(Heap* heap, uint32_t new_length) {
  return new_length > kMaxFastArrayLength;
}

bool JSArray::AllowsSetLength() {
  bool result = elements()->IsFixedArray() || elements()->IsFixedDoubleArray();
  DCHECK(result == !HasFixedTypedArrayElements());
  return result;
}

void JSArray::SetContent(Handle<JSArray> array,
                         Handle<FixedArrayBase> storage) {
  EnsureCanContainElements(array, storage, storage->length(),
                           ALLOW_COPIED_DOUBLE_ELEMENTS);

  DCHECK((storage->map() == array->GetHeap()->fixed_double_array_map() &&
          IsDoubleElementsKind(array->GetElementsKind())) ||
         ((storage->map() != array->GetHeap()->fixed_double_array_map()) &&
          (IsObjectElementsKind(array->GetElementsKind()) ||
           (IsSmiElementsKind(array->GetElementsKind()) &&
            Handle<FixedArray>::cast(storage)->ContainsOnlySmisOrHoles()))));
  array->set_elements(*storage);
  array->set_length(Smi::FromInt(storage->length()));
}

bool JSArray::HasArrayPrototype(Isolate* isolate) {
  return map()->prototype() == *isolate->initial_array_prototype();
}

void* JSArrayBuffer::backing_store() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kBackingStoreOffset);
  return reinterpret_cast<void*>(ptr);
}

void JSArrayBuffer::set_backing_store(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kBackingStoreOffset, ptr);
}

ACCESSORS(JSArrayBuffer, byte_length, Object, kByteLengthOffset)

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
  return byte_length()->Number();
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

ArrayBuffer::Allocator::AllocationMode JSArrayBuffer::allocation_mode() const {
  using AllocationMode = ArrayBuffer::Allocator::AllocationMode;
  return is_wasm_memory() ? AllocationMode::kReservation
                          : AllocationMode::kNormal;
}

bool JSArrayBuffer::is_wasm_memory() const {
  bool const is_wasm_memory = IsWasmMemory::decode(bit_field());
  DCHECK_EQ(is_wasm_memory,
            GetIsolate()->wasm_engine()->memory_tracker()->IsWasmMemory(
                backing_store()));
  return is_wasm_memory;
}

void JSArrayBuffer::set_bit_field(uint32_t bits) {
  if (kInt32Size != kPointerSize) {
#if V8_TARGET_LITTLE_ENDIAN
    WRITE_UINT32_FIELD(this, kBitFieldSlot + kInt32Size, 0);
#else
    WRITE_UINT32_FIELD(this, kBitFieldSlot, 0);
#endif
  }
  WRITE_UINT32_FIELD(this, kBitFieldOffset, bits);
}

uint32_t JSArrayBuffer::bit_field() const {
  return READ_UINT32_FIELD(this, kBitFieldOffset);
}

bool JSArrayBuffer::is_external() { return IsExternal::decode(bit_field()); }

void JSArrayBuffer::set_is_external(bool value) {
  set_bit_field(IsExternal::update(bit_field(), value));
}

bool JSArrayBuffer::is_neuterable() {
  return IsNeuterable::decode(bit_field());
}

void JSArrayBuffer::set_is_neuterable(bool value) {
  set_bit_field(IsNeuterable::update(bit_field(), value));
}

bool JSArrayBuffer::was_neutered() { return WasNeutered::decode(bit_field()); }

void JSArrayBuffer::set_was_neutered(bool value) {
  set_bit_field(WasNeutered::update(bit_field(), value));
}

bool JSArrayBuffer::is_shared() { return IsShared::decode(bit_field()); }

void JSArrayBuffer::set_is_shared(bool value) {
  set_bit_field(IsShared::update(bit_field(), value));
}

bool JSArrayBuffer::is_growable() { return IsGrowable::decode(bit_field()); }

void JSArrayBuffer::set_is_growable(bool value) {
  set_bit_field(IsGrowable::update(bit_field(), value));
}

Object* JSArrayBufferView::byte_offset() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kByteOffsetOffset));
}

void JSArrayBufferView::set_byte_offset(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kByteOffsetOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kByteOffsetOffset, value, mode);
}

Object* JSArrayBufferView::byte_length() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kByteLengthOffset));
}

void JSArrayBufferView::set_byte_length(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kByteLengthOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kByteLengthOffset, value, mode);
}

ACCESSORS(JSArrayBufferView, buffer, Object, kBufferOffset)
#ifdef VERIFY_HEAP
ACCESSORS(JSArrayBufferView, raw_byte_offset, Object, kByteOffsetOffset)
ACCESSORS(JSArrayBufferView, raw_byte_length, Object, kByteLengthOffset)
#endif

bool JSArrayBufferView::WasNeutered() const {
  return JSArrayBuffer::cast(buffer())->was_neutered();
}

Object* JSTypedArray::length() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kLengthOffset));
}

uint32_t JSTypedArray::length_value() const {
  if (WasNeutered()) return 0;
  uint32_t index = 0;
  CHECK(Object::cast(READ_FIELD(this, kLengthOffset))->ToArrayLength(&index));
  return index;
}

void JSTypedArray::set_length(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kLengthOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kLengthOffset, value, mode);
}

bool JSTypedArray::is_on_heap() const {
  DisallowHeapAllocation no_gc;
  // Checking that buffer()->backing_store() is not nullptr is not sufficient;
  // it will be nullptr when byte_length is 0 as well.
  FixedTypedArrayBase* fta(FixedTypedArrayBase::cast(elements()));
  return fta->base_pointer() == fta;
}

// static
MaybeHandle<JSTypedArray> JSTypedArray::Validate(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 const char* method_name) {
  if (V8_UNLIKELY(!receiver->IsJSTypedArray())) {
    const MessageTemplate::Template message = MessageTemplate::kNotTypedArray;
    THROW_NEW_ERROR(isolate, NewTypeError(message), JSTypedArray);
  }

  Handle<JSTypedArray> array = Handle<JSTypedArray>::cast(receiver);
  if (V8_UNLIKELY(array->WasNeutered())) {
    const MessageTemplate::Template message =
        MessageTemplate::kDetachedOperation;
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

ACCESSORS(JSArrayIterator, iterated_object, Object, kIteratedObjectOffset)
ACCESSORS(JSArrayIterator, next_index, Object, kNextIndexOffset)

IterationKind JSArrayIterator::kind() const {
  return static_cast<IterationKind>(
      Smi::cast(READ_FIELD(this, kKindOffset))->value());
}

void JSArrayIterator::set_kind(IterationKind kind) {
  WRITE_FIELD(this, kKindOffset, Smi::FromInt(static_cast<int>(kind)));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_INL_H_
