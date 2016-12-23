// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/factory.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ArrayBufferGetByteLength) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSArrayBuffer, holder, 0);
  return holder->byte_length();
}


RUNTIME_FUNCTION(Runtime_ArrayBufferSliceImpl) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, source, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, target, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(first, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(new_length, 3);

  if (source->was_neutered() || target->was_neutered()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "ArrayBuffer.prototype.slice")));
  }

  CHECK(!source.is_identical_to(target));
  size_t start = 0, target_length = 0;
  CHECK(TryNumberToSize(*first, &start));
  CHECK(TryNumberToSize(*new_length, &target_length));
  CHECK(NumberToSize(target->byte_length()) >= target_length);

  if (target_length == 0) return isolate->heap()->undefined_value();

  size_t source_byte_length = NumberToSize(source->byte_length());
  CHECK(start <= source_byte_length);
  CHECK(source_byte_length - start >= target_length);
  uint8_t* source_data = reinterpret_cast<uint8_t*>(source->backing_store());
  uint8_t* target_data = reinterpret_cast<uint8_t*>(target->backing_store());
  CopyBytes(target_data, source_data + start, target_length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ArrayBufferNeuter) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, array_buffer, 0);
  if (array_buffer->backing_store() == NULL) {
    CHECK(Smi::FromInt(0) == array_buffer->byte_length());
    return isolate->heap()->undefined_value();
  }
  // Shared array buffers should never be neutered.
  CHECK(!array_buffer->is_shared());
  DCHECK(!array_buffer->is_external());
  void* backing_store = array_buffer->backing_store();
  size_t byte_length = NumberToSize(array_buffer->byte_length());
  array_buffer->set_is_external(true);
  isolate->heap()->UnregisterArrayBuffer(*array_buffer);
  array_buffer->Neuter();
  isolate->array_buffer_allocator()->Free(backing_store, byte_length);
  return isolate->heap()->undefined_value();
}


void Runtime::ArrayIdToTypeAndSize(int arrayId, ExternalArrayType* array_type,
                                   ElementsKind* fixed_elements_kind,
                                   size_t* element_size) {
  switch (arrayId) {
#define ARRAY_ID_CASE(Type, type, TYPE, ctype, size)      \
  case ARRAY_ID_##TYPE:                                   \
    *array_type = kExternal##Type##Array;                 \
    *fixed_elements_kind = TYPE##_ELEMENTS;               \
    *element_size = size;                                 \
    break;

    TYPED_ARRAYS(ARRAY_ID_CASE)
#undef ARRAY_ID_CASE

    default:
      UNREACHABLE();
  }
}


RUNTIME_FUNCTION(Runtime_TypedArrayInitialize) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 6);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  CONVERT_SMI_ARG_CHECKED(arrayId, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, maybe_buffer, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_offset_object, 3);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_length_object, 4);
  CONVERT_BOOLEAN_ARG_CHECKED(initialize, 5);

  CHECK(arrayId >= Runtime::ARRAY_ID_FIRST &&
        arrayId <= Runtime::ARRAY_ID_LAST);

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;                            // Bogus initialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId, &array_type, &fixed_elements_kind,
                                &element_size);
  CHECK(holder->map()->elements_kind() == fixed_elements_kind);

  size_t byte_offset = 0;
  size_t byte_length = 0;
  CHECK(TryNumberToSize(*byte_offset_object, &byte_offset));
  CHECK(TryNumberToSize(*byte_length_object, &byte_length));

  if (maybe_buffer->IsJSArrayBuffer()) {
    Handle<JSArrayBuffer> buffer = Handle<JSArrayBuffer>::cast(maybe_buffer);
    size_t array_buffer_byte_length = NumberToSize(buffer->byte_length());
    CHECK(byte_offset <= array_buffer_byte_length);
    CHECK(array_buffer_byte_length - byte_offset >= byte_length);
  } else {
    CHECK(maybe_buffer->IsNull(isolate));
  }

  CHECK(byte_length % element_size == 0);
  size_t length = byte_length / element_size;

  if (length > static_cast<unsigned>(Smi::kMaxValue)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTypedArrayLength));
  }

  // All checks are done, now we can modify objects.

  DCHECK_EQ(v8::ArrayBufferView::kInternalFieldCount,
            holder->GetInternalFieldCount());
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    holder->SetInternalField(i, Smi::FromInt(0));
  }
  Handle<Object> length_obj = isolate->factory()->NewNumberFromSize(length);
  holder->set_length(*length_obj);
  holder->set_byte_offset(*byte_offset_object);
  holder->set_byte_length(*byte_length_object);

  if (!maybe_buffer->IsNull(isolate)) {
    Handle<JSArrayBuffer> buffer = Handle<JSArrayBuffer>::cast(maybe_buffer);
    holder->set_buffer(*buffer);

    Handle<FixedTypedArrayBase> elements =
        isolate->factory()->NewFixedTypedArrayWithExternalPointer(
            static_cast<int>(length), array_type,
            static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
    holder->set_elements(*elements);
  } else {
    Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
    JSArrayBuffer::Setup(buffer, isolate, true, NULL, byte_length,
                         SharedFlag::kNotShared);
    holder->set_buffer(*buffer);
    Handle<FixedTypedArrayBase> elements =
        isolate->factory()->NewFixedTypedArray(static_cast<int>(length),
                                               array_type, initialize);
    holder->set_elements(*elements);
  }
  return isolate->heap()->undefined_value();
}


