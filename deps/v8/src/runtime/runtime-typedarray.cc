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
  RUNTIME_ASSERT(!source.is_identical_to(target));
  size_t start = 0, target_length = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *first, &start));
  RUNTIME_ASSERT(TryNumberToSize(isolate, *new_length, &target_length));
  RUNTIME_ASSERT(NumberToSize(isolate, target->byte_length()) >= target_length);

  if (target_length == 0) return isolate->heap()->undefined_value();

  size_t source_byte_length = NumberToSize(isolate, source->byte_length());
  RUNTIME_ASSERT(start <= source_byte_length);
  RUNTIME_ASSERT(source_byte_length - start >= target_length);
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
  RUNTIME_ASSERT(!array_buffer->is_shared());
  DCHECK(!array_buffer->is_external());
  void* backing_store = array_buffer->backing_store();
  size_t byte_length = NumberToSize(isolate, array_buffer->byte_length());
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

  RUNTIME_ASSERT(arrayId >= Runtime::ARRAY_ID_FIRST &&
                 arrayId <= Runtime::ARRAY_ID_LAST);

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;                            // Bogus initialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId, &array_type, &fixed_elements_kind,
                                &element_size);
  RUNTIME_ASSERT(holder->map()->elements_kind() == fixed_elements_kind);

  size_t byte_offset = 0;
  size_t byte_length = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_offset_object, &byte_offset));
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_length_object, &byte_length));

  if (maybe_buffer->IsJSArrayBuffer()) {
    Handle<JSArrayBuffer> buffer = Handle<JSArrayBuffer>::cast(maybe_buffer);
    size_t array_buffer_byte_length =
        NumberToSize(isolate, buffer->byte_length());
    RUNTIME_ASSERT(byte_offset <= array_buffer_byte_length);
    RUNTIME_ASSERT(array_buffer_byte_length - byte_offset >= byte_length);
  } else {
    RUNTIME_ASSERT(maybe_buffer->IsNull());
  }

  RUNTIME_ASSERT(byte_length % element_size == 0);
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

  if (!maybe_buffer->IsNull()) {
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

  RUNTIME_ASSERT(arrayId >= Runtime::ARRAY_ID_FIRST &&
                 arrayId <= Runtime::ARRAY_ID_LAST);

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;                            // Bogus initialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId, &array_type, &fixed_elements_kind,
                                &element_size);

  RUNTIME_ASSERT(holder->map()->elements_kind() == fixed_elements_kind);

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  size_t length = 0;
  if (source->IsJSTypedArray() &&
      JSTypedArray::cast(*source)->type() == array_type) {
    length_obj = handle(JSTypedArray::cast(*source)->length(), isolate);
    length = JSTypedArray::cast(*source)->length_value();
  } else {
    RUNTIME_ASSERT(TryNumberToSize(isolate, *length_obj, &length));
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
      size_t source_byte_offset =
          NumberToSize(isolate, typed_array->byte_offset());
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
BUFFER_VIEW_GETTER(DataView, Buffer, buffer)

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
  RUNTIME_ASSERT(TryNumberToSize(isolate, *offset_obj, &offset));
  size_t target_length = target->length_value();
  size_t source_length = source->length_value();
  size_t target_byte_length = NumberToSize(isolate, target->byte_length());
  size_t source_byte_length = NumberToSize(isolate, source->byte_length());
  if (offset > target_length || offset + source_length > target_length ||
      offset + source_length < offset) {  // overflow
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kTypedArraySetSourceTooLarge));
  }

  size_t target_offset = NumberToSize(isolate, target->byte_offset());
  size_t source_offset = NumberToSize(isolate, source->byte_offset());
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
                                    obj->type() != kExternalFloat64Array);
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


inline static bool NeedToFlipBytes(bool is_little_endian) {
#ifdef V8_TARGET_LITTLE_ENDIAN
  return !is_little_endian;
#else
  return is_little_endian;
#endif
}


template <int n>
inline void CopyBytes(uint8_t* target, uint8_t* source) {
  for (int i = 0; i < n; i++) {
    *(target++) = *(source++);
  }
}


template <int n>
inline void FlipBytes(uint8_t* target, uint8_t* source) {
  source = source + (n - 1);
  for (int i = 0; i < n; i++) {
    *(target++) = *(source--);
  }
}


template <typename T>
inline static bool DataViewGetValue(Isolate* isolate,
                                    Handle<JSDataView> data_view,
                                    Handle<Object> byte_offset_obj,
                                    bool is_little_endian, T* result) {
  size_t byte_offset = 0;
  if (!TryNumberToSize(isolate, *byte_offset_obj, &byte_offset)) {
    return false;
  }
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(data_view->buffer()));

  size_t data_view_byte_offset =
      NumberToSize(isolate, data_view->byte_offset());
  size_t data_view_byte_length =
      NumberToSize(isolate, data_view->byte_length());
  if (byte_offset + sizeof(T) > data_view_byte_length ||
      byte_offset + sizeof(T) < byte_offset) {  // overflow
    return false;
  }

  union Value {
    T data;
    uint8_t bytes[sizeof(T)];
  };

  Value value;
  size_t buffer_offset = data_view_byte_offset + byte_offset;
  DCHECK(NumberToSize(isolate, buffer->byte_length()) >=
         buffer_offset + sizeof(T));
  uint8_t* source =
      static_cast<uint8_t*>(buffer->backing_store()) + buffer_offset;
  if (NeedToFlipBytes(is_little_endian)) {
    FlipBytes<sizeof(T)>(value.bytes, source);
  } else {
    CopyBytes<sizeof(T)>(value.bytes, source);
  }
  *result = value.data;
  return true;
}


