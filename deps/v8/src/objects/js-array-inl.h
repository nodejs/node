// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_INL_H_
#define V8_OBJECTS_JS_ARRAY_INL_H_

#include "src/objects/js-array.h"

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

void* JSArrayBuffer::allocation_base() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kAllocationBaseOffset);
  return reinterpret_cast<void*>(ptr);
}

void JSArrayBuffer::set_allocation_base(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kAllocationBaseOffset, ptr);
}

size_t JSArrayBuffer::allocation_length() const {
  return *reinterpret_cast<const size_t*>(
      FIELD_ADDR_CONST(this, kAllocationLengthOffset));
}

void JSArrayBuffer::set_allocation_length(size_t value) {
  (*reinterpret_cast<size_t*>(FIELD_ADDR(this, kAllocationLengthOffset))) =
      value;
}

ArrayBuffer::Allocator::AllocationMode JSArrayBuffer::allocation_mode() const {
  using AllocationMode = ArrayBuffer::Allocator::AllocationMode;
  return has_guard_region() ? AllocationMode::kReservation
                            : AllocationMode::kNormal;
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

bool JSArrayBuffer::has_guard_region() const {
  return HasGuardRegion::decode(bit_field());
}

void JSArrayBuffer::set_has_guard_region(bool value) {
  set_bit_field(HasGuardRegion::update(bit_field(), value));
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

bool JSTypedArray::HasJSTypedArrayPrototype(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  Object* proto = map()->prototype();
  if (!proto->IsJSObject()) return false;

  JSObject* proto_obj = JSObject::cast(proto);
  return proto_obj->map()->prototype() == *isolate->typed_array_prototype();
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

// static
Handle<JSFunction> JSTypedArray::DefaultConstructor(
    Isolate* isolate, Handle<JSTypedArray> exemplar) {
  Handle<JSFunction> default_ctor = isolate->uint8_array_fun();
  switch (exemplar->type()) {
#define TYPED_ARRAY_CTOR(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array: {                        \
    default_ctor = isolate->type##_array_fun();         \
    break;                                              \
  }

    TYPED_ARRAYS(TYPED_ARRAY_CTOR)
#undef TYPED_ARRAY_CTOR
    default:
      UNREACHABLE();
  }

  return default_ctor;
}

#ifdef VERIFY_HEAP
ACCESSORS(JSTypedArray, raw_length, Object, kLengthOffset)
#endif

ACCESSORS(JSArrayIterator, object, Object, kIteratedObjectOffset)
ACCESSORS(JSArrayIterator, index, Object, kNextIndexOffset)
ACCESSORS(JSArrayIterator, object_map, Object, kIteratedObjectMapOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_INL_H_