// Initializes a typed array from an array-like object.
// If an array-like object happens to be a typed array of the same type,
// initializes backing store using memove.
//
// Returns true if backing store was initialized or false otherwise.
RUNTIME_FUNCTION(Runtime_TypedArrayInitializeFromArrayLike) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  CONVERT_SMI_ARG_CHECKED(arrayId, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(length_obj, 3);

  CHECK(arrayId >= Runtime::ARRAY_ID_FIRST &&
        arrayId <= Runtime::ARRAY_ID_LAST);

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;                            // Bogus initialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId, &array_type, &fixed_elements_kind,
                                &element_size);

  CHECK(holder->map()->elements_kind() == fixed_elements_kind);

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  size_t length = 0;
  if (source->IsJSTypedArray() &&
      JSTypedArray::cast(*source)->type() == array_type) {
    length = JSTypedArray::cast(*source)->length_value();
  } else {
    CHECK(TryNumberToSize(*length_obj, &length));
  }

  if ((length > static_cast<unsigned>(Smi::kMaxValue)) ||
      (length > (kMaxInt / element_size))) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTypedArrayLength));
  }
  size_t byte_length = length * element_size;

  DCHECK_EQ(v8::ArrayBufferView::kInternalFieldCount,
            holder->GetInternalFieldCount());
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    holder->SetInternalField(i, Smi::FromInt(0));
  }

  // NOTE: not initializing backing store.
  // We assume that the caller of this function will initialize holder
  // with the loop
  //      for(i = 0; i < length; i++) { holder[i] = source[i]; }
  // We assume that the caller of this function is always a typed array
  // constructor.
  // If source is a typed array, this loop will always run to completion,
  // so we are sure that the backing store will be initialized.
  // Otherwise, the indexing operation might throw, so the loop will not
  // run to completion and the typed array might remain partly initialized.
  // However we further assume that the caller of this function is a typed array
  // constructor, and the exception will propagate out of the constructor,
  // therefore uninitialized memory will not be accessible by a user program.
  //
  // TODO(dslomov): revise this once we support subclassing.

  if (!JSArrayBuffer::SetupAllocatingData(buffer, isolate, byte_length,
                                          false)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }

  holder->set_buffer(*buffer);
  holder->set_byte_offset(Smi::FromInt(0));
  Handle<Object> byte_length_obj(
      isolate->factory()->NewNumberFromSize(byte_length));
  holder->set_byte_length(*byte_length_obj);
  length_obj = isolate->factory()->NewNumberFromSize(length);
  holder->set_length(*length_obj);

  Handle<FixedTypedArrayBase> elements =
      isolate->factory()->NewFixedTypedArrayWithExternalPointer(
          static_cast<int>(length), array_type,
          static_cast<uint8_t*>(buffer->backing_store()));
  holder->set_elements(*elements);

  if (source->IsJSTypedArray()) {
    Handle<JSTypedArray> typed_array(JSTypedArray::cast(*source));

    if (typed_array->type() == holder->type()) {
      uint8_t* backing_store =
          static_cast<uint8_t*>(typed_array->GetBuffer()->backing_store());
      size_t source_byte_offset = NumberToSize(typed_array->byte_offset());
      memcpy(buffer->backing_store(), backing_store + source_byte_offset,
             byte_length);
      return isolate->heap()->true_value();
    }
  }

  return isolate->heap()->false_value();
}


#define BUFFER_VIEW_GETTER(Type, getter, accessor)   \
  RUNTIME_FUNCTION(Runtime_##Type##Get##getter) {    \
    HandleScope scope(isolate);                      \
    DCHECK_EQ(1, args.length());                     \
    CONVERT_ARG_HANDLE_CHECKED(JS##Type, holder, 0); \
    return holder->accessor();                       \
  }

BUFFER_VIEW_GETTER(ArrayBufferView, ByteLength, byte_length)
BUFFER_VIEW_GETTER(ArrayBufferView, ByteOffset, byte_offset)
BUFFER_VIEW_GETTER(TypedArray, Length, length)

#undef BUFFER_VIEW_GETTER

RUNTIME_FUNCTION(Runtime_TypedArrayGetBuffer) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  return *holder->GetBuffer();
}