template <typename T>
static bool DataViewSetValue(Isolate* isolate, Handle<JSDataView> data_view,
                             Handle<Object> byte_offset_obj,
                             bool is_little_endian, T data) {
  size_t byte_offset = 0;
  if (!TryNumberToSize(isolate, *byte_offset_obj, &byte_offset)) {
    return false;
  }
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(data_view->buffer()));

  size_t data_view_byte_offset =
      NumberToSize(isolate, data_view->byte_offset());
  size_t data_view_byte_length =
      NumberToSize(isolate, data_view->byte_length());
  if (byte_offset + sizeof(T) > data_view_byte_length ||
      byte_offset + sizeof(T) < byte_offset) {  // overflow
    return false;
  }

  union Value {
    T data;
    uint8_t bytes[sizeof(T)];
  };

  Value value;
  value.data = data;
  size_t buffer_offset = data_view_byte_offset + byte_offset;
  DCHECK(NumberToSize(isolate, buffer->byte_length()) >=
         buffer_offset + sizeof(T));
  uint8_t* target =
      static_cast<uint8_t*>(buffer->backing_store()) + buffer_offset;
  if (NeedToFlipBytes(is_little_endian)) {
    FlipBytes<sizeof(T)>(target, value.bytes);
  } else {
    CopyBytes<sizeof(T)>(target, value.bytes);
  }
  return true;
}


#define DATA_VIEW_GETTER(TypeName, Type, Converter)                        \
  RUNTIME_FUNCTION(Runtime_DataViewGet##TypeName) {                        \
    HandleScope scope(isolate);                                            \
    DCHECK(args.length() == 3);                                            \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                     \
    CONVERT_NUMBER_ARG_HANDLE_CHECKED(offset, 1);                          \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 2);                      \
    Type result;                                                           \
    if (DataViewGetValue(isolate, holder, offset, is_little_endian,        \
                         &result)) {                                       \
      return *isolate->factory()->Converter(result);                       \
    } else {                                                               \
      THROW_NEW_ERROR_RETURN_FAILURE(                                      \
          isolate,                                                         \
          NewRangeError(MessageTemplate::kInvalidDataViewAccessorOffset)); \
    }                                                                      \
  }

DATA_VIEW_GETTER(Uint8, uint8_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int8, int8_t, NewNumberFromInt)
DATA_VIEW_GETTER(Uint16, uint16_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int16, int16_t, NewNumberFromInt)
DATA_VIEW_GETTER(Uint32, uint32_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int32, int32_t, NewNumberFromInt)
DATA_VIEW_GETTER(Float32, float, NewNumber)
DATA_VIEW_GETTER(Float64, double, NewNumber)

#undef DATA_VIEW_GETTER


template <typename T>
static T DataViewConvertValue(double value);


template <>
int8_t DataViewConvertValue<int8_t>(double value) {
  return static_cast<int8_t>(DoubleToInt32(value));
}


template <>
int16_t DataViewConvertValue<int16_t>(double value) {
  return static_cast<int16_t>(DoubleToInt32(value));
}


template <>
int32_t DataViewConvertValue<int32_t>(double value) {
  return DoubleToInt32(value);
}


template <>
uint8_t DataViewConvertValue<uint8_t>(double value) {
  return static_cast<uint8_t>(DoubleToUint32(value));
}


template <>
uint16_t DataViewConvertValue<uint16_t>(double value) {
  return static_cast<uint16_t>(DoubleToUint32(value));
}


template <>
uint32_t DataViewConvertValue<uint32_t>(double value) {
  return DoubleToUint32(value);
}


template <>
float DataViewConvertValue<float>(double value) {
  return static_cast<float>(value);
}


template <>
double DataViewConvertValue<double>(double value) {
  return value;
}


#define DATA_VIEW_SETTER(TypeName, Type)                                   \
  RUNTIME_FUNCTION(Runtime_DataViewSet##TypeName) {                        \
    HandleScope scope(isolate);                                            \
    DCHECK(args.length() == 4);                                            \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                     \
    CONVERT_NUMBER_ARG_HANDLE_CHECKED(offset, 1);                          \
    CONVERT_NUMBER_ARG_HANDLE_CHECKED(value, 2);                           \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 3);                      \
    Type v = DataViewConvertValue<Type>(value->Number());                  \
    if (DataViewSetValue(isolate, holder, offset, is_little_endian, v)) {  \
      return isolate->heap()->undefined_value();                           \
    } else {                                                               \
      THROW_NEW_ERROR_RETURN_FAILURE(                                      \
          isolate,                                                         \
          NewRangeError(MessageTemplate::kInvalidDataViewAccessorOffset)); \
    }                                                                      \
  }

DATA_VIEW_SETTER(Uint8, uint8_t)
DATA_VIEW_SETTER(Int8, int8_t)
DATA_VIEW_SETTER(Uint16, uint16_t)
DATA_VIEW_SETTER(Int16, int16_t)
DATA_VIEW_SETTER(Uint32, uint32_t)
DATA_VIEW_SETTER(Int32, int32_t)
DATA_VIEW_SETTER(Float32, float)
DATA_VIEW_SETTER(Float64, double)

#undef DATA_VIEW_SETTER
}  // namespace internal
}  // namespace v8
