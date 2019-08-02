// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/execution/message-template.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/elements.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_ArrayBufferDetach) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> argument = args.at(0);
  // This runtime function is exposed in ClusterFuzz and as such has to
  // support arbitrary arguments.
  if (!argument->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotTypedArray));
  }
  Handle<JSArrayBuffer> array_buffer = Handle<JSArrayBuffer>::cast(argument);
  if (!array_buffer->is_detachable()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  if (array_buffer->backing_store() == nullptr) {
    CHECK_EQ(0, array_buffer->byte_length());
    return ReadOnlyRoots(isolate).undefined_value();
  }
  // Shared array buffers should never be detached.
  CHECK(!array_buffer->is_shared());
  DCHECK(!array_buffer->is_external());
  void* backing_store = array_buffer->backing_store();
  size_t byte_length = array_buffer->byte_length();
  array_buffer->set_is_external(true);
  isolate->heap()->UnregisterArrayBuffer(*array_buffer);
  array_buffer->Detach();
  isolate->array_buffer_allocator()->Free(backing_store, byte_length);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_TypedArrayCopyElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(length_obj, 2);

  size_t length;
  CHECK(TryNumberToSize(*length_obj, &length));

  ElementsAccessor* accessor = target->GetElementsAccessor();
  return accessor->CopyElements(source, target, length);
}

RUNTIME_FUNCTION(Runtime_TypedArrayGetBuffer) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  return *holder->GetBuffer();
}


namespace {

template <typename T>
bool CompareNum(T x, T y) {
  if (x < y) {
    return true;
  } else if (x > y) {
    return false;
  } else if (!std::is_integral<T>::value) {
    double _x = x, _y = y;
    if (x == 0 && x == y) {
      /* -0.0 is less than +0.0 */
      return std::signbit(_x) && !std::signbit(_y);
    } else if (!std::isnan(_x) && std::isnan(_y)) {
      /* number is less than NaN */
      return true;
    }
  }
  return false;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_TypedArraySortFast) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  // Validation is handled in the Torque builtin.
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, array, 0);
  DCHECK(!array->WasDetached());

  size_t length = array->length();
  if (length <= 1) return *array;

  // In case of a SAB, the data is copied into temporary memory, as
  // std::sort might crash in case the underlying data is concurrently
  // modified while sorting.
  CHECK(array->buffer().IsJSArrayBuffer());
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(array->buffer()), isolate);
  const bool copy_data = buffer->is_shared();

  Handle<ByteArray> array_copy;
  if (copy_data) {
    const size_t bytes = array->byte_length();
    // TODO(szuend): Re-check this approach once support for larger typed
    //               arrays has landed.
    CHECK_LE(bytes, INT_MAX);
    array_copy = isolate->factory()->NewByteArray(static_cast<int>(bytes));
    std::memcpy(static_cast<void*>(array_copy->GetDataStartAddress()),
                static_cast<void*>(array->DataPtr()), bytes);
  }

  DisallowHeapAllocation no_gc;

  switch (array->type()) {
#define TYPED_ARRAY_SORT(Type, type, TYPE, ctype)                          \
  case kExternal##Type##Array: {                                           \
    ctype* data =                                                          \
        copy_data                                                          \
            ? reinterpret_cast<ctype*>(array_copy->GetDataStartAddress())  \
            : static_cast<ctype*>(array->DataPtr());                       \
    if (kExternal##Type##Array == kExternalFloat64Array ||                 \
        kExternal##Type##Array == kExternalFloat32Array) {                 \
      if (COMPRESS_POINTERS_BOOL && alignof(ctype) > kTaggedSize) {        \
        /* TODO(ishell, v8:8875): See UnalignedSlot<T> for details. */     \
        std::sort(UnalignedSlot<ctype>(data),                              \
                  UnalignedSlot<ctype>(data + length), CompareNum<ctype>); \
      } else {                                                             \
        std::sort(data, data + length, CompareNum<ctype>);                 \
      }                                                                    \
    } else {                                                               \
      if (COMPRESS_POINTERS_BOOL && alignof(ctype) > kTaggedSize) {        \
        /* TODO(ishell, v8:8875): See UnalignedSlot<T> for details. */     \
        std::sort(UnalignedSlot<ctype>(data),                              \
                  UnalignedSlot<ctype>(data + length));                    \
      } else {                                                             \
        std::sort(data, data + length);                                    \
      }                                                                    \
    }                                                                      \
    break;                                                                 \
  }

    TYPED_ARRAYS(TYPED_ARRAY_SORT)
#undef TYPED_ARRAY_SORT
  }

  if (copy_data) {
    DCHECK(!array_copy.is_null());
    const size_t bytes = array->byte_length();
    std::memcpy(static_cast<void*>(array->DataPtr()),
                static_cast<void*>(array_copy->GetDataStartAddress()), bytes);
  }

  return *array;
}

// 22.2.3.23 %TypedArray%.prototype.set ( overloaded [ , offset ] )
RUNTIME_FUNCTION(Runtime_TypedArraySet) {
  HandleScope scope(isolate);
  Handle<JSTypedArray> target = args.at<JSTypedArray>(0);
  Handle<Object> obj = args.at(1);
  Handle<Smi> offset = args.at<Smi>(2);

  DCHECK(!target->WasDetached());  // Checked in TypedArrayPrototypeSet.
  DCHECK(!obj->IsJSTypedArray());  // Should be handled by CSA.
  DCHECK_LE(0, offset->value());

  const uint32_t uint_offset = static_cast<uint32_t>(offset->value());

  if (obj->IsNumber()) {
    // For number as a first argument, throw TypeError
    // instead of silently ignoring the call, so that
    // users know they did something wrong.
    // (Consistent with Firefox and Blink/WebKit)
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidArgument));
  }

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, obj,
                                     Object::ToObject(isolate, obj));

  Handle<Object> len;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, len,
      Object::GetProperty(isolate, obj, isolate->factory()->length_string()));
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len,
                                     Object::ToLength(isolate, len));

  if (uint_offset + len->Number() > target->length()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kTypedArraySetSourceTooLarge));
  }

  uint32_t int_l;
  CHECK(DoubleToUint32IfEqualToSelf(len->Number(), &int_l));

  Handle<JSReceiver> source = Handle<JSReceiver>::cast(obj);
  ElementsAccessor* accessor = target->GetElementsAccessor();
  return accessor->CopyElements(source, target, int_l, uint_offset);
}

}  // namespace internal
}  // namespace v8