// Return codes for Runtime_TypedArraySetFastCases.
// Should be synchronized with typedarray.js natives.
enum TypedArraySetResultCodes {
  // Set from typed array of the same type.
  // This is processed by TypedArraySetFastCases
  TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE = 0,
  // Set from typed array of the different type, overlapping in memory.
  TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING = 1,
  // Set from typed array of the different type, non-overlapping.
  TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING = 2,
  // Set from non-typed array.
  TYPED_ARRAY_SET_NON_TYPED_ARRAY = 3
};


RUNTIME_FUNCTION(Runtime_TypedArraySetFastCases) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  if (!args[0]->IsJSTypedArray()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotTypedArray));
  }

  if (!args[1]->IsJSTypedArray())
    return Smi::FromInt(TYPED_ARRAY_SET_NON_TYPED_ARRAY);

  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, target_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, source_obj, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(offset_obj, 2);

  Handle<JSTypedArray> target(JSTypedArray::cast(*target_obj));
  Handle<JSTypedArray> source(JSTypedArray::cast(*source_obj));
  size_t offset = 0;
  CHECK(TryNumberToSize(*offset_obj, &offset));
  size_t target_length = target->length_value();
  size_t source_length = source->length_value();
  size_t target_byte_length = NumberToSize(target->byte_length());
  size_t source_byte_length = NumberToSize(source->byte_length());
  if (offset > target_length || offset + source_length > target_length ||
      offset + source_length < offset) {  // overflow
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kTypedArraySetSourceTooLarge));
  }

  size_t target_offset = NumberToSize(target->byte_offset());
  size_t source_offset = NumberToSize(source->byte_offset());
  uint8_t* target_base =
      static_cast<uint8_t*>(target->GetBuffer()->backing_store()) +
      target_offset;
  uint8_t* source_base =
      static_cast<uint8_t*>(source->GetBuffer()->backing_store()) +
      source_offset;

  // Typed arrays of the same type: use memmove.
  if (target->type() == source->type()) {
    memmove(target_base + offset * target->element_size(), source_base,
            source_byte_length);
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE);
  }

  // Typed arrays of different types over the same backing store
  if ((source_base <= target_base &&
       source_base + source_byte_length > target_base) ||
      (target_base <= source_base &&
       target_base + target_byte_length > source_base)) {
    // We do not support overlapping ArrayBuffers
    DCHECK(target->GetBuffer()->backing_store() ==
           source->GetBuffer()->backing_store());
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING);
  } else {  // Non-overlapping typed arrays
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING);
  }
}


RUNTIME_FUNCTION(Runtime_TypedArrayMaxSizeInHeap) {
  DCHECK(args.length() == 0);
  DCHECK_OBJECT_SIZE(FLAG_typed_array_max_size_in_heap +
                     FixedTypedArrayBase::kDataOffset);
  return Smi::FromInt(FLAG_typed_array_max_size_in_heap);
}


RUNTIME_FUNCTION(Runtime_IsTypedArray) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->ToBoolean(args[0]->IsJSTypedArray());
}


RUNTIME_FUNCTION(Runtime_IsSharedTypedArray) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->heap()->ToBoolean(
      args[0]->IsJSTypedArray() &&
      JSTypedArray::cast(args[0])->GetBuffer()->is_shared());
}


RUNTIME_FUNCTION(Runtime_IsSharedIntegerTypedArray) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSTypedArray()) {
    return isolate->heap()->false_value();
  }

  Handle<JSTypedArray> obj(JSTypedArray::cast(args[0]));
  return isolate->heap()->ToBoolean(obj->GetBuffer()->is_shared() &&
                                    obj->type() != kExternalFloat32Array &&
                                    obj->type() != kExternalFloat64Array &&
                                    obj->type() != kExternalUint8ClampedArray);
}


RUNTIME_FUNCTION(Runtime_IsSharedInteger32TypedArray) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (!args[0]->IsJSTypedArray()) {
    return isolate->heap()->false_value();
  }

  Handle<JSTypedArray> obj(JSTypedArray::cast(args[0]));
  return isolate->heap()->ToBoolean(obj->GetBuffer()->is_shared() &&
                                    obj->type() == kExternalInt32Array);
}

}  // namespace internal
}  // namespace v8
