// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/message-template.h"
#include "src/execution/arguments-inl.h"
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
  array_buffer->Detach();
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
  return accessor->CopyElements(source, target, length, 0);
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
  DCHECK_LT(1, length);

  // In case of a SAB, the data is copied into temporary memory, as
  // std::sort might crash in case the underlying data is concurrently
  // modified while sorting.
  CHECK(array->buffer().IsJSArrayBuffer());
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(array->buffer()), isolate);
  const bool copy_data = buffer->is_shared();

  Handle<ByteArray> array_copy;
  std::vector<uint8_t> offheap_copy;
  void* data_copy_ptr = nullptr;
  if (copy_data) {
    const size_t bytes = array->byte_length();
    if (bytes <= static_cast<unsigned>(
                     ByteArray::LengthFor(kMaxRegularHeapObjectSize))) {
      array_copy = isolate->factory()->NewByteArray(static_cast<int>(bytes));
      data_copy_ptr = array_copy->GetDataStartAddress();
    } else {
      // Allocate copy in C++ heap.
      offheap_copy.resize(bytes);
      data_copy_ptr = &offheap_copy[0];
    }
    std::memcpy(data_copy_ptr, static_cast<void*>(array->DataPtr()), bytes);
  }

  DisallowHeapAllocation no_gc;

  switch (array->type()) {
#define TYPED_ARRAY_SORT(Type, type, TYPE, ctype)                          \
  case kExternal##Type##Array: {                                           \
    ctype* data = copy_data ? reinterpret_cast<ctype*>(data_copy_ptr)      \
                            : static_cast<ctype*>(array->DataPtr());       \
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
    DCHECK_NOT_NULL(data_copy_ptr);
    DCHECK_NE(array_copy.is_null(), offheap_copy.empty());
    const size_t bytes = array->byte_length();
    std::memcpy(static_cast<void*>(array->DataPtr()), data_copy_ptr, bytes);
  }

  return *array;
}

RUNTIME_FUNCTION(Runtime_TypedArraySet) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(length_obj, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(offset_obj, 3);

  size_t length;
  CHECK(TryNumberToSize(*length_obj, &length));

  size_t offset;
  CHECK(TryNumberToSize(*offset_obj, &offset));

  ElementsAccessor* accessor = target->GetElementsAccessor();
  return accessor->CopyElements(source, target, length, offset);
}

}  // namespace internal
}  // namespace v8
